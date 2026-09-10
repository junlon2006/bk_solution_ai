/* SPDX-License-Identifier: Apache-2.0 */
#include "bk7259_ops.h"

#include <mybot/platform/mybot_lcd.h>

#include <common/avdk_pixel_types.h>
#include <common/bk_err.h>
#include <components/bk_display.h>
#include <components/bk_frame_buffer.h>
#include "bk7259_platform_log.h"
#include <driver/gpio.h>
#include <gpio_driver.h>
#include <lcd/lcd_mipi_jd9855_320x385.h>
#include <modules/pm.h>
#include <os/os.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TAG "mybot_lcd"

#define LCD_NATIVE_WIDTH 320
#define LCD_NATIVE_HEIGHT 385
#define LCD_LOGICAL_WIDTH LCD_NATIVE_HEIGHT
#define LCD_LOGICAL_HEIGHT LCD_NATIVE_WIDTH
#define LCD_BYTES_PER_PIXEL 2
#define LCD_FRAME_BYTES (LCD_NATIVE_WIDTH * LCD_NATIVE_HEIGHT * LCD_BYTES_PER_PIXEL)
#define LCD_FRAME_COUNT 2
#define LCD_RENDER_TIMEOUT_MS 250

#define LCD_POWER_GPIO GPIO_53
#define LCD_RESET_GPIO GPIO_5
#define LCD_BACKLIGHT_GPIO GPIO_7

#define COLOR_BLACK 0x0000
#define COLOR_NEAR_BLACK 0x0841
#define COLOR_WHITE 0xffff
#define COLOR_CYAN 0x3fff
#define COLOR_BLUE 0x4a7f
#define COLOR_GREEN 0x47e8
#define COLOR_YELLOW 0xffe0
#define COLOR_AMBER 0xfd20
#define COLOR_RED 0xf986
#define COLOR_GRAY 0x8410
/* Voiceprint indicator: the MyBot ESP32 boards' saved green, RGB565(114, 255, 156).
 * The not-yet-registered state borrows the shared red instead of the ESP32 amber,
 * which did not stand out against the conversation screen. */
#define COLOR_VP_SAVED 0x77f3

typedef struct {
    bk_display_bus_handle_t bus;
    bk_avdk_lcd_panel_handle_t panel;
    bk_display_ctlr_handle_t controller;
    uint16_t *frames[LCD_FRAME_COUNT];
    uint32_t frame_busy[LCD_FRAME_COUNT];
    beken_semaphore_t frame_done;
    unsigned int next_frame;
    bool vddio_owned;
    bool power_gpio_owned;
    bool backlight_gpio_owned;
    bool controller_inited;
    bool controller_open;
    bool prepared;
    bool accepting;
    bool sdk_attached;
} lcd_context_t;

static lcd_context_t s_lcd;
static beken_mutex_t s_lcd_lock;
static bool s_frame_buffer_initialized;
static const bk_display_dpu_config_t s_controller_config = {
    .video = {
        .enable = true,
        .decompress = false,
        .format = BK_PIXEL_FORMAT_RGB565,
    },
};

/* Adapted from the Apache-2.0 MyBot ESP32 display implementation. */
static const uint8_t s_digits[10][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31},
    {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e},
};

static const uint8_t s_letters[26][5] = {
    {0x7e, 0x11, 0x11, 0x11, 0x7e},
    {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a},
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0x00, 0x41, 0x7f, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f},
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7f, 0x01, 0x01}, {0x3f, 0x40, 0x40, 0x40, 0x3f},
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t *glyph_for(char character)
{
    static const uint8_t space[5] = {0};
    static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t question[5] = {0x02, 0x01, 0x51, 0x09, 0x06};

    if (character >= '0' && character <= '9') {
        return s_digits[character - '0'];
    }
    if (character >= 'A' && character <= 'Z') {
        return s_letters[character - 'A'];
    }
    if (character == ' ') {
        return space;
    }
    if (character == '-') {
        return dash;
    }
    return question;
}

static bool ensure_lcd_lock(void)
{
    if (s_lcd_lock) {
        return true;
    }
    return rtos_init_mutex(&s_lcd_lock) == BK_OK;
}

