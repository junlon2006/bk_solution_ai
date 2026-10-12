# MyBot 工程（BK7259）

- [English](./README.md)

## 1. 工程简介

`mybot` 是 MyBot SDK 的 BK7259 AP/CP 双核固件集成工程，在同一次会话中提供设备服务控制、
RTC 双向音频、本地配网与界面，以及可选的单向编码视频上行。视频默认开启。
当前 `release/v4.0.1-mybot-r2` 分支适配耘趣 V1.0 / Robot R2 新板，包含 GPIO47 单键
控制会话与长按恢复出厂、GC9503CV 屏和双模拟麦 AEC；正常界面采用模拟器的工艺灰皮肤与像素眼镜。

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
6. 在产品循环中处理按键请求和 SDK 状态日志；启用视频时，摄像头等 RTC `CONNECTED` 后才启动。

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
GPIO27 在平台准备阶段输出高电平，使能摄像头外部 1.2V 电源。显示的板级电源 owner
持有 2.8V 和 1.2V 电源直到完整平台关闭；关闭会话或视频只通过 GPIO30 复位摄像头，
保留板级电源，保持屏幕点亮。
恢复中、失败和重启中提示支持中文/英文，由现有显示任务渲染，SDK 停止/解挂不会覆盖提示。

LVGL 9.5.0 界面采用用户提供模拟器的 `process`（工艺灰）皮肤与 `geek`（像素眼镜）画法，
使用 `#050708` 固定黑色玻璃底与灰色装饰，避免 RGB565 深色渐变色带。
“中性待机”使用 `#f1f1ee` 米白镜框、双弧眼和微笑；
“打招呼”使用 `#f1ca79` 暖金镜框、左弧眼、右圆点眼和微笑。

| 阶段 | 画面 |
| --- | --- |
| 启动、配网、鉴权或配对 | 中英文必要引导；配对码在网页控制台输入 |
| SDK 实际进入 `READY` | 默认中性待机 |
| 进入及保持 `IN_CONVERSATION` | 默认打招呼 |
| 挂断返回 `READY` | 中性待机 |
| 网络或启动失败 | 必要错误提示 |
| 恢复出厂 | 恢复中、失败或重启中提示，优先于 SDK 页面更新 |

配网成功不会改变 SDK 就绪状态或跳过配对。正常待机、会话画面不再显示声纹与
聆听/思考/回复状态，相关 RTM 事件仍由 SDK 处理，不切换表情、不弹提示、不重播过渡。
终端文字恢复模拟器的四行：

```text
> OIM / R2
> mood: neutral
> state: online
> agora / realtime
```

`mood` 使用当前表情的小写名称；`state` 在 `SLEEP` 时为 `standby`，其余表情为 `online`。
这些文字是画面装饰，不是 SDK 或连接状态。`</>` 签名和最后一行光标保留，UI 不控制云台动作。

全部 23 个表情均已实现，枚举顺序与模拟器一致：

```text
NEUTRAL, GREETING, HAPPY, EXCITED, CURIOUS, CONFUSED, THINKING, LISTENING,
SURPRISED, SHY, PROUD, LOVE, SLEEPY, BORED, SAD, PLEADING, APOLOGY, WORRIED,
ANNOYED, REFUSE, PROCESSING, ERROR, SLEEP
```

独立的 `include/bk7259_expression.h` 模块提供本地显示请求，供后续命令集成使用：

```c
#include "bk7259_expression.h"

int show_happy(void) {
    return bk7259_lcd_set_expression(BK7259_EXPRESSION_HAPPY);
}
```

`bk7259_expression_from_name()` 将完整模拟器名称转换为枚举，忽略 ASCII 字母大小写；
`bk7259_expression_name()` 返回稳定的大写名称。
`bk7259_lcd_clear_expression()` 恢复当前流程页面的 NEUTRAL 或 GREETING。
设置和清除返回 `0` 表示显示所有者接受了已复制的请求，不表示已经刷屏完成；
无效表情或当前无法接受表情页面请求时返回 `-1`。接口使用现有 LCD snapshot 锁和
mailbox，不在调用线程执行 LVGL。只有准备完成且运行中的 `READY`/`IN_CONVERSATION`
页面接受请求，配网、恢复出厂与关闭过程拒绝请求。流程页面变化会清除覆盖，
仅 indicator 更新会保留覆盖；重复请求同一表情不会重播过渡。

