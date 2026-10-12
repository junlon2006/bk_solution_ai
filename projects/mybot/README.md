# MyBot Project (BK7259)

- [中文](./README_CN.md)

## 1. Overview

`mybot` is the BK7259 AP/CP firmware integration for the MyBot SDK. It combines
device-service control, bidirectional RTC audio, local provisioning and UI, and
optional one-way encoded video uplink in one conversation. Video is enabled by default.

The `release/v4.0.1-mybot-r2` branch targets the Yunqu V1.0 R2 board with a
GC9503CV MIPI LCD, KEY1/GPIO47, and dual analog microphones plus an amplifier
feedback channel for hardware AEC. Its normal UI follows the simulator's
process-gray skin and pixel-glasses face.

## 2. AP and CP Roles

| Core | Responsibility |
| --- | --- |
| AP (`bk7259_ap`) | Owns the product control loop, APSTA provisioning, MyBot SDK and AOSL lifecycle, HTTPS/RTC/RTM, audio, LCD, keys, camera capture, H.264 encoding, and video uplink. |
| CP (`bk7259`) | Initializes the BK runtime and starts the AP system. |

The `bk7259` project target builds and packages both cores.

## 3. Build

Use the `bk_avdk_smp` revision pinned by the parent repository on the matching
`release/v4.0.1-mybot-r2` line. From the `mybot-bk7259` repository root:

```bash
make -C bk_solution_ai/projects/mybot bk7259 SDK_DIR="$PWD/bk_avdk_smp"
```

For a clean rebuild:

```bash
make -C bk_solution_ai/projects/mybot clean SDK_DIR="$PWD/bk_avdk_smp"
make -C bk_solution_ai/projects/mybot bk7259 SDK_DIR="$PWD/bk_avdk_smp"
```

The main outputs are:

```text
bk_solution_ai/projects/mybot/build/bk7259/mybot/package/all-app.bin
bk_solution_ai/projects/mybot/build/bk7259/mybot/package/app_pack.rbl
```

The per-core ELF files are under `build/bk7259/mybot/bk7259_ap/` and
`build/bk7259/mybot/bk7259/` relative to the project directory.

## 4. Startup

AP initializes the BK runtime and media service, then creates the `mybot_app`
product task. The task derives the stable `BK7259-...` ID from the chip UID,
initializes AOSL, and prepares the product-owned LCD, network manager, and key,
in that order.
It checks `mybot.factory_reset.v1` before platform registration, Wi-Fi setup, or
SDK startup: a valid marker resumes reset; a reported marker read/validation
error displays reset failure and waits for a fresh long press.

Without a reset marker, AP registers the platform and tries saved Wi-Fi. Missing
or unusable credentials lead to APSTA provisioning at `http://192.168.4.1/`.
After STA obtains IPv4 and the portal stops, the task starts the SDK for device
authentication and pairing as needed. Pairing codes are entered in the web
console. The product loop polls SDK state and handles key requests.

Network/SDK failures retain the prepared key and display for factory-reset
requests. Identity, platform-preparation, or descriptor-registration failures
can exit startup; that recovery path is not guaranteed for those earlier errors.
Descriptor-registration failure shuts down the prepared platform resources.

## 5. Keys and Factory Reset

| KEY1/GPIO47 action | Behavior |
| --- | --- |
| Short press and release | `READY`: start conversation; `IN_CONVERSATION`: stop conversation; other states ignore it |
| Hold for 5 seconds | Request factory reset once; release does not toggle conversation |
| Already held at platform preparation | Release first to arm a new gesture |

The active-low key uses 20 ms scanning and two-sample debounce. Long press
works after successful platform preparation, including provisioning and before
SDK key attachment. A key held while the platform prepares must first be
released to arm a new gesture. Five seconds is the gesture threshold; actual
completion also depends on SDK and driver teardown. The CEN button remains
hardware reset only; double-press, volume, and other key actions are not registered.

Normal conversation hangup stops local video/RTC and clears session PCM before
publishing READY and sending one HTTPS stop notification. Notification failure
does not restart media or retry; HTTP still runs synchronously on the SDK
control worker, so a subsequent start can wait for it to finish.

The key timer posts requests; `ap_main.c` executes reset in the product task.
Its order is:

1. Show restoring status and persist `mybot.factory_reset.v1`.
2. Stop the SDK and require `mybot_is_running() == false` and `STOPPED`.
3. Stop the portal/SoftAP, scan, and STA; require completed teardown.
4. Delete and verify absence of `mybot.device_auth`, `mybot.wifi.v1`, and
   `mybot.volume.v1`, then delete the reset marker last.
