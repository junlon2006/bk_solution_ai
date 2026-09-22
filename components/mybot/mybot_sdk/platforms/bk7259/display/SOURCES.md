# MyBot LVGL status view

The platform-independent view and generated resources are adapted from
`junlon2006/mybot-esp32` commit
`19af3bdfaf1dd35461af48def8f9558e3503e755`:

- `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_view.cc`
- `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_assets.c`
- `components/mybot_platform/src/drivers/display/renderers/lvgl/lvgl_fonts.c`
- `components/mybot_platform/src/internal/display/lvgl_view.h`
- `components/mybot_platform/src/internal/display/lvgl_assets.h`

The view is MIT-licensed; the original layout reference and its copyright notice
are preserved in `assets/licenses/LVGL_VIEW_LICENSE.txt`. The asset API header
retains its Apache-2.0 SPDX identifier. The generated font and emoji pixel data
remain under SIL OFL-1.1, with their original notices in
`assets/licenses/LVGL_VIEW_FONT_LICENSE.txt` and
`assets/ui/noto_emoji/LICENSE.txt` respectively. Emoji source hashes, upstream
revision, format and generation details are retained in
`assets/ui/noto_emoji/SOURCES.json`.

The four constant 64x64 ARGB8888 emoji images total 65,536 pixel bytes. The
20-pixel MyBot UI Sans font contains 161 glyphs, including ASCII and the fixed
Chinese UI vocabulary. Neither bitmap set is modified in this port. Arbitrary
Chinese SSIDs or chat text require a separately extended font. The Chinese ready
hint is `开始对话`; the English hint is `Press to start`, matching button control.

BK7259 changes remove ESP-IDF and Wi-Fi controller dependencies, receive a copied
SSID from the display owner, keep dynamic label strings in fixed view storage,
and expand the card to the 385x320 logical screen. All view/timer operations run
on the same display owner task. The view never owns the panel or framebuffer.
