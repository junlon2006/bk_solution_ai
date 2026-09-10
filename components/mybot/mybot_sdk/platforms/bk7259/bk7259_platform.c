/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_bk7259_platform.h"
#include "bk7259_ops.h"

#include <components/log.h>
#include <components/system.h>
#include <easyflash.h>
#include <mybot/platform/mybot_platform.h>
#include <os/os.h>

#define TAG "mybot_platform"

static bool s_prepared;

void mybot_bk7259_factory_reset(void) {
    /* Every persisted MyBot record (Wi-Fi credentials, device credentials,
     * volume) lives in the EasyFlash environment, so resetting the partition
     * is the whole factory reset. The AP environment has no vendor calibration
     * data. The caller must have stopped the SDK first. */
    BK_LOGW(TAG, "factory reset: erasing persisted state\r\n");
    if (ef_env_set_default() != EF_NO_ERR) {
        BK_LOGE(TAG, "factory reset: EasyFlash environment erase failed\r\n");
    }
    BK_LOGW(TAG, "factory reset: rebooting\r\n");
    bk_reboot();
    for (;;) {
        (void)rtos_delay_milliseconds(1000);
    }
}

void mybot_bk7259_set_conversation_state_getter(
    mybot_bk7259_conversation_state_getter_t getter) {
    bk7259_key_set_conversation_state_getter(getter);
}

int mybot_bk7259_platform_prepare(void) {
    if (s_prepared) {
        return 0;
    }
    if (bk7259_lcd_prepare() < 0) {
        BK_LOGE(TAG, "LCD preparation failed\r\n");
        return -1;
    }
    if (bk7259_wifi_prepare() < 0) {
        BK_LOGE(TAG, "Wi-Fi preparation failed\r\n");
        bk7259_lcd_shutdown();
        return -1;
    }
    if (bk7259_key_prepare() < 0) {
        BK_LOGE(TAG, "key preparation failed\r\n");
        bk7259_wifi_shutdown();
        bk7259_lcd_shutdown();
        return -1;
    }
    s_prepared = true;
    BK_LOGI(TAG, "platform prepared\r\n");
    return 0;
}

int mybot_bk7259_platform_register(void) {
    static const mybot_platform_descriptor_t descriptor = {
        .wifi = &g_mybot_bk7259_wifi_ops,
        .kv_store = &g_mybot_bk7259_kv_ops,
        .key = &g_mybot_bk7259_key_ops,
        .audio_capture = &g_mybot_bk7259_capture_ops,
        .audio_playback = &g_mybot_bk7259_playback_ops,
        .audio_volume = &g_mybot_bk7259_volume_ops,
        .https = &g_mybot_bk7259_https_ops,
        .lcd = &g_mybot_bk7259_lcd_ops,
        .announce = &g_mybot_bk7259_announce_ops,
        .wake_words = NULL,
    };

    int result = mybot_platform_register(&descriptor);
    if (result < 0) {
        BK_LOGE(TAG, "platform descriptor registration failed\r\n");
        return result;
    }
    BK_LOGI(TAG, "platform descriptor registered\r\n");
    return 0;
}

void mybot_bk7259_platform_shutdown(void) {
    if (!s_prepared) {
        return;
    }
    BK_LOGI(TAG, "platform shutting down\r\n");
    bk7259_key_shutdown();
    bk7259_wifi_shutdown();
    bk7259_lcd_shutdown();
    s_prepared = false;
    BK_LOGI(TAG, "platform shut down\r\n");
}