新增 21 个表情不会自动轮播，本次未实现 MCP/RTM 命令映射和 UART 动作。
选择 ERROR 或 SLEEP 图案不会让 SDK 进入错误状态，也不会使硬件休眠。

模拟器 1024x896 画布按 9/16 等比缩放为 576x504 虚拟区域，位于 640x480 逻辑屏的 `(32,-12)`。
上下只裁去空白，文字与表情仍位于圆角安全区内，眼镜总宽约 345 像素。
终端保留参考的 37 像素源坐标行距，`</>` 左对齐。
固定 LVGL 控件与采样曲线绘制表情；心形、星形、实心椭圆、腮红和泪滴使用五块小型
只读 A8 抗锯齿蒙版，合计 13631 字节 Flash，消除三角扇接缝。蒙版由参考几何在本地生成，不使用完整表情图片，
不增加 canvas 缓冲。`CONFIG_MYBOT_LVGL_UI_ANIMATIONS`
控制短过渡、表情动态效果和 500 ms 光标闪烁，浅色主题选项已移除。复用现有显示任务、20480 字节 RGB565
绘制条带、两块各 614400 字节扫描缓冲和 8192 字节任务栈，配置刷新间隔为 50 ms。
LVGL 内部分配与栈峰值仍需在同时音视频运行时实测。

终端装饰使用由 DejaVu Sans Mono Bold 生成的只读 BK7259 Terminal 子集：15 像素 ASCII
和 16 像素 `</>`，合计 4634 字节位图，替代 Unscii 16，按 LVGL 实际字宽贴近参考缩放文字比例，
光标按实际字宽定位。
中英文操作引导使用 MyBot UI Sans 20 字体子集。
旧 emoji 源码与许可证保留用于来源追溯，但不再链接。资源来源、蒙版生成与复现、模拟器 SHA256 见
[display/SOURCES.md](../../components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md)。
显示构建与验证记录见 [UI_VALIDATION.md](./UI_VALIDATION.md)。

## 7. 多模态视频上行

AP 默认配置开启视频（`CONFIG_MYBOT_VIDEO=y`）。在
`projects/mybot/ap/config/bk7259_ap/defconfig` 中改为
`# CONFIG_MYBOT_VIDEO is not set` 并 clean 后重新编译即可关闭。启用时 BK7259 视频链路为：

```text
MIPI CSI GC2053/CV2005 传感器（1280x720，5 fps）
  -> ISP MP（640x480 NV12）
  -> 硬件 FLEXA H.264 编码器
  -> 编码帧池
  -> MyBot 视频处理器
  -> Agora RTSA 主视频流
```

启用视频后的参数与行为：

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
工艺灰像素眼镜基础渲染已有用户真机通过反馈；四行文字与完整 23 表情尚待真机验证。
中英文固件编译、手势、恢复流程、持久化故障注入和配网取消已有主机验证；以下列表用于
后续回归，不能据此认定断电、驱动或 Flash 故障已全部做过真机验证。
视频相关检查需要先编译启用 `CONFIG_MYBOT_VIDEO=y` 的固件。

- [ ] 冷启动组合镜像 `all-app.bin`，确认 AP 和 CP 均正常启动。
- [ ] 确认摄像头探测前打印 `camera 1.2V power enabled: gpio=27 level=1`，并在新摄像头硬件上测量 GPIO27 电平与外部 1.2V 电源。
- [ ] 短按松手开启/关闭会话；关闭后屏幕仍亮，双向音频和 AEC 正常。
- [ ] 确认实际 `READY` 时为中性待机、会话全程保持打招呼、挂断后立即回中性待机。
- [ ] 确认四行终端文字、小写表情名称、仅 SLEEP 显示 `standby`，对照模拟器验证 23 个表情及动态效果。
- [ ] 验证本地设置/清除、非法名称和枚举、indicator 更新、流程切换及恢复优先级，确认没有自动轮播。
- [ ] 确认声纹、聆听/思考/回复 RTM 更新不弹提示、不覆盖表情、不重播切换动画。
- [ ] 验证中英文引导、网页配对文案、圆角安全区域及恢复出厂显示优先级。
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
- [ ] 确认结束会话或视频后 GPIO30 保持复位，GPIO27/1.2V 与 GPIO29/2.8V 仍开启；仅完整平台关闭时释放板级电源。