5. Show rebooting status and restart; normal startup enters Wi-Fi provisioning.

SDK and provisioning writers stop before deletion, including the volume
adapter's final persistence attempt. Only these product records are cleared;
the UID-derived device ID stays the same and no server-side unbind request is
sent. The next SDK startup uses the default speaker volume and registers/authenticates again according to
the server's retained binding state.

Failure displays a bilingual retry page and waits for a fresh five-second hold.
Incomplete cleanup keeps the established marker; a final marker-deletion error
attempts to restore it. Boot resumes valid pending resets before starting the
network or SDK. A reported marker error requires an explicit new long press.
The marker is written when the product task processes the request; loss of
power before that point does not establish a pending reset. Failed network
teardown keeps its resources owned for retry. Network cancellation returns control to the owner, which verifies all
teardown results before deletion. The existing LCD worker retains restoring/
failed/rebooting pages across SDK stop/detach.

Module declarations are independent of the platform abstraction:

- `platforms/bk7259/include/bk7259_key.h`: conversation request and event dispatch.
- `platforms/bk7259/include/bk7259_factory_reset.h`: reset requests, persistent
  intent/cleanup, network teardown, and reset display phases.
- `platforms/bk7259/include/mybot_bk7259_platform.h`: platform registration and
  preparation abstraction.

These paths are relative to `components/mybot/mybot_sdk/`. The first two APIs
use `bk7259_*` names. Reset storage is implemented in `bk7259_factory_reset.c`;
it does not erase the whole EasyFlash environment.

## 6. R2 Audio and Display

Capture reads three differential analog ADC lanes:

| ADC | Signal | Analog gain | Digital gain |
| --- | --- | --- | --- |
| ADC0 | Microphone 1 | 20 | 16 dB |
| ADC1 | Microphone 2 | 20 | 16 dB |
| ADC2 | HT6872 amplifier feedback | 0 | 0 dB |

The ADC stream disables digital microphones and the appended digital reference
(`dmic_en=0`, `adc_cfg.aec_en=0`). AEC is enabled in the downstream algorithm:

```text
onboard_mic_stream_v2 (ADC0 + ADC1 + ADC2)
  -> aec_v3_algorithm_v2 (hardware AEC, in-channel reference ADC2)
  -> raw stream -> 16 kHz mono signed 16-bit PCM -> RTC uplink
```

The AEC stage uses three ADC channels, `dual_ch=1`, traditional noise suppression,
and VAD disabled. Its main output includes AEC, dual-microphone processing, and
noise suppression. Speaker playback uses the onboard DAC stream and active-high
GPIO50 amplifier enable. Volume changes apply speaker digital gain and save
`mybot.volume.v1` synchronously; failures retry on a later set or adapter
destruction. This board has no volume keys.

The GC9503CV panel is native 480x640 with a logical 640x480 RGB565 UI. GPIO20
enables the 3.3 V rail, GPIO29 the shared LCD/camera 2.8 V rail, GPIO42 resets the
panel, and GPIO40 drives its active-low backlight. GPIO27 drives high during
platform preparation to enable the external camera 1.2 V rail. The LCD
board-power owner retains both rails until full platform shutdown;
conversation/video teardown resets the sensor through GPIO30 without cutting
board power or blanking the display. The existing UI worker renders both SDK screens and
bilingual restoring/failed/rebooting pages; reset status takes precedence over
SDK stop/detach updates.

The LVGL 9.5.0 face follows the user-supplied simulator's `process` skin and
`geek` face style: a fixed `#050708` black glass background and gray decorations
avoid visible dark-gradient banding in RGB565. The neutral
idle face uses `#f1f1ee` off-white glasses, two curved eyes, and a smile. The
greeting uses `#f1ca79` warm-gold glasses, a curved left eye, a round right eye,
and a smile.

| Product stage | Display |
| --- | --- |
| Startup, provisioning, authentication, or pairing | Bilingual operational guidance; pairing codes are entered in the web console |
| Actual SDK `READY` | Neutral idle face by default |
| Entering and remaining in `IN_CONVERSATION` | Greeting face by default |
| Hangup returning to `READY` | Neutral idle face |
| Network/startup error | Operational error guidance |
| Factory reset | Restoring, failed, or rebooting page, with priority over SDK display updates |

