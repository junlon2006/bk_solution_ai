/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_LVGL_VIEW_H_
#define MYBOT_LVGL_VIEW_H_

#include "lvgl.h"
#include <mybot/platform/mybot_lcd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared LVGL view. Only the display owner task calls these functions and LVGL.
 * The display and its active screen must outlive the view. Its single LVGL timer
 * is owned by the view and removed before any widgets on destroy. Update borrows
 * content and provisioning_ssid for the duration of the call. SSIDs are at most
 * 32 bytes; NULL uses the generic provisioning title. No tasks are created here.
 * Chinese glyphs cover the fixed UI vocabulary, not arbitrary UTF-8 SSIDs. */
int mybot_lvgl_view_create(lv_display_t *display);
void mybot_lvgl_view_update(const mybot_lcd_content_t *content, const char *provisioning_ssid);
void mybot_lvgl_view_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_LVGL_VIEW_H_ */
