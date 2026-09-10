# Third-party notices

## MyBot SDK

The sources under `include/` and `src/` are an unmodified snapshot of
https://github.com/junlon2006/mybot at commit
`27324e7177b52ad9d8743aba31acf94d0a125f44`. They are provided under the
Apache License 2.0 in `LICENSE`, except where a source file states otherwise.

`src/support/mybot_json.c` and `src/internal/mybot_json.h` are namespaced
derivatives of cJSON. Their MIT license and Dave Gamble copyright notice are
retained in those files.

The compact digit and uppercase glyphs in
`platforms/bk7259/bk7259_lcd.c` are adapted from the Apache-2.0 MyBot ESP32
display implementation at https://github.com/junlon2006/mybot-esp32, commit
`c8687315522f640683acda7d8dccbe5e40c9050b`.

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
