# MyBot Project (BK7259)

- [中文](./README_CN.md)

## 1. Overview

`mybot` is the BK7259 AP/CP firmware integration for the MyBot SDK. It combines
device-service control, bidirectional RTC audio, local provisioning and UI, and
one-way encoded video uplink in one conversation.

## 2. AP and CP Roles

| Core | Responsibility |
| --- | --- |
| AP (`bk7259_ap`) | Owns the product control loop, APSTA provisioning, MyBot SDK and AOSL lifecycle, HTTPS/RTC/RTM, audio, LCD, keys, camera capture, H.264 encoding, and video uplink. |
| CP (`bk7259`) | Initializes the BK runtime and starts the AP system. |

The `bk7259` project target builds and packages both cores.

## 3. Build

Use the matching `bk_avdk_smp` release. From the `mybot-bk7259` repository root:

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

## 4. Multimodal Video Uplink

The default AP configuration enables `CONFIG_MYBOT_VIDEO=y`. The BK7259 video
path is:

```text
MIPI CSI GC2053/CV2005 sensor (1280x720 at 5 fps)
  -> ISP MP (640x480 NV12)
  -> hardware FLEXA H.264 encoder
  -> encoded-frame pool
  -> MyBot video handler
  -> Agora RTSA high video stream
```

Current settings and behavior:

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

## 5. Real-Device Validation

- [ ] Cold boot the combined `all-app.bin` and confirm both AP and CP start.
- [ ] Confirm the installed GC2053 or CV2005 camera is detected and the source reports 1280x720 at 5 fps with 640x480 H.264 output.
- [ ] Start a conversation and verify audio in both directions plus H.264 video at the remote endpoint.
- [ ] Verify camera capture and encoding start only after RTC is connected and emit no frames after conversation teardown begins.
- [ ] Measure approximately 5 fps, a 256-512 kbit/s encoder rate, and the initial 384 kbit/s target.
- [ ] Verify a periodic IDR about every 6 seconds and that an RTSA key-frame request forces a new IDR.
- [ ] Exercise constrained Wi-Fi and confirm RTSA bitrate updates stay within 256-512 kbit/s without breaking audio.
- [ ] End, restart, and interrupt conversations repeatedly; confirm the encoder, frame pool, camera, and RTC resources stop and restart cleanly.