Wi-Fi provisioning does not change SDK readiness or bypass pairing. Normal
idle and conversation screens hide voiceprint and listening/thinking/speaking
indicators. The SDK continues processing those events, but they do not change
the face, show toasts, or replay the transition. The simulator's four terminal
lines are restored:

```text
> OIM / R2
> mood: neutral
> state: online
> agora / realtime
```

The mood uses the selected expression's lowercase name. The state line is
`standby` for `SLEEP`, otherwise `online`; these are artwork labels, not SDK or
connectivity state. The `</>` signature and cursor on the last row remain.
This UI does not control gimbal movement.

All 23 simulator expressions are implemented, in the simulator's enum order:

```text
NEUTRAL, GREETING, HAPPY, EXCITED, CURIOUS, CONFUSED, THINKING, LISTENING,
SURPRISED, SHY, PROUD, LOVE, SLEEPY, BORED, SAD, PLEADING, APOLOGY, WORRIED,
ANNOYED, REFUSE, PROCESSING, ERROR, SLEEP
```

The independent `include/bk7259_expression.h` module exposes local display
requests, ready for later command integration:

```c
#include "bk7259_expression.h"

int show_happy(void) {
    return bk7259_lcd_set_expression(BK7259_EXPRESSION_HAPPY);
}
```

`bk7259_expression_from_name()` converts an exact simulator name while ignoring
ASCII letter case; `bk7259_expression_name()` returns its stable uppercase
name. `bk7259_lcd_clear_expression()` restores NEUTRAL or GREETING for the current
workflow screen. Set/clear return `0` when the owner accepts the copied request,
not when scanout completes, and `-1` for an invalid expression or an unavailable
face page. They use the existing LCD snapshot lock and mailbox, never caller-side
LVGL. Requests are accepted only on prepared, running `READY`/`IN_CONVERSATION`
pages; provisioning, reset, and shutdown reject them. Workflow screen changes
clear overrides, indicator-only updates preserve them, and requesting the same
active expression does not replay its transition.

The additional 21 expressions do not cycle automatically. MCP/RTM command
mapping and UART actions are outside this change. Selecting the ERROR or SLEEP
artwork does not enter the SDK's error state or suspend the hardware.

The simulator's 1024x896 artwork is scaled uniformly by 9/16 to a virtual
576x504 area at logical `(32,-12)` on the 640x480 display. The cropped top and
bottom contain only empty canvas space; visible text and face geometry stay
inside the rounded-bezel safe area. The glasses span about 345 pixels.
Terminal rows retain the reference's 37-pixel source grid, and the signature
is left-aligned. Fixed LVGL widgets and sampled curves draw the face. Five small
immutable A8 masks (13631 bytes in Flash) render hearts, stars, filled ellipses, cheeks, and tears
without triangle-fan seams. They are generated locally from the reference
geometry; no full-face images or additional canvas buffer are required.
`CONFIG_MYBOT_LVGL_UI_ANIMATIONS` enables short
face transitions, expression motion, and a 500 ms cursor blink; there is no light-theme selector.
Rendering uses the existing owner task, a 20480-byte RGB565 draw strip, two
614400-byte scanout buffers, and an 8192-byte task stack. The configured refresh
interval is 50 ms. Internal LVGL allocations and stack peaks still need target
measurement while audio/video run.

Terminal decorations use read-only BK7259 Terminal subsets derived from
DejaVu Sans Mono Bold: 15-pixel ASCII and a 16-pixel `</>` signature, totaling
4634 bitmap bytes. These replace Unscii 16; their actual LVGL text advances
closely match the reference's scaled text proportions.
The cursor follows the actual text width. Bilingual operation text uses the
existing MyBot UI Sans 20 subset. The old emoji sources and
licenses remain for provenance, but the emoji image source is no longer linked.
See [display/SOURCES.md](../../components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md)
for simulator hashes, mask generation/reproduction, and resource origins.

## 7. Multimodal Video Uplink

The default AP configuration enables video (`CONFIG_MYBOT_VIDEO=y`). To disable
it, use `# CONFIG_MYBOT_VIDEO is not set` in
`projects/mybot/ap/config/bk7259_ap/defconfig` and run a clean rebuild. With video
enabled, the BK7259 video path is:

```text
MIPI CSI GC2053/CV2005 sensor (1280x720 at 5 fps)
  -> ISP MP (640x480 NV12)
  -> hardware FLEXA H.264 encoder
  -> encoded-frame pool
  -> MyBot video handler
  -> Agora RTSA high video stream
```

