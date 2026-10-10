# MyBot 工程（BK7259）

- [English](./README.md)

## 1. 工程简介

`mybot` 是 MyBot SDK 的 BK7259 AP/CP 双核固件集成工程，在同一次会话中提供设备服务控制、
RTC 双向音频、本地配网与界面，以及单向编码视频上行。
当前 `release/v4.0.1-mybot-r2` 分支适配耘趣 V1.0 / Robot R2 新板，包含 GPIO47 单键
控制会话与长按恢复出厂、GC9503CV 屏和双模拟麦 AEC。

## 2. AP 与 CP 职责

| 核 | 职责 |
| --- | --- |
| AP（`bk7259_ap`） | 负责产品控制循环、APSTA 配网、MyBot SDK 与 AOSL 生命周期、恢复出厂、HTTPS/RTC/RTM、音频、LCD、按键、摄像头采集、H.264 编码和视频上行。 |
| CP（`bk7259`） | 初始化 BK 运行环境并启动 AP 系统。 |

`bk7259` 工程目标会同时编译并打包两个核。

## 3. 编译

请使用父仓库固定的 `release/v4.0.1-mybot-r2` AVDK，其中包含 GC9503CV 屏驱动。
在 `mybot-bk7259` 仓库根目录执行：

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

## 4. 上电初始化

CP 初始化 BK 系统并启动 AP。AP 的 `main()` 执行 `bk_init()`、打印版本、初始化媒体
服务，再创建产品控制任务；媒体服务先准备好显示后端需要的 OSI wrapper。

产品任务依次执行：

1. 根据芯片 UID 生成 `BK7259-<24位大写十六进制>` 设备 ID，填充语言、服务地址和版本配置。
2. 初始化 AOSL，准备 LCD、Wi-Fi 管理器和 GPIO47 扫描定时器。
3. 检查 `mybot.factory_reset.v1`；有效标记先继续恢复出厂，可检测的异常标记等待新长按。
4. 无恢复任务时注册平台描述符，尝试已保存的 Wi-Fi，或进入 APSTA 配网门户。
5. STA 获得 IPv4 并关闭门户、SoftAP 后启动 MyBot SDK，按服务器状态完成注册和绑定检查。
6. 在产品循环中处理按键请求和 SDK 状态日志；摄像头仍等 RTC `CONNECTED` 后才启动。

网络设置或 SDK 启动失败后的等待状态保留产品显示和按键，允许长按恢复出厂；媒体、
平台准备或注册等更早的初始化错误会退出产品任务。

## 5. 按键与恢复出厂

对话键 `KEY1` 使用 GPIO47，内部上拉、低电平有效。每 20 ms 扫描，两次一致采样消抖：

| 手势 | 行为 |
| --- | --- |
| 不足 5 秒松开 | `READY` 开启对话，`IN_CONVERSATION` 关闭对话，其余状态忽略短按 |
| 按住达到 5 秒 | 投递一次恢复出厂请求，松手不触发会话切换 |
| 按键准备时已按住 | 先松开再按，避免跨重启重复触发 |

CEN 保留硬件复位功能，双击、音量和其它按键未注册。长按在按键扫描准备后即可使用，
不依赖 SDK 按键回调；5 秒是手势阈值，清理完成时间取决于 SDK 和驱动退出。

正常挂断先停止本地视频与 RTC、清理会话 PCM，恢复 READY 后再发送一次 HTTPS 停止通知。
通知失败不重启媒体、不重试；HTTP 仍占用 SDK 控制线程，紧接着发起的新会话可能等待
该请求完成。

`ap_main.c` 是恢复操作的所有者：显示恢复状态，保存并读回意图，停止 SDK 并确认
`STOPPED`，停止门户/SoftAP/扫描/STA，再依次删除并检查以下记录，最后删除恢复标记并重启：

| 记录 | 内容 |
| --- | --- |
| `mybot.device_auth` | 本地设备鉴权 |
| `mybot.wifi.v1` | Wi-Fi 凭据 |
| `mybot.volume.v1` | 持久化音量 |

失败时显示“恢复失败，请长按重试”，业务保持停止，不自动重启；松手后新长按可重试。
未完成的清理保留意图，最终标记删除报错时尝试重建标记。有效意图保存后遇到掉电，
下次启动在联网前继续清理；部分删除不会回滚。按键请求仅在
产品任务处理时才持久化。该功能不擦除整个 ENV、固件或射频校准数据，不改变 UID/设备 ID，
不解绑服务器。重启后重新配网、注册，并按服务端保留的绑定状态继续工作。

