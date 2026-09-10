#include "bk_private/bk_init.h"

#include <components/log.h>
#include <components/bk_uid.h>
#include <media_service.h>
#include <mbedtls/md5.h>
#include <os/os.h>
#include <api/aosl.h>

#include <mybot/mybot.h>
#include <mybot/mybot_version.h>

#include <mybot_bk7259_platform.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TAG "mybot"
#define MYBOT_POLL_INTERVAL_MS 100
#define MYBOT_APP_TASK_STACK_SIZE (16 * 1024)

/* The SDK generates this in the project ELF source as __DATE__ " " __TIME__,
 * so it carries the build time of the image that is actually running. */
extern volatile const char build_version[];

static mybot_config_t s_config;

static const char *mybot_state_name(mybot_state_t state)
{
    switch (state) {
    case MYBOT_STATE_STOPPED:
        return "STOPPED";
    case MYBOT_STATE_WIFI_PROVISIONING:
        return "WIFI_PROVISIONING";
    case MYBOT_STATE_STARTING_SERVICES:
        return "STARTING_SERVICES";
    case MYBOT_STATE_READY:
        return "READY";
    case MYBOT_STATE_WIFI_DISCONNECTED:
        return "WIFI_DISCONNECTED";
    case MYBOT_STATE_FAILED:
        return "FAILED";
    case MYBOT_STATE_STOPPING:
        return "STOPPING";
    case MYBOT_STATE_IN_CONVERSATION:
        return "IN_CONVERSATION";
    default:
        return "UNKNOWN";
    }
}

/* The SDK owns the state machine. Poll its atomic view at the application
 * boundary and emit one production log record per observed edge. */
static void mybot_log_state_transition(mybot_state_t *last_state, bool *valid)
{
    mybot_state_t current;

    if (!last_state || !valid) {
        return;
    }
    current = mybot_get_state();
    if (!*valid) {
        BK_LOGI(TAG, "runtime state=%s(%d)\r\n", mybot_state_name(current),
                (int)current);
        *last_state = current;
        *valid = true;
        return;
    }
    if (current != *last_state) {
        BK_LOGI(TAG, "runtime state transition: %s(%d) -> %s(%d)\r\n",
                mybot_state_name(*last_state), (int)*last_state,
                mybot_state_name(current), (int)current);
        *last_state = current;
    }
}

/*
 * Product identity. The device id follows the BK725x MyBot controller rule
 * (mybot_controller_bk725x.c build_device_config()): the 32-byte per-chip
 * device UID, read from OTP by the CP and published through bk_uid, is hashed
 * with MD5 and hex-encoded to 32 lowercase characters. The server base follows
 * the selected language, the firmware version is the SDK's own version, and the
 * hardware model is fixed by this port. None of them is a Kconfig value.
 */