static void put_pixel(uint16_t *frame, int x, int y, uint16_t color)
{
    if (!frame || x < 0 || x >= LCD_LOGICAL_WIDTH || y < 0 || y >= LCD_LOGICAL_HEIGHT) {
        return;
    }

    /* AVDK ROTATE_90 is a 270-degree buffer rotation into the native panel. */
    int native_x = y;
    int native_y = LCD_LOGICAL_WIDTH - x - 1;
    frame[(size_t)native_y * LCD_NATIVE_WIDTH + (size_t)native_x] = color;
}

static void fill_screen(uint16_t *frame, uint16_t color)
{
    for (size_t i = 0; i < (size_t)LCD_NATIVE_WIDTH * LCD_NATIVE_HEIGHT; ++i) {
        frame[i] = color;
    }
}

static void fill_rect(uint16_t *frame, int x, int y, int width, int height, uint16_t color)
{
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            put_pixel(frame, x + column, y + row, color);
        }
    }
}

static void draw_line(uint16_t *frame, int x0, int y0, int x1, int y1, int thickness,
                      uint16_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        fill_rect(frame, x0 - thickness / 2, y0 - thickness / 2, thickness, thickness, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }
        int doubled = error * 2;
        if (doubled >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void draw_ring(uint16_t *frame, int center_x, int center_y, int radius, int thickness,
                      uint16_t color)
{
    int outer_squared = radius * radius;
    int inner_radius = radius - thickness;
    int inner_squared = inner_radius * inner_radius;

    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            int dx = x - center_x;
            int dy = y - center_y;
            int distance = dx * dx + dy * dy;
            if (distance <= outer_squared && distance >= inner_squared) {
                put_pixel(frame, x, y, color);
            }
        }
    }
}

static void draw_disc(uint16_t *frame, int center_x, int center_y, int radius, uint16_t color)
{
    int radius_squared = radius * radius;

    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx * dx + dy * dy <= radius_squared) {
                put_pixel(frame, x, y, color);
            }
        }
    }
}

