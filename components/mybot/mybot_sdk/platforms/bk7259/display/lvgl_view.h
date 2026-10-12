/* SPDX-License-Identifier: MIT */
#ifndef MYBOT_LVGL_VIEW_H_
#define MYBOT_LVGL_VIEW_H_

#include "bk7259_expression.h"
#include "bk7259_factory_reset.h"
#include "bk7259_lcd.h"
#include "lvgl.h"
#include <mybot/platform/mybot_lcd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Product LVGL view. Only the display owner task calls these functions and LVGL.
 * The display and its active screen must outlive the view. Its single LVGL timer
 * is owned by the view and removed before any widgets on destroy. Update borrows
 * content, provisioning_ssid and runtime status for the duration of the call.
 * SSIDs are at most 32 bytes; NULL uses the generic provisioning title. A NULL
 * runtime status displays stopped. No tasks are created here.
 * Chinese glyphs cover the fixed UI vocabulary, not arbitrary UTF-8 SSIDs.
 * READY renders the neutral pixel-glasses face; IN_CONVERSATION renders the
 * greeting face. Runtime state and SDK indicators update only the third terminal
 * line; these updates preserve expressions and their animation clocks.
 * Objects and borrowed line-point arrays are fixed for the view's lifetime. */
int mybot_lvgl_view_create(lv_display_t *display);
void mybot_lvgl_view_update(const mybot_lcd_content_t *content, const char *provisioning_ssid,
                            const bk7259_lcd_runtime_status_t *status);
/* Display-owner-only expression override. Guidance/reset screens reject it.
 * Repeating the current ID is a no-op and never restarts an animation. */
int bk7259_lvgl_view_show_expression(bk7259_expression_t expression);
/* Product-owned override; the LCD snapshot owner keeps SDK updates from
 * replacing it. Uses existing widgets/timer and borrows no external strings. */
void bk7259_lvgl_view_show_factory_reset(bk7259_factory_reset_screen_t screen);
void mybot_lvgl_view_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_LVGL_VIEW_H_ */
