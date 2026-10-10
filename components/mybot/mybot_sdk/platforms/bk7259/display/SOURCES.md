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
20-pixel MyBot UI Sans font contains 170 glyphs, including ASCII and the fixed
Chinese UI vocabulary. It includes the four glyphs needed by the web-console
pairing prompt: `页`, `控`, `制`, and `台`. Arbitrary
Chinese SSIDs or chat text require a separately extended font. The Chinese ready
hint is `开始对话`; the English hint is `Press to start`, matching button control.

The local `generate-lvgl-font.py` generator is adapted from the reference's
MIT-licensed font generator and reads the fixed vocabulary in `lvgl_view.cc`.
It regenerates the 20-pixel, 4 bpp subset from the OFL-licensed
`SourceHanSansSC-Normal.otf`, with required input SHA256
`1ee89e1669362dee13851129c0a8a791a87521eb4148e5efbf5d26596738e25b`.
The current subset includes all Chinese factory-reset messages and punctuation,
uses 22,018 bitmap bytes, and was generated with Pillow 10.2.0 / FreeType 2.13.2.
The source font is an external generation input; firmware embeds only this
subset. Run `python3 generate-lvgl-font.py --font /path/to/SourceHanSansSC-Normal.otf`
to regenerate, or `python3 generate-lvgl-font.py --check` to check UI glyph
coverage without the source font or Pillow.

BK7259 changes remove ESP-IDF and Wi-Fi controller dependencies, receive a copied
SSID from the display owner, keep dynamic label strings in fixed view storage,
and size the card for the R2 panel's 640x480 logical screen (native GC9503CV
480x640). The rounded bezel is
accounted for with header insets of 40 pixels horizontally and 14 vertically,
footer insets of 32 pixels horizontally and 18 from the bottom, and a 16-pixel
horizontal card margin. Text widths and pairing-code font selection follow the
actual label width inside these margins. All view/timer operations run
on the same display owner task. The view never owns the panel or framebuffer.

The R2 product adds Chinese and English factory-reset restoring, failed, and
rebooting presentations through `bk7259_lvgl_view_show_factory_reset()`. The LCD
snapshot owner gives this product status priority over SDK workflow updates and
detach, reusing the existing widgets, timer, and framebuffers.