模块接口使用 `bk7259_*` 命名，独立于平台抽象：

- `components/mybot/mybot_sdk/platforms/bk7259/include/bk7259_key.h`：领取会话请求、投递开始/结束事件。
- `components/mybot/mybot_sdk/platforms/bk7259/include/bk7259_factory_reset.h`：恢复请求、意图持久化、网络退出、配置清理和显示阶段。
- `mybot_bk7259_platform.h`：保持平台准备、注册、网络和关闭接口。

## 6. R2 音频与显示

三通道采集映射如下，均为差分模拟输入、16 kHz、16 位：

| 通道 | 信号 | 模拟增益 | 数字增益 |
| --- | --- | --- | --- |
| ADC0 | 第一颗模拟麦 | 20 dB | 16 dB |
| ADC1 | 第二颗模拟麦 | 20 dB | 16 dB |
| ADC2 | HT6872 功放回采参考 | 0 dB | 0 dB |

采集链为 `onboard_mic_stream_v2 -> aec_v3_algorithm_v2 -> raw_stream`。DMIC 和 ADC
附加数字参考关闭（`dmic_en=0`、`adc_cfg.aec_en=0`）；AEC 本身启用硬件模式、通道内
ADC2 参考、双麦处理和传统降噪，VAD 关闭，输出给 SDK 的是单声道 PCM。
GPIO50 高电平使能扬声器功放。音量通过 SDK `audio_volume` ops 调整数字增益，变更时
同步持久化；写入失败在后续设置或模块销毁时重试。当前没有音量按键，恢复出厂后使用
扬声器默认增益。

GC9503CV MIPI 屏物理分辨率为 480x640，逻辑画面为 640x480 RGB565；GPIO20 控制 3.3V，
GPIO29 控制 LCD/摄像头共用 2.8V 电源，GPIO42 为复位，GPIO40 背光低有效。
显示从平台准备到平台关闭由产品持有；关闭会话只复位摄像头，不关闭共用电源，保持屏幕点亮。
恢复中、失败和重启中提示支持中文/英文，由现有显示任务渲染，SDK 停止/解挂不会覆盖提示。
显示构建与验证记录见 [UI_VALIDATION.md](./UI_VALIDATION.md)。

## 7. 多模态视频上行

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

## 8. 真机验收

R2 LCD、双模拟麦上行与 AEC、结束会话后的显示以及长按恢复出厂已有用户真机通过反馈。
中英文固件编译、手势、恢复流程、持久化故障注入和配网取消已有主机验证；以下列表用于
后续回归，不能据此认定断电、驱动或 Flash 故障已全部做过真机验证。

- [ ] 冷启动组合镜像 `all-app.bin`，确认 AP 和 CP 均正常启动。
- [ ] 短按松手开启/关闭会话；关闭后屏幕仍亮，双向音频和 AEC 正常。
- [ ] 按住约 4.9 秒松手不恢复出厂；按住超过 5 秒只恢复一次，松手不发会话事件。
- [ ] 持续按住跨过重启不重复恢复，先松开后才能产生新手势。
- [ ] 分别在配网、离线和会话中长按，检查恢复提示、自动重启和重新配网。
- [ ] 清理后设备 ID 和服务端绑定保持不变，音量使用默认值，非产品 ENV 记录仍存在。
- [ ] 在有效意图保存后中断供电，检查下次启动续做；注入停止/删除失败，检查失败提示和新长按重试。
- [ ] 确认板上 GC2053 或 CV2005 摄像头识别成功，视频源报告 1280x720、5 fps，并输出 640x480 H.264。
- [ ] 开启会话，确认双向音频正常，远端能收到 H.264 视频。
- [ ] 确认摄像头采集和编码只在 RTC 连接后启动；会话拆除开始后不再发送视频帧。
- [ ] 实测帧率约 5 fps、编码码率位于 256-512 kbit/s，初始目标为 384 kbit/s。
- [ ] 确认周期 IDR 间隔约 6 秒，并验证 RTSA 关键帧请求会强制生成新的 IDR。
- [ ] 在受限 Wi-Fi 下确认 RTSA 码率调整始终位于 256-512 kbit/s，且不会破坏音频通路。
- [ ] 反复结束、重启和中断会话，确认编码器、帧池、摄像头及 RTC 资源均能正常停止并再次启动。