static void draw_character(uint16_t *frame, int x, int y, char character, int scale,
                           uint16_t color)
{
    const uint8_t *glyph = glyph_for(character);

    for (int column = 0; column < 5; ++column) {
        for (int row = 0; row < 7; ++row) {
            if (glyph[column] & (1u << row)) {
                fill_rect(frame, x + column * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text_centered(uint16_t *frame, int y, const char *text, size_t length, int scale,
                               uint16_t color)
{
    int width = length == 0 ? 0 : ((int)length * 6 - 1) * scale;
    int x = (LCD_LOGICAL_WIDTH - width) / 2;

    for (size_t i = 0; i < length; ++i) {
        draw_character(frame, x + (int)i * 6 * scale, y, text[i], scale, color);
    }
}

static uint16_t screen_color(mybot_lcd_screen_t screen)
{
    switch (screen) {
    case MYBOT_LCD_SCREEN_STARTING:
        return COLOR_CYAN;
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
        return COLOR_AMBER;
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
    case MYBOT_LCD_SCREEN_FAILED:
        return COLOR_RED;
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        return COLOR_BLUE;
    case MYBOT_LCD_SCREEN_PAIRING:
        return COLOR_YELLOW;
    case MYBOT_LCD_SCREEN_READY:
        return COLOR_GREEN;
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        return COLOR_CYAN;
    case MYBOT_LCD_SCREEN_STOPPING:
        return COLOR_GRAY;
    case MYBOT_LCD_SCREEN_PAIR_CODE:
    case MYBOT_LCD_SCREEN_COUNT:
        return COLOR_CYAN;
    }
    return COLOR_GRAY;
}

static const char *screen_label(mybot_lcd_screen_t screen)
{
    switch (screen) {
    case MYBOT_LCD_SCREEN_STARTING:
        return "STARTING";
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
        return "WIFI SETUP";
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
        return "WIFI LOST";
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        return "SERVICES";
    case MYBOT_LCD_SCREEN_PAIRING:
        return "PAIRING";
    case MYBOT_LCD_SCREEN_PAIR_CODE:
        return "PAIR CODE";
    case MYBOT_LCD_SCREEN_READY:
        return "READY";
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        return "TALKING";
    case MYBOT_LCD_SCREEN_FAILED:
        return "FAILED";
    case MYBOT_LCD_SCREEN_STOPPING:
        return "STOPPING";
    case MYBOT_LCD_SCREEN_COUNT:
        return NULL;
    }
    return NULL;
}

static void draw_state_icon(uint16_t *frame, mybot_lcd_screen_t screen, uint16_t color)
{
    const int center_x = LCD_LOGICAL_WIDTH / 2;
    const int center_y = 118;

    draw_ring(frame, center_x, center_y, 70, 5, color);
    switch (screen) {
    case MYBOT_LCD_SCREEN_STARTING:
    case MYBOT_LCD_SCREEN_STARTING_SERVICES:
        draw_line(frame, center_x, center_y - 37, center_x, center_y, 8, color);
        draw_line(frame, center_x, center_y, center_x + 28, center_y + 20, 8, color);
        break;
    case MYBOT_LCD_SCREEN_WIFI_PROVISIONING:
    case MYBOT_LCD_SCREEN_PAIRING:
        draw_disc(frame, center_x - 32, center_y, 9, color);
        draw_disc(frame, center_x, center_y, 9, color);
        draw_disc(frame, center_x + 32, center_y, 9, color);
        break;
    case MYBOT_LCD_SCREEN_WIFI_DISCONNECTED:
    case MYBOT_LCD_SCREEN_FAILED:
        draw_line(frame, center_x - 28, center_y - 28, center_x + 28, center_y + 28, 9,
                  color);
        draw_line(frame, center_x + 28, center_y - 28, center_x - 28, center_y + 28, 9,
                  color);
        break;
    case MYBOT_LCD_SCREEN_READY:
        draw_line(frame, center_x - 36, center_y, center_x - 10, center_y + 27, 9, color);
        draw_line(frame, center_x - 10, center_y + 27, center_x + 43, center_y - 31, 9,
                  color);
        break;
    case MYBOT_LCD_SCREEN_IN_CONVERSATION:
        fill_rect(frame, center_x - 39, center_y - 23, 12, 46, color);
        fill_rect(frame, center_x - 6, center_y - 40, 12, 80, color);
        fill_rect(frame, center_x + 27, center_y - 23, 12, 46, color);
        break;
    case MYBOT_LCD_SCREEN_STOPPING:
        fill_rect(frame, center_x - 35, center_y - 5, 70, 10, color);
        break;
    case MYBOT_LCD_SCREEN_PAIR_CODE:
    case MYBOT_LCD_SCREEN_COUNT:
        break;
    }
}

static void draw_voiceprint_glyph(uint16_t *frame, int center_x, int center_y, uint16_t color)
{
    /* A single waveform trace reads as a voiceprint; the badge's disc color,
     * not the glyph, carries the registration state. */
    draw_line(frame, center_x - 10, center_y, center_x - 7, center_y, 3, color);
    draw_line(frame, center_x - 7, center_y, center_x - 4, center_y - 4, 3, color);
    draw_line(frame, center_x - 4, center_y - 4, center_x - 1, center_y + 6, 3, color);
    draw_line(frame, center_x - 1, center_y + 6, center_x + 2, center_y - 8, 3, color);
    draw_line(frame, center_x + 2, center_y - 8, center_x + 5, center_y + 3, 3, color);
    draw_line(frame, center_x + 5, center_y + 3, center_x + 8, center_y, 3, color);
    draw_line(frame, center_x + 8, center_y, center_x + 10, center_y, 3, color);
}

static void draw_voiceprint_overlay(uint16_t *frame, bool registered)
{
    const int center_x = LCD_LOGICAL_WIDTH / 2 + 53;
    const int center_y = 66;

    draw_disc(frame, center_x, center_y, 17, registered ? COLOR_VP_SAVED : COLOR_RED);
    draw_voiceprint_glyph(frame, center_x, center_y, COLOR_BLACK);
}

static int pair_code_length(const char *code, size_t *out_length)
{
    if (!code || !out_length) {
        return -1;
    }

    size_t length = 0;
    while (length < MYBOT_LCD_PAIR_CODE_CAPACITY && code[length] != '\0') {
        ++length;
    }
    if (length != 6) {
        return -1;
    }
    for (size_t index = 0; index < length; ++index) {
        if (code[index] < '0' || code[index] > '9') {
            return -1;
        }
    }
    *out_length = length;
    return 0;
}

static int render_content(uint16_t *frame, const mybot_lcd_content_t *content)
{
    if (!frame || !content || content->screen < MYBOT_LCD_SCREEN_STARTING ||
        content->screen >= MYBOT_LCD_SCREEN_COUNT) {
        return -1;
    }

    fill_screen(frame, content->screen == MYBOT_LCD_SCREEN_PAIR_CODE ? COLOR_BLACK
                                                                     : COLOR_NEAR_BLACK);
    const char *label = screen_label(content->screen);
    if (!label) {
        return -1;
    }

    if (content->screen == MYBOT_LCD_SCREEN_PAIR_CODE) {
        size_t code_length;
        if (pair_code_length(content->pair_code, &code_length) < 0) {
            return -1;
        }
        int scale = 8;
        while ((((int)code_length * 6 - 1) * scale > LCD_LOGICAL_WIDTH - 24) && scale > 1) {
            --scale;
        }
        draw_text_centered(frame, 56, label, strlen(label), 4, COLOR_CYAN);
        draw_text_centered(frame, (LCD_LOGICAL_HEIGHT - 7 * scale) / 2 + 22,
                           content->pair_code, code_length, scale, COLOR_WHITE);
        return 0;
    }

    uint16_t color = screen_color(content->screen);
    draw_state_icon(frame, content->screen, color);
    draw_text_centered(frame, 235, label, strlen(label), 4, color);
    if (content->screen == MYBOT_LCD_SCREEN_IN_CONVERSATION) {
        draw_voiceprint_overlay(
            frame, (content->indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED) != 0);
    }
    return 0;
}

static void release_frame(unsigned int index)
{
    if (index >= LCD_FRAME_COUNT) {
        return;
    }
    if (__atomic_exchange_n(&s_lcd.frame_busy[index], 0, __ATOMIC_ACQ_REL) != 0 &&
        s_lcd.frame_done) {
        (void)rtos_set_semaphore(&s_lcd.frame_done);
    }
}

static avdk_err_t frame_release_callback(void *frame)
{
    for (unsigned int index = 0; index < LCD_FRAME_COUNT; ++index) {
        if (frame == s_lcd.frames[index]) {
            release_frame(index);
            return AVDK_ERR_OK;
        }
    }
    return AVDK_ERR_INVAL;
}

static int wait_for_frame(unsigned int *out_index)
{
    uint32_t started_at = rtos_get_time();

    for (;;) {
        for (unsigned int offset = 0; offset < LCD_FRAME_COUNT; ++offset) {
            unsigned int index = (s_lcd.next_frame + offset) % LCD_FRAME_COUNT;
            if (__atomic_load_n(&s_lcd.frame_busy[index], __ATOMIC_ACQUIRE) == 0) {
                *out_index = index;
                return 0;
            }
        }

        uint32_t elapsed = rtos_get_time() - started_at;
        if (elapsed >= LCD_RENDER_TIMEOUT_MS ||
            rtos_get_semaphore(&s_lcd.frame_done, LCD_RENDER_TIMEOUT_MS - elapsed) != BK_OK) {
            MYBOT_LOGE(TAG, "timed out waiting for a display frame");
            return -1;
        }
    }
}

static int submit_content_locked(const mybot_lcd_content_t *content)
{
    unsigned int index;
    if (!s_lcd.controller || wait_for_frame(&index) < 0 ||
        render_content(s_lcd.frames[index], content) < 0) {
        return -1;
    }

    __atomic_store_n(&s_lcd.frame_busy[index], 1, __ATOMIC_RELEASE);
    avdk_err_t result =
        bk_display_flush(s_lcd.controller, s_lcd.frames[index], frame_release_callback);
    if (result != AVDK_ERR_OK) {
        /* A failed update may already have become the active scanout frame. */
        MYBOT_LOGE(TAG, "frame submission failed: %d", result);
        return -1;
    }
    s_lcd.next_frame = (index + 1) % LCD_FRAME_COUNT;
    return 0;
}

static int set_backlight(bool enabled)
{
    if (!s_lcd.backlight_gpio_owned) {
        return -1;
    }
    return (enabled ? bk_gpio_set_output_low(LCD_BACKLIGHT_GPIO)
                    : bk_gpio_set_output_high(LCD_BACKLIGHT_GPIO)) == BK_OK
               ? 0
               : -1;
}

static int panel_power_on(void)
{
    if (gpio_dev_unmap(LCD_BACKLIGHT_GPIO) != BK_OK ||
        gpio_dev_unmap(LCD_POWER_GPIO) != BK_OK || gpio_dev_unmap(LCD_RESET_GPIO) != BK_OK ||
        bk_gpio_enable_output(LCD_RESET_GPIO) != BK_OK ||
        bk_gpio_set_output_low(LCD_RESET_GPIO) != BK_OK ||
        bk_gpio_enable_output(LCD_BACKLIGHT_GPIO) != BK_OK ||
        bk_gpio_pull_down(LCD_BACKLIGHT_GPIO) != BK_OK) {
        return -1;
    }
    /* The pinned BK7259 API exposes this setter as bool but its HAL returns
     * BK_OK (0) on success; keep the reference board's best-effort use. */
    (void)bk_gpio_set_capacity(LCD_BACKLIGHT_GPIO, GPIO_DRIVER_CAPACITY_3);
    if (bk_gpio_set_output_high(LCD_BACKLIGHT_GPIO) != BK_OK ||
        bk_gpio_enable_output(LCD_POWER_GPIO) != BK_OK ||
        bk_gpio_pull_up(LCD_POWER_GPIO) != BK_OK ||
        bk_gpio_set_output_high(LCD_POWER_GPIO) != BK_OK) {
        return -1;
    }
    s_lcd.backlight_gpio_owned = true;
    if (set_backlight(false) < 0) {
        return -1;
    }
    s_lcd.power_gpio_owned = true;

    pm_auxldo_ctrl_cfg_t vddio = {
        .ldo = AUXLDOS_SEL_1P8V,
        .out = PM_AUXLDO_1P8V_OUT_1P8V,
        .user = PM_AUXLDO_USER_DISPLAY,
        .state = PM_AUXLDO_ENABLE,
    };
    if (bk_pm_auxldo_ctrl_vote(&vddio) != BK_OK) {
        return -1;
    }
    s_lcd.vddio_owned = true;
    rtos_delay_milliseconds(10);
    return 0;
}

static int panel_power_off(void)
{
    int result = 0;

    if (s_lcd.backlight_gpio_owned) {
        if (set_backlight(false) != 0) {
            MYBOT_LOGW(TAG, "failed to disable panel backlight");
            result = -1;
        } else {
            s_lcd.backlight_gpio_owned = false;
        }
    }
    if (bk_gpio_enable_output(LCD_RESET_GPIO) != BK_OK ||
        bk_gpio_set_output_low(LCD_RESET_GPIO) != BK_OK) {
        MYBOT_LOGW(TAG, "failed to assert panel reset");
        return -1;
    }
    if (s_lcd.vddio_owned) {
        pm_auxldo_ctrl_cfg_t vddio = {
            .ldo = AUXLDOS_SEL_1P8V,
            .out = PM_AUXLDO_1P8V_OUT_1P8V,
            .user = PM_AUXLDO_USER_DISPLAY,
            .state = PM_AUXLDO_DISABLE,
        };
        if (bk_pm_auxldo_ctrl_vote(&vddio) != BK_OK) {
            MYBOT_LOGW(TAG, "failed to release panel VDDIO");
            return -1;
        }
        s_lcd.vddio_owned = false;
    }
    if (s_lcd.power_gpio_owned) {
        if (bk_gpio_set_output_low(LCD_POWER_GPIO) != BK_OK) {
            MYBOT_LOGW(TAG, "failed to disable panel 3.3V");
            result = -1;
        } else {
            s_lcd.power_gpio_owned = false;
        }
    }
    return result;
}

static bool resources_owned(void)
{
    return s_lcd.controller || s_lcd.panel || s_lcd.bus || s_lcd.frames[0] ||
           s_lcd.frames[1] || s_lcd.frame_done || s_lcd.vddio_owned ||
           s_lcd.power_gpio_owned || s_lcd.backlight_gpio_owned;
}

static int teardown_locked(void)
{
    if (s_lcd.backlight_gpio_owned) {
        if (set_backlight(false) == 0) {
            s_lcd.backlight_gpio_owned = false;
        }
    }
    if (s_lcd.controller) {
        if (s_lcd.controller_open) {
            if (bk_display_close(s_lcd.controller) != AVDK_ERR_OK) {
                MYBOT_LOGW(TAG, "failed to close display controller");
            } else {
                s_lcd.controller_open = false;
            }
        }
        if (s_lcd.controller_inited) {
            if (bk_display_deinit(s_lcd.controller) != AVDK_ERR_OK) {
                MYBOT_LOGE(TAG, "display deinit failed; retaining owned framebuffers");
                return -1;
            }
            s_lcd.controller_inited = false;
        }
        if (bk_display_delete(s_lcd.controller) != AVDK_ERR_OK) {
            MYBOT_LOGE(TAG, "display delete failed; retaining owned framebuffers");
            return -1;
        }
        s_lcd.controller = NULL;
        s_lcd.controller_inited = false;
        s_lcd.controller_open = false;
    }
    if (s_lcd.panel) {
        if (bk_lcd_panel_delete(s_lcd.panel) != BK_OK) {
            MYBOT_LOGE(TAG, "panel delete failed; retaining display resources");
            return -1;
        }
        s_lcd.panel = NULL;
    }
    if (s_lcd.bus) {
        if (bk_display_bus_delete(s_lcd.bus) != AVDK_ERR_OK) {
            MYBOT_LOGE(TAG, "DSI bus delete failed; retaining display resources");
            return -1;
        }
        s_lcd.bus = NULL;
    }
    int power_result = panel_power_off();

    /* display_deinit returns both the current and update buffers via the callback. */
    for (unsigned int index = 0; index < LCD_FRAME_COUNT; ++index) {
        __atomic_store_n(&s_lcd.frame_busy[index], 0, __ATOMIC_RELEASE);
        if (s_lcd.frames[index]) {
            bk_frame_buffer_free(s_lcd.frames[index]);
            s_lcd.frames[index] = NULL;
        }
    }
    if (s_lcd.frame_done) {
        (void)rtos_deinit_semaphore(&s_lcd.frame_done);
        s_lcd.frame_done = NULL;
    }
    s_lcd.next_frame = 0;
    return power_result;
}

int bk7259_lcd_prepare(void)
{
    if (!ensure_lcd_lock() || rtos_lock_mutex(&s_lcd_lock) != BK_OK) {
        return -1;
    }
    if (s_lcd.prepared) {
        rtos_unlock_mutex(&s_lcd_lock);
        return 0;
    }
    if (resources_owned() && teardown_locked() < 0) {
        rtos_unlock_mutex(&s_lcd_lock);
        return -1;
    }

    s_lcd.accepting = false;
    s_lcd.sdk_attached = false;
    if (!s_frame_buffer_initialized) {
        bk_frame_buffer_init();
        s_frame_buffer_initialized = true;
    }
    if (rtos_init_semaphore(&s_lcd.frame_done, LCD_FRAME_COUNT) != BK_OK) {
        goto failed;
    }
    for (unsigned int index = 0; index < LCD_FRAME_COUNT; ++index) {
        s_lcd.frames[index] = bk_frame_buffer_malloc(MEM_SLAB_HEAP_UNCODED, LCD_FRAME_BYTES);
        if (!s_lcd.frames[index]) {
            MYBOT_LOGE(TAG, "failed to allocate framebuffer %u", index);
            goto failed;
        }
    }
    if (panel_power_on() < 0 || bk_display_dsi_bus_new(&s_lcd.bus, NULL) != AVDK_ERR_OK) {
        goto failed;
    }

    const bk_lcd_panel_config_t panel_config = {.reset_pin = LCD_RESET_GPIO};
    if (bk_lcd_mipi_panel_new(s_lcd.bus, &panel_config,
                              &lcd_device_jd9855_mipi_320x385, &s_lcd.panel) != BK_OK) {
        goto failed;
    }
    if (bk_display_dpu_ctlr_new(&s_lcd.controller, s_lcd.panel, &s_controller_config) !=
        AVDK_ERR_OK) {
        goto failed;
    }
    if (bk_display_init(s_lcd.controller) != AVDK_ERR_OK) {
        goto failed;
    }
    s_lcd.controller_inited = true;
    if (bk_display_open(s_lcd.controller) != AVDK_ERR_OK) {
        goto failed;
    }
    s_lcd.controller_open = true;

    mybot_lcd_content_t starting = {.screen = MYBOT_LCD_SCREEN_STARTING};
    if (submit_content_locked(&starting) < 0) {
        goto failed;
    }
    rtos_delay_milliseconds(20);
    if (set_backlight(true) < 0) {
        goto failed;
    }
    s_lcd.prepared = true;
    s_lcd.accepting = true;
    MYBOT_LOGI(TAG, "JD9855 MIPI display ready");
    rtos_unlock_mutex(&s_lcd_lock);
    return 0;

failed:
    MYBOT_LOGE(TAG, "display preparation failed");
    (void)teardown_locked();
    s_lcd.prepared = false;
    rtos_unlock_mutex(&s_lcd_lock);
    return -1;
}

void bk7259_lcd_shutdown(void)
{
    if (!s_lcd_lock || rtos_lock_mutex(&s_lcd_lock) != BK_OK) {
        return;
    }
    s_lcd.accepting = false;
    s_lcd.sdk_attached = false;
    if (resources_owned()) {
        int result = teardown_locked();
        s_lcd.prepared = false;
        if (result == 0) {
            MYBOT_LOGI(TAG, "display shut down");
        }
    }
    rtos_unlock_mutex(&s_lcd_lock);
}

int bk7259_lcd_show_screen(mybot_lcd_screen_t screen)
{
    if (screen == MYBOT_LCD_SCREEN_PAIR_CODE) {
        /* A pair-code screen is only valid with the server-provided six
         * digit value; do not render a misleading placeholder. */
        return -1;
    }

    mybot_lcd_content_t content = {.screen = screen};

    if (!s_lcd_lock || rtos_lock_mutex(&s_lcd_lock) != BK_OK) {
        return -1;
    }
    int result = s_lcd.prepared && s_lcd.accepting ? submit_content_locked(&content) : -1;
    rtos_unlock_mutex(&s_lcd_lock);
    return result;
}

static int lcd_sdk_init(void **out_context)
{
    if (!out_context || !s_lcd_lock || rtos_lock_mutex(&s_lcd_lock) != BK_OK) {
        return -1;
    }
    *out_context = NULL;
    if (!s_lcd.prepared || !s_lcd.accepting || s_lcd.sdk_attached) {
        rtos_unlock_mutex(&s_lcd_lock);
        return -1;
    }
    s_lcd.sdk_attached = true;
    *out_context = &s_lcd;
    rtos_unlock_mutex(&s_lcd_lock);
    return 0;
}

static int lcd_sdk_render(void *context, const mybot_lcd_content_t *content)
{
    if (context != &s_lcd || !content || !s_lcd_lock ||
        rtos_lock_mutex(&s_lcd_lock) != BK_OK) {
        return -1;
    }
    int result = s_lcd.prepared && s_lcd.accepting && s_lcd.sdk_attached
                     ? submit_content_locked(content)
                     : -1;
    rtos_unlock_mutex(&s_lcd_lock);
    return result;
}

static void lcd_sdk_destroy(void *context)
{
    if (context != &s_lcd || !s_lcd_lock || rtos_lock_mutex(&s_lcd_lock) != BK_OK) {
        return;
    }
    s_lcd.sdk_attached = false;
    rtos_unlock_mutex(&s_lcd_lock);
}

const mybot_lcd_ops_t g_mybot_bk7259_lcd_ops = {
    .init = lcd_sdk_init,
    .render = lcd_sdk_render,
    .destroy = lcd_sdk_destroy,
};