Settings and behavior when video is enabled:

| Item | Value |
| --- | --- |
| Encoded output | H.264 Annex-B access units, 640x480 |
| Sensor input | 1280x720 at 5 fps |
| Bitrate range | 256000-512000 bit/s; initial midpoint 384000 bit/s |
| GOP | 30 frames, approximately 6 seconds at 5 fps; RTSA key-frame requests can force an IDR |
| Lifecycle | Capture and encoding start only after RTC reaches `CONNECTED`; they stop before leaving RTC |
| Direction | Device uplink only; remote video subscription is disabled |
| RTSA timing | `frame_rate=0`, so RTSA uses real frame timestamps instead of synthesizing a fixed cadence |

RTSA bandwidth estimates are clamped to the configured bitrate range and
applied to the hardware encoder. The audio path remains bidirectional while
video is uplink-only.

Video configuration is defined in:

- `components/mybot/mybot_sdk/Kconfig`
- `projects/mybot/ap/config/bk7259_ap/defconfig`

After changing video configuration, run a clean rebuild so the generated
configuration cannot retain stale values.

## 8. Real-Device Validation

The user confirmed R2 LCD bring-up, conversation start/stop without blanking,
audio/AEC, and the factory-reset feature work on hardware.
The user confirmed basic process-gray pixel-glasses rendering. The restored
four-line layout and complete 23-expression set await hardware validation.
The checklist below describes regression coverage; it does not imply every
fault or power-interruption scenario has already been tested on the device.
Video-specific checks require a build with `CONFIG_MYBOT_VIDEO=y`.

- [ ] Cold boot the combined `all-app.bin` and confirm both AP and CP start.
- [ ] Confirm the boot log reports `camera 1.2V power enabled: gpio=27 level=1` before sensor detection; verify GPIO27 and the external 1.2 V rail on the new camera hardware.
- [ ] Confirm the installed GC2053 or CV2005 camera is detected and the source reports 1280x720 at 5 fps with 640x480 H.264 output.
- [ ] Start a conversation and verify audio in both directions plus H.264 video at the remote endpoint.
- [ ] Verify camera capture and encoding start only after RTC is connected and emit no frames after conversation teardown begins.
- [ ] Measure approximately 5 fps, a 256-512 kbit/s encoder rate, and the initial 384 kbit/s target.
- [ ] Verify a periodic IDR about every 6 seconds and that an RTSA key-frame request forces a new IDR.
- [ ] Exercise constrained Wi-Fi and confirm RTSA bitrate updates stay within 256-512 kbit/s without breaking audio.
- [ ] End, restart, and interrupt conversations repeatedly; confirm the encoder, frame pool, camera, and RTC resources stop and restart cleanly.
- [ ] Confirm conversation/video stop holds GPIO30 in reset while GPIO27/1.2 V and GPIO29/2.8 V remain enabled; only full platform shutdown releases the board rails.
- [ ] Short press/release starts and stops a conversation without blanking the LCD.
- [ ] Verify neutral idle after actual `READY`, greeting for the entire conversation, and neutral immediately after hangup.
- [ ] Verify four terminal rows, lowercase expression names, and `standby` only for SLEEP; compare all 23 faces and their motion with the simulator.
- [ ] Exercise local set/clear requests, invalid names/IDs, indicator updates, workflow changes, and reset priority; verify no automatic expression cycling.
- [ ] Verify RTM voiceprint/listening/thinking/speaking updates do not show overlays or restart face transitions.
- [ ] Verify both languages, web-console pairing instructions, round-bezel clearance, and factory-reset display priority.
- [ ] A hold below 5 seconds performs only the state-appropriate short action on release; a 5-second hold resets once and suppresses that action.
- [ ] Hold the key across reboot, then release; confirm a new gesture is required before another reset.
- [ ] Request reset during conversation, saved-STA connection, and APSTA provisioning; confirm shutdown precedes record deletion and the reset page remains visible.
- [ ] Confirm Wi-Fi credentials, local authentication, and saved volume are cleared, while device ID and server-side binding are preserved.
- [ ] Interrupt power after the reset marker is persisted; confirm boot resumes cleanup before network/SDK startup.
- [ ] Inject shutdown/storage failures; confirm the failure page waits for a fresh 5-second hold rather than rebooting or erasing while writers remain active.

See [UI_VALIDATION.md](./UI_VALIDATION.md) for build and host-test evidence.
