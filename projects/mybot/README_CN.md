# MyBot 工程（BK7259）

- [English](./README.md)

## 1. 工程简介

`mybot` 是 MyBot SDK 的 BK7259 AP/CP 双核固件集成工程，在同一次会话中提供设备服务控制、
RTC 双向音频、本地配网与界面，以及单向编码视频上行。

## 2. AP 与 CP 职责

| 核 | 职责 |
| --- | --- |
| AP（`bk7259_ap`） | 负责产品控制循环、APSTA 配网、MyBot SDK 与 AOSL 生命周期、HTTPS/RTC/RTM、音频、LCD、按键、摄像头采集、H.264 编码和视频上行。 |
| CP（`bk7259`） | 初始化 BK 运行环境并启动 AP 系统。 |

`bk7259` 工程目标会同时编译并打包两个核。

## 3. 编译

请使用版本匹配的 `bk_avdk_smp`。在 `mybot-bk7259` 仓库根目录执行：

```bash
make -C bk_solution_ai/projects/mybot bk7259 SDK_DIR="$PWD/bk_avdk_smp"
```

完整重新编译：

```bash
make -C bk_solution_ai/projects/mybot clean SDK_DIR="$PWD/bk_avdk_smp"
make -C bk_solution_ai/projects/mybot bk7259 SDK_DIR="$PWD/bk_avdk_smp"
```

主要产物为：

```text
bk_solution_ai/projects/mybot/build/bk7259/mybot/package/all-app.bin
bk_solution_ai/projects/mybot/build/bk7259/mybot/package/app_pack.rbl
```

以工程目录为基准，各核 ELF 分别位于 `build/bk7259/mybot/bk7259_ap/` 和
`build/bk7259/mybot/bk7259/`。

## 4. 多模态视频上行

AP 默认配置已开启 `CONFIG_MYBOT_VIDEO=y`。BK7259 视频链路为：

```text
MIPI CSI GC2053/CV2005 传感器（1280x720，5 fps）
  -> ISP MP（640x480 NV12）
  -> 硬件 FLEXA H.264 编码器
  -> 编码帧池
  -> MyBot 视频处理器
  -> Agora RTSA 主视频流
```

当前参数与行为：

| 项目 | 当前值 |
| --- | --- |
| 编码输出 | H.264 Annex-B access unit，640x480 |
| 传感器输入 | 1280x720，5 fps |
| 码率范围 | 256000-512000 bit/s，初始中点为 384000 bit/s |
| GOP | 30 帧；5 fps 下约 6 秒，RTSA 关键帧请求可强制生成 IDR |
| 生命周期 | RTC 到达 `CONNECTED` 后才启动采集和编码；离开 RTC 前先停止 |
| 方向 | 仅设备视频上行，远端视频订阅已关闭 |
| RTSA 时序 | `frame_rate=0`，由 RTSA 使用真实帧时间戳，不按固定帧率合成时间戳 |

RTSA 的带宽估算结果会被限制在配置的码率范围内，再应用到硬件编码器。音频保持双向，
视频仅上行。

视频配置位于：

- `components/mybot/mybot_sdk/Kconfig`
- `projects/mybot/ap/config/bk7259_ap/defconfig`

修改视频配置后应执行完整重新编译，避免生成配置保留旧值。

## 5. 真机验收

- [ ] 冷启动组合镜像 `all-app.bin`，确认 AP 和 CP 均正常启动。
- [ ] 确认板上 GC2053 或 CV2005 摄像头识别成功，视频源报告 1280x720、5 fps，并输出 640x480 H.264。
- [ ] 开启会话，确认双向音频正常，远端能收到 H.264 视频。
- [ ] 确认摄像头采集和编码只在 RTC 连接后启动；会话拆除开始后不再发送视频帧。
- [ ] 实测帧率约 5 fps、编码码率位于 256-512 kbit/s，初始目标为 384 kbit/s。
- [ ] 确认周期 IDR 间隔约 6 秒，并验证 RTSA 关键帧请求会强制生成新的 IDR。
- [ ] 在受限 Wi-Fi 下确认 RTSA 码率调整始终位于 256-512 kbit/s，且不会破坏音频通路。
- [ ] 反复结束、重启和中断会话，确认编码器、帧池、摄像头及 RTC 资源均能正常停止并再次启动。