static int mybot_build_config(void)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char uid[32] = {0};
    unsigned char digest[16];

    s_config = (mybot_config_t){0};
    (void)snprintf(s_config.server_base, sizeof(s_config.server_base), "%s",
                   MYBOT_SERVER_BASE);
    (void)snprintf(s_config.firmware_ver, sizeof(s_config.firmware_ver), "%s",
                   MYBOT_VERSION_STRING);
    (void)snprintf(s_config.hw_model, sizeof(s_config.hw_model), "%s",
                   "mybot-bk7259");

    if (bk_uid_get_data(uid) != BK_OK) {
        /* The UID is the only source of the device identity: nothing here
         * stands in for it, and a device without an identity can be neither
         * provisioned nor started. */
        BK_LOGE(TAG, "device UID unavailable; no device identity\r\n");
        return -1;
    }
    if (mbedtls_md5(uid, sizeof(uid), digest) != 0) {
        BK_LOGE(TAG, "failed to hash the device UID\r\n");
        return -1;
    }
    for (size_t i = 0; i < sizeof(digest); ++i) {
        s_config.device_id[i * 2] = hex[digest[i] >> 4];
        s_config.device_id[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    s_config.device_id[sizeof(digest) * 2] = '\0';
    return 0;
}

static int mybot_run(void)
{
    bool aosl_ref_held = false;
    mybot_state_t last_state = MYBOT_STATE_STOPPED;
    bool state_valid = false;

    if (mybot_build_config() < 0) {
        BK_LOGE(TAG, "could not build the device configuration\r\n");
        return -1;
    }

    /* Provisioning prompts use the same AOSL-backed audio adapter as the SDK,
     * but they can run before mybot_start() acquires its own reference. Keep
     * one product-lifetime reference so both paths share initialized globals. */
    aosl_ctor();
    aosl_ref_held = true;
    mybot_log_state_transition(&last_state, &state_valid);

    if (s_config.server_base[0] == '\0') {
        BK_LOGW(TAG,
                "MYBOT_SERVER_BASE is empty; runtime startup will fail\r\n");
    }
    BK_LOGI(TAG, "device=%s server=%s\r\n", s_config.device_id,
            s_config.server_base);

    if (mybot_bk7259_platform_prepare() < 0) {
        BK_LOGE(TAG, "platform preparation failed\r\n");
        aosl_dtor();
        return -1;
    }

    if (mybot_bk7259_platform_register() < 0) {
        BK_LOGE(TAG, "platform registration failed\r\n");
        mybot_bk7259_platform_shutdown();
        aosl_dtor();
        return -1;
    }

    int result = -1;
    for (;;) {
        mybot_log_state_transition(&last_state, &state_valid);
        /* The SDK is not running here, so the erase is safe to perform. */
        if (mybot_bk7259_wait_reset_request(0)) {
            BK_LOGW(TAG, "factory reset requested while the SDK is stopped\r\n");
            mybot_bk7259_factory_reset();
        }
        int network_result = mybot_bk7259_ensure_network(s_config.device_id);
        mybot_log_state_transition(&last_state, &state_valid);
        if (network_result < 0) {
            BK_LOGE(TAG, "network setup failed\r\n");
            break;
        }
        if (network_result > 0) {
            (void)mybot_bk7259_wait_provision_request(0);
        } else if (mybot_bk7259_wait_provision_request(0)) {
            if (mybot_bk7259_provision_wifi(s_config.device_id) < 0) {
                BK_LOGE(TAG, "APSTA provisioning failed\r\n");
                break;
            }
            (void)mybot_bk7259_wait_provision_request(0);
            continue;
        }

        BK_LOGI(TAG, "starting SDK\r\n");
        if (mybot_start(&s_config) < 0) {
            mybot_log_state_transition(&last_state, &state_valid);
            BK_LOGE(TAG, "SDK startup failed\r\n");
            mybot_stop();
            mybot_log_state_transition(&last_state, &state_valid);
            break;
        }
        BK_LOGI(TAG, "SDK started\r\n");
        mybot_log_state_transition(&last_state, &state_valid);

        bool provision_requested = false;
        bool reset_requested = false;
        bool sdk_failed = false;
        while (mybot_is_running()) {
            mybot_log_state_transition(&last_state, &state_valid);
            if (mybot_bk7259_wait_reset_request(MYBOT_POLL_INTERVAL_MS)) {
                reset_requested = true;
                break;
            }
            if (mybot_bk7259_wait_provision_request(MYBOT_POLL_INTERVAL_MS)) {
                provision_requested = true;
                break;
            }
            if (mybot_get_state() == MYBOT_STATE_FAILED) {
                BK_LOGE(TAG, "SDK entered the failed state\r\n");
                sdk_failed = true;
                break;
            }
        }
        mybot_log_state_transition(&last_state, &state_valid);

        if (!provision_requested && !reset_requested) {
            provision_requested = mybot_bk7259_wait_provision_request(0);
            reset_requested = mybot_bk7259_wait_reset_request(0);
            sdk_failed = sdk_failed || mybot_get_state() == MYBOT_STATE_FAILED;
        }

        BK_LOGI(TAG, "stopping SDK, reason=%s\r\n",
                reset_requested ? "reset"
                                : (provision_requested ? "provision" : "sdk_exit"));
        mybot_state_t stop_state = mybot_get_state();
        BK_LOGI(TAG, "SDK stop requested from state=%s(%d)\r\n",
                mybot_state_name(stop_state), (int)stop_state);
        mybot_stop();
        mybot_log_state_transition(&last_state, &state_valid);
        if (!provision_requested) {
            provision_requested = mybot_bk7259_wait_provision_request(0);
        }
        if (!reset_requested) {
            reset_requested = mybot_bk7259_wait_reset_request(0);
        }

        /* The SDK has released its resources, so the erase takes precedence over
         * the STOPPED check and over provisioning; it never returns. */
        if (reset_requested) {
            BK_LOGW(TAG, "performing factory reset\r\n");
            mybot_bk7259_factory_reset();
        }

        if (mybot_is_running() || mybot_get_state() != MYBOT_STATE_STOPPED) {
            BK_LOGE(TAG, "SDK shutdown did not reach STOPPED\r\n");
            break;
        }
        if (!provision_requested) {
            result = sdk_failed ? -1 : 0;
            break;
        }

        BK_LOGI(TAG, "starting APSTA provisioning\r\n");
        if (mybot_bk7259_provision_wifi(s_config.device_id) < 0) {
            BK_LOGE(TAG, "APSTA provisioning failed\r\n");
            break;
        }
        (void)mybot_bk7259_wait_provision_request(0);
    }

    mybot_stop();
    mybot_log_state_transition(&last_state, &state_valid);
    mybot_bk7259_platform_shutdown();
    if (aosl_ref_held) {
        aosl_dtor();
    }
    return result;
}

static void mybot_app_task(beken_thread_arg_t arg)
{
    (void)arg;
    int result = mybot_run();
    BK_LOGI(TAG, "MyBot task exited, result=%d\r\n", result);
    rtos_delete_thread(NULL);
}

int main(void)
{
    if (bk_init() != 0) {
        BK_LOGE(TAG, "BK component initialization failed\r\n");
        return -1;
    }

    /* Same banner as the BK725x controller, printed once the SDK is up so the
     * log is live. */
    BK_LOGI(TAG, "mybot version: %s, build time: %s\r\n", MYBOT_VERSION_STRING,
            (const char *)build_version);

    /*
     * bk_display's DPU backend calls the AVDK Nano OSI wrapper indirectly
     * (for example, viv_os_mem_alloc -> vsios_malloc).  The wrapper table is
     * registered by media_service_init(); it must be ready before LCD/DPU
     * preparation starts.
     */
    if (media_service_init() != 0) {
        BK_LOGE(TAG, "media service initialization failed\r\n");
        return -1;
    }

    /*
     * The vendor startup invokes main() from a short-lived, highest-priority
     * app_main_thread and deletes it after main returns.  Keep the product
     * control loop in its own normal-priority task so it cannot monopolize an
     * SMP core while waiting for network or SDK events.
     */
    if (rtos_create_psram_thread(NULL, BEKEN_DEFAULT_WORKER_PRIORITY,
                                 "mybot_app", mybot_app_task,
                                 MYBOT_APP_TASK_STACK_SIZE, NULL) != BK_OK) {
        BK_LOGE(TAG, "failed to create MyBot task\r\n");
        return -1;
    }

    return 0;
}
