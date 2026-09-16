# Third-party notices

## MyBot SDK

The sources under `include/` and `src/` are based on a snapshot of
https://github.com/junlon2006/mybot at commit
`1baee9a61ddaa4c4b7b72406fa6c8a0503f4b61d`, with target patches recorded in
`SDK_REVISION`. They are provided under the Apache License 2.0 in `LICENSE`,
except where a source file states otherwise.

`src/support/mybot_json.c` and `src/internal/mybot_json.h` are namespaced
derivatives of cJSON. Their MIT license and Dave Gamble copyright notice are
retained in those files.

The 4-bit UI glyph coverage in
`platforms/bk7259/bk7259_lcd_font.inc` is generated from Liberation Sans
2.1.5 in the pinned BK7259 AVDK tree. The source TTF has SHA256
`3e81ba4717a115f8d125cd2327d4a1246be2997b9ffd7291a45b6f84ed1a27d1`.
The derived data is identified internally as the MyBot UI font and does not
use the reserved font name as its name. It remains under the SIL Open Font
License 1.1 in `platforms/bk7259/OFL-1.1.txt`.

The embedded Chinese and English Ogg/Opus prompt assets under
`projects/mybot/assets/` are derived from the MIT-licensed `xiaozhi-esp32`
resources. Their license and attribution are retained in
`projects/mybot/assets/LICENSE.xiaozhi-esp32`.

## AOSL

AOSL is vendored by the sibling `mybot_aosl` component from commit
`84e086084ebcd0ae2455a0ce5721950c5fe2e656`. Its license adds conditions to
Apache-2.0; read `../mybot_aosl/aosl/LICENSE` before use or redistribution.

## Agora RTSA SDK

The BK7259 RTSA binary is supplied externally and is not part of this source
tree. Its package contains no standalone redistribution grant. Do not publish
the package, static library, or firmware containing it without written Agora
authorization.
