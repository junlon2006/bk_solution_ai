# BK7259 LVGL UI 验证记录

本文先记录当前像素眼镜界面的验证，再保留原始 LVGL 状态视图及 R2 功能的历史记录。
当前界面采用用户提供模拟器的工艺灰/像素眼镜样式，默认中文、启用表情过渡与光标动画。
实现位于 `components/mybot/mybot_sdk/platforms/bk7259/{bk7259_lcd.c,display/}`；
项目配置为 `ap/lv_conf_custom.h` 和 `ap/config/bk7259_ap/defconfig`。
SSID 由配网模块复制投递。SDK 的 `include/src` 快照已单独同步到锁定版本；AOSL 和 AVDK 源码
未因原始 UI 移植修改；后续 R2 屏驱动版本见下节。
旧直接渲染字体及其独立许可证已删除，可从 Git 历史恢复；新资源许可见
`components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md`。

## 2026-10-12 真实生命周期与会话状态

第三行由真实 SDK 生命周期驱动，READY 显示 `> state: ready`；会话中结合 SDK LCD
indicator 显示 `listening/thinking/speaking | vp:pending/ok`，没有有效阶段时回到
`in_conversation`。`vp:ok` 表示本会话已收到声纹成功通知，`pending` 表示尚未确认。
会话字段仅在运行状态和 LCD 页面同时处于 IN_CONVERSATION 时出现，挂断或断网后隐藏。
恢复页明确显示 `> state: factory_reset`，避免残留上一会话的阶段和 VP；该文字为产品流程，
不表示新增 SDK 枚举。四行坐标、字体、表情、光标和显示缓冲沿用现有实现。

应用在现有状态观测中读取 `mybot_get_state()`，补齐 PAIRING 名称映射，经独立
`include/bk7259_lcd.h` 接口复制到 LCD 快照。LCD 内部去重，首次准备完成及停止返回后
也发布观测值；仅 UI owner 操作 LVGL。SDK LCD 回调继续提供语义页面与 indicator。
启动前产品配网时 SDK 可以仍为 STOPPED，配网指导页继续可见。同步停止期间使用现有
STOPPING 指导页，应用轮询不保证捕获每个瞬态；停止完成后立即发布实际 stopped 值。
恢复提示仍拥有最高优先级。状态更新不清除表情 override，不重启过渡或光标时钟，
SLEEP 图案不再决定生命周期文字。

实现基于 Solution `aa95842d0ef80934123a6cb1c7bcc201e35a0ac5`，工作分支为
`release/v4.0.1-mybot-r2`。合同已核对 SDK `cfd178a` 的完整 PORTING/EMBEDDED、公开
`mybot.h`/`platform/mybot_lcd.h` 和 BK7259 RTM/声纹参考。MyBot revision lock 聚合摘要为
`bd59fb9a4f7aced74d5b8e770da869897229316c284e4ccded4d2aa8e667fa7a`，AOSL 摘要为
`2e9fc3fa5571972fd31c1ab64546477a98550b6b210e38fec181c0f4daeae64e`。
AVDK 固定 `90aaf113d78ef875635fcc49587f80fd2a8ddadc`，跟踪文件干净。SDK、AOSL、RTSA
和板级引脚不涉及本次修改，任务前 defconfig 与 RTM 文件哈希复核一致。

验证结果：

- 原工程执行 clean/build，随后包含恢复页文字修复的最终增量构建成功，AP/CP 合包通过。
  固件沿用当前用户配置：中文、HTTP、视频与 UI 动画开启。生成配置、ELF 端点及新增状态
  文字已核对，固定 `online/standby` 文字不再链接。
- 使用真实目标 view、字体及锁定 AVDK LVGL 9.5 主机库，中文、英文、关闭动画三种配置
  均通过 `-Wall -Wextra -Werror` 与 ASan/UBSan。覆盖真实状态、VP 与阶段变化、阶段清空、
  未知位、运行状态/页面不一致、借用内容修改、250 ms 过渡期间连续更新、SLEEP override、
  恢复页去除旧会话字段及每种配置 25 次创建销毁。LeakSanitizer 因环境限制关闭，不计通过。
- 实际 LVGL 渲染输出已检查，最长 `> state: in_conversation | vp:pending` 为 333 px，
  可放入现有 366 px 文字区域。字体覆盖检查通过，仍为 152 个操作引导字符。
- 两仓库 `git diff --check` 通过。旧边界检查器仍固定过期 SDK/AVDK 规则与旧板卡 LCD
  引脚，并扫描本地参考资料；执行失败，未计为通过，本次不更新该历史检查器。

| 区域 | 当前中文固件 |
| --- | ---: |
| AP Flash | 1,895,676 字节 |
| AP 静态 RAM | 155,140 字节 |
| AP DTCM | 35,744 字节 |
| CP Flash | 1,295,948 字节 |

原工程 `package/all-app.bin` SHA256：
`d647dd49da2699644594ea50db51140211cc41673c4c6e9cc6789eca6bb45822`。
构建、主机回归和实际 LVGL 截图位于 `/tmp/mybot-ui-live-state.IU4Lg0/`。
本次真机 RTM 状态、挂断、断网重连和长按恢复效果仍待用户烧录验证。

## 2026-10-10 四行终端与完整 23 表情

恢复模拟器 `drawGeekFace()` 的四行文字，沿用前一版 9/16 比例、Terminal 15/16 字体和
37 像素源坐标行距；光标移到第四行末尾：

```text
> OIM / R2
> mood: neutral
> state: online
> agora / realtime
```

`mood` 随表情更新为小写名称；仅 SLEEP 的 `state` 为 `standby`，这些字段为模拟器装饰。
新增独立 `include/bk7259_expression.h`、名称转换模块和显示所有者的 set/clear 请求，补齐
全部 23 个表情。默认 READY 为 NEUTRAL，会话为 GREETING；其余 21 个供后续调用，不自动
轮播，没有本次 MCP/RTM 命令或 UART/舵机集成。流程页面切换清除表情覆盖，仅 indicator
更新保留覆盖；配网、配对和恢复出厂提示优先。

渲染使用 66 个固定对象、两组形状和一个定时器，保留 250 ms 过渡。EXCITED 的眼睛跳动、
THINKING/BORED 的横移、PROCESSING 的眼睛运动、SLEEPY 的哈欠、SLEEP 的呼吸以最高
20 fps 更新；静态表情只更新 500 ms 光标。颜色、低亮度和短暂入场闪亮按 geek
路径配置，没有套用 character 路径的眨眼、瞳孔或抖动。LISTENING 下移 18 个源坐标像素，
SAD/SLEEPY/APOLOGY 下移 28；CURIOUS/CONFUSED/PLEADING 的整体 8° 倾斜暂未实现，避免
整屏旋转图层。这是已记录的参考姿态差异。

最初的三角扇填充在实际 LVGL 中出现内部抗锯齿接缝，已改成五块只读 A8 几何蒙版，
合计 13,631 像素字节；生成脚本和固定参考 SHA 见 `display/SOURCES.md`。低亮度曲线先与
背景预混颜色再绘制，避免半透明圆头/线段重复叠加导致亮度不均。最终星形、心形和椭圆
实绘填充平整，困倦/休眠线条亮度均匀。没有新增 UI 任务、整帧 canvas、扫描缓冲或整组
透明度/旋转图层。哈欠只缩放小型椭圆蒙版，仍可能使用 LVGL 的临时条带内存；运行时总堆
峰值与栈高水位尚需实板测量。ARM ELF 中 `View` 为 2408 字节。

验证与产物：

- 实际锁定 LVGL 的中文、英文、关闭动画配置通过 ASan/UBSan；覆盖全部 23 表情的六个
  时间采样、完整四行标签、名称转换与非法参数、重复请求不重播、indicator 不覆盖表情、
  流程默认映射、配网/配对/恢复页面、借用文本复制和 25 轮创建/销毁。关闭动画同时通过
  `-Wall -Wextra -Werror`，所有表情保持静态。LeakSanitizer 因环境限制关闭。
- 从当前 LCD 源提取实际请求函数，用 HAL stand-in 验证 23 个编号、锁失败、生命周期拒绝、
  最后请求合并、重复请求去重、内容复制、流程/indicator 区别和恢复出厂优先级，ASan/UBSan
  通过。该项验证软件快照语义，不代替实际 RTOS/LCD 并发或硬件验证。
- 本地 Chrome 执行原始模拟器 Canvas 代码，按相同缩放和已采用的 Y 位移生成参考。
  稳定 1000 ms 采样的 23 个表情中，脸部与眼睛/嘴的亮像素轮廓双向均落在 2 像素邻域内；
  背景、字体栅格、RGB565 量化、20 fps 采样和上述 8° 差异不作逐像素一致声明。
- 原工程中文 AP/CP 编译与合包、独立英文 `scripts/build_all.py --language en-US` 通过。
  最终原工程仍为中文，动画与视频启用。字体覆盖、蒙版复现和 `git diff --check` 通过。
  旧边界检查器仍因过期 AVDK gitlink 规则失败，未计为通过；本次没有修改该检查器。

| 区域 | 中文 | 英文 |
| --- | ---: | ---: |
| AP Flash | 1,894,132 字节 | 1,894,100 字节 |
| AP 静态 RAM | 155,020 字节 | 155,020 字节 |
| AP DTCM | 35,744 字节 | 35,744 字节 |
| CP Flash | 1,295,948 字节 | 1,295,948 字节 |

中文原工程 `package/all-app.bin` SHA256：
`749783e211b8ea6e25f37c7af1dadaa9777a6d605f586685eb8aec8dc539116d`；
独立英文固件 SHA256：
`a1bf3eaaf3481dbc40debaf8e41d56c8302c5b1766d40b22ea4463b7178cd106`。
构建和 LCD 请求检查记录位于 `/tmp/mybot-expression-ui.79iyVU/`；主机验证、23 表情总览和
参考对照位于 `/tmp/mybot-pixel-expressions-host.pmCYGH/`。AVDK、SDK/AOSL 和用户任务前的
RTM 日志修改保持不变，合同与依赖基线沿用下方记录。上述本机构建保留了该 RTM 日志
调整，本次 UI 提交仅包含显示源码、资源、配置和文档，该用户调整仍留在工作树。
基础像素 UI 已由用户确认实板渲染正常，四行文字和新增 21 表情仍待此次烧录验证。

## 2026-10-10 实板反馈后的比例与字体校准

用户确认首版可正常渲染，但图案大小和文字位置与模拟器不协调。进一步测量发现：首版
512x448 安全区把镜框压到屏宽的 48%，Unscii 16 的字符 advance 则是 16 像素，终端行
宽达 288 像素，约为参考缩放文字的两倍；`</>` 还被容器居中偏移。首版的 2 像素脸部
轮廓比较只证明选定缩放后的图形接近，不代表整体占屏和文字排版一致。

校准后，虚拟画布为 576x504，原点 `(32,-12)`，按 9/16 统一缩放，裁掉上下空白。
镜框实际宽从 307 增至 345 像素；眼睛、嘴和线宽使用同一比例。品牌文字恢复源坐标
`(104,108)` 和 `(104,145)` 的行网格，`</>` 在 `(795,112)` 左对齐，光标跟随实际字宽。

终端使用由固定 DejaVu Sans Mono Bold 输入生成并重命名的 BK7259 Terminal 15/16：
15 像素 ASCII 子集含 95 字符、4479 字节 bitmap；16 像素符号子集含 3 字符、155 字节
bitmap，合计 4634 字节。采用 15 像素，是因为参考 26 像素按 9/16 投射为 14.625 像素，
而 LVGL 会把 fractional advance 四舍五入；实测 15 像素的 9 像素字宽更接近参考。
字体输入 SHA 和 Bitstream Vera 许可记录在 `display/SOURCES.md` 及
`display/assets/licenses/TERMINAL_FONT_LICENSE.txt`，生成脚本为 `display/generate-pixel-font.py`。
中文操作字库仍为 152 字符、18,458 bitmap 字节，没有新动态字体解码或显示缓冲。

| 元素 | 校准后 LVGL 宽度 | 模拟器投射宽度 |
| --- | ---: | ---: |
| `> OIM / R2` | 90 像素 | 88.05 像素 |
| `> agora / realtime` | 162 像素 | 158.49 像素 |
| `</>` | 30 像素 | 28.45 像素 |

品牌和终端行的宽度误差约 2.21%，符号误差约 5.46%；字形起点、光标、行高和所有可见
标签边界均验证完整。脸部轮廓与新的参考投射双向均落在对方的 2 像素邻域内。
正常待机/会话状态、indicator 忽略、过渡、光标、双语操作引导和恢复出厂优先级保持原语义。
中文、英文、关闭动画三种实际 LVGL 配置再次通过 ASan/UBSan、最大借用内容复制和
25 轮创建销毁；关闭动画还通过 `-Wall -Wextra -Werror`。LeakSanitizer 保持关闭。

最终中英文 AP/CP 编译与合包通过，原工程最后保留中文固件。ELF 确认链接 Terminal 15/16，
不再链接 Unscii 16；AVDK、AOSL、SDK 和任务前用户 RTM 日志改动均未修改。

| 区域 | 中文 | 英文 |
| --- | ---: | ---: |
| AP Flash | 1,872,884 字节 | 1,872,868 字节 |
| AP 静态 RAM | 153,244 字节 | 153,244 字节 |
| AP DTCM | 35,744 字节 | 35,744 字节 |
| CP Flash | 1,295,948 字节 | 1,295,948 字节 |

中文 `package/all-app.bin` SHA256：
`19eff45485a5c1efcc29afbfe528f8a73274af9ec7be3d70efe7df8264a9ccfa`。
构建记录位于 `/tmp/mybot-pixel-ui-layout.ApIDzc/`，最终截图、前后对比和字体实测位于其
`host-validation/` 子目录；首版记录完整保留。校准版在该阶段尚未完成用户真机验证，
也尚未单独提交。

## 2026-10-10 像素眼镜 UI 重写（首版）

本次基于 Solution `2fe505913007fcfd5be64d83159d2dc514a076ac`，在
`release/v4.0.1-mybot-r2` 工作树实现。重写 `display/lvgl_view.cc/.h`，更新字体、组件源列表、
动画配置和双语文档。AVDK 固定为 `90aaf113d78ef875635fcc49587f80fd2a8ddadc`，跟踪文件干净；
LCD 驱动、显示任务、AOSL、AEC、按键和恢复出厂执行逻辑没有修改。

视觉依据为 `mcp-resource/threejs-project` 的 `process` 皮肤与 `geek` 表情画法：

| 场景 | 显示 |
| --- | --- |
| SDK READY | 米白 `#f1f1ee` 镜框、双弧眼、微笑 |
| SDK IN_CONVERSATION | 暖金 `#f1ca79` 镜框、左弧眼、右圆点眼、微笑，保持到会话结束 |
| 配网、配对、错误、恢复出厂 | 同风格中英文操作引导，保留热点、门户入口与网页控制台配对码 |

正常界面保留两行静态终端装饰、`</>`、500 ms 光标，取消 mood/state、声纹、聆听/思考/
回复指示及通知。仅 indicator 更新不会改变表情或重启 250 ms 过渡。恢复出厂提示优先级
保持最高。1024x896 坐标等比缩为 512x448，居中于逻辑屏幕 `(64,16)`，保留圆角安全区。
初版深色渐变在 RGB565 实绘中出现色带，最终使用 `#050708` 固定黑底和灰色装饰。

渲染使用 33 个固定 LVGL 对象、一个定时器和 408 字节曲线点数据；32 位目标的 View 为
636 字节。过渡时每 50 ms 更新，静态表情只有光标每 500 ms 翻转。没有新任务、canvas
整帧缓存或组透明度图层；已有双扫描缓冲 1,228,800 字节、条带 20,480 字节和 UI 栈
8,192 字节继续复用。旧 emoji 源文件和许可记录保留，但 `lvgl_assets.c` 不再参与链接。
终端使用内置 Unscii 16；操作引导的 MyBot UI Sans 子集为 152 字符、18,458 bitmap 字节，
来源字体 SHA256 和生成参数保持固定。

验证了锁定 `main=cfd178a` 的 PORTING/EMBEDDED 和 LCD 平台合同、项目构建规则及 LVGL 9.5
line-point 借用、定时器与字体接口。用户要求隐藏指示，因此本次只调整平台显示消费方式，
不修改 SDK 的声纹/服务端状态协议或生命周期。SDK 保留任务前已有的两处 RTM 日志 NOTICE
改动：当前 include/src 摘要为 `fa13a414fb688c92738c4643363cea77a72b723b7e53da4a78fc23dc1354e5a4`。
在临时审计副本中去除该用户补丁后，摘要为 revision lock 中的
`bd59fb9a4f7aced74d5b8e770da869897229316c284e4ccded4d2aa8e667fa7a`；本任务未修改 SDK 源或锁。
AOSL 摘要与其锁一致，RTM 文件哈希与任务前一致。

真实 LVGL 主机验证使用 640x480 RGB565 和 16 行条带，直接编译本次 view、字体及锁定 AVDK
LVGL，适配仅限临时主机内存/断言后端：

- 中文、英文动画配置和英文关闭动画配置均通过 ASan/UBSan；关闭动画还通过
  `-Wall -Wextra -Werror`。LeakSanitizer 因 ptrace 环境关闭，不作其通过结论。
- READY → IN_CONVERSATION → READY、过渡期间及之后的 indicator 更新、500 ms 光标边界、
  所有操作与恢复页面、固定文本高度/安全区域、最大 SSID 和配对码的借用内容复制均通过。
- 每种配置 25 次创建/销毁后对象与 timer 回到初始显示基线；恢复页面不被 SDK 更新覆盖。
- 使用本地 Chrome 直接执行模拟器原始 Canvas 绘图，输出两张参考表情。脸部亮像素轮廓
  双向均位于对方的 2 像素邻域内；终端文字精简、Unscii 字号和固定黑底为产品适配，
  不宣称整幅截图逐像素一致。

中英文 AP/CP 构建和合包通过。中文在原工程 clean/build，最后再次构建最终纯黑背景版本；
英文通过独立项目 `scripts/build_all.py --language en-US`。最终中文开启 GC9503CV、视频和
动画，ELF 已确认链接 Unscii 16、不再链接旧 emoji；未改屏幕方向或音视频参数。

| 区域 | 中文 | 英文 |
| --- | ---: | ---: |
| AP Flash | 1,869,660 字节 | 1,869,596 字节 |
| AP 静态 RAM | 153,244 字节 | 153,244 字节 |
| AP DTCM | 35,744 字节 | 35,744 字节 |
| CP Flash | 1,295,948 字节 | 1,295,948 字节 |

中文 `package/all-app.bin` SHA256：
`84413db6531549892a4877ebe0f0733e23d2431dd9af09673d6543c96a71a51b`。
相对前次 SDK 同步中文构建，AP Flash 减少 71,440 字节，静态 RAM 增加 456 字节。
运行时堆峰值、线程栈高水位、实际刷屏负载仍需上板测量。

临时构建/审计记录位于 `/tmp/mybot-pixel-ui.K9J0v2/`，主机截图、回归与模拟器对比位于
`/tmp/mybot-pixel-ui-host.CqRsKv/`。`git diff --check` 和字体覆盖检查通过；旧边界检查器
仍使用过期版本、旧 LCD/配网规则并扫描参考资料，未计为通过。首版在该验证阶段尚未提交，
也未完成真机验证。烧录后检查待机/打招呼、挂断回待机、圆角可见区域、配网/配对、长按恢复出厂，以及
同时音视频运行时的光标和过渡效果。

## 2026-10-10 SDK main 同步与挂断顺序复核

远端 `main` 已确认并同步到 `cfd178a02fa94775cf01dc77e344f4ce5930225c`（PR #109），
SDK 版本仍为 1.2.0。整套 56 个 `include/src` 文件从该提交导入；相对上一版 `0643978`
仅生命周期实现与内部结构变化，公开平台头文件没有变化。
`SDK_REVISION` 和第三方声明已更新，最终聚合 SHA256 为
`bd59fb9a4f7aced74d5b8e770da869897229316c284e4ccded4d2aa8e667fa7a`。

唯一保留的目标补丁是既有 HTTPS 请求/响应 body 日志；RTM 日志等级调整已由用户回退，
`src/rtc/mybot_agora_rtc.c` 与上游逐字一致。AVDK、AOSL、RTSA、R2 板级配置和平台适配
没有修改。本次同步基于 Solution `61cfb71ec1c87b8f1193d24dd137d4dd03cf5344`。

SDK 现在先停止视频、退出 RTC/RTM、清理会话 PCM 并恢复 READY，再用原会话 ID 和停止
原因发送一次 HTTPS 停止通知。通知失败只记录 warning，不重试，也不会重新开启媒体。
断网/网络代次变化的响应仍会过滤，设备鉴权被拒绝时仍重新配对。
HTTP 仍在控制线程同步执行：本地音视频和 LCD 不再等待远端响应，但紧接着开始新会话或
调用 `mybot_stop()` 仍可能等待正在执行的通知请求。远端自动结束依赖设备服务支持设备
离开 RTC channel 后终止会话。

验证结果：

- 已核对该提交的完整 PORTING/EMBEDDED 文档、涉及的平台契约及 BK7259 RTSA/RTM 参考。
  单键状态查询、LCD 挂接、双麦 ADC2 AEC、视频停止顺序和恢复出厂均无需新增平台改动。
- 最终带 HTTPS 日志补丁的 vendored SDK 使用 video=ON、cloud AEC=OFF、60 ms ptime，
  上游既有 22/22 主机测试通过，覆盖本地停止先于 HTTP、单次通知、重复请求、断网、
  重新配对及 app/RTC/media。19 个单元测试和公共头文件测试使用 ASan/UBSan；两个 CMake
  集成测试按上游配置关闭 sanitizer。LeakSanitizer 因 ptrace 环境限制关闭，不计为通过。
- 中文原工程执行 clean/build，再核准回退 RTM 日志后的最终增量构建；英文使用独立工程
  `scripts/build_all.py --language en-US`，两者 AP/CP 编译与合包通过。
- 最终中文配置为 `CONFIG_MYBOT_LANGUAGE_ZH_CN=1`、`CONFIG_MYBOT_VIDEO=1`；ELF 为 ARM32、
  小端、EABI5 hard-float。`git diff --check`、快照路径集合、实际摘要和声明补丁复核通过。
- 旧边界检查器仍使用过期 SDK/AVDK 锁及旧 LCD/配网假设，并扫描参考资料，未计为通过。

| 区域 | 中文使用量 | 英文使用量 |
| --- | ---: | ---: |
| AP Flash | 1,941,100 字节 | 1,941,060 字节 |
| AP 静态 RAM | 152,788 字节 | 152,788 字节 |
| AP DTCM | 35,744 字节 | 35,744 字节 |
| CP Flash | 1,295,948 字节 | 1,295,948 字节 |

中文版 `package/all-app.bin` SHA256 为
`9a7d22fe63d7ed6c064c30e1dfaa58ae316279ad474e74a53e97faa0c3fe42db`。
本机构建、主机测试及检查日志保留在 `/tmp/mybot-sdk-refresh.6Dj0Bx/`，不属于源码或构建
输入。尚未对这版固件完成真机验证；烧录后应检查松手至本地音视频停止的耗时、屏幕保持
点亮、连续开关会话、离线挂断和长按恢复出厂。

## 2026-10-10 R2 与恢复出厂复核

功能验证基线为 Solution `0ebb7f5821cb5084f39038961e0ebd5ad50816d1`，父仓库 `ffb9c3a`
已同步其 gitlink，均在 `release/v4.0.1-mybot-r2` 分支。AVDK 固定为
`90aaf113d78ef875635fcc49587f80fd2a8ddadc`，包含 GC9503CV R2 屏驱动，工作树干净。
SDK 固定为 `064397882187a242be25a525a91df83d353b78bb` 加已声明的 HTTP 日志补丁；
SDK/AOSL 摘要与各自 revision lock 一致，恢复出厂功能未修改这两个快照。

当前显示和生命周期与下方历史配置的差异：

- GC9503CV MIPI 原生 480x640，逻辑 RGB565 640x480；GPIO20 控制 3.3V，GPIO29 控制
  LCD/摄像头共用 2.8V，GPIO42 复位，GPIO40 背光低有效。
- 两块扫描缓冲共 1,228,800 字节，一块 640x16 绘制条带为 20,480 字节，UI 栈仍为
  8,192 字节；对象与绘制任务的动态分配峰值仍需在目标机测量。
- 显示持有共用 2.8V 电源到完整平台关闭，视频停止只复位摄像头，避免结束会话后黑屏。
- GPIO47 短按在松手时切换会话，长按 5 秒请求恢复出厂；按键准备时已按住须先松手再按。
  产品任务在联网前检查持久恢复意图，并在 SDK 与网络停止后清理本地鉴权、Wi-Fi 和音量。

恢复页面由既有显示任务和视图渲染。LCD 快照将产品恢复状态设为优先覆盖，SDK 的
STOPPING、render 或解挂不会替换恢复提示；没有新增任务、UI 定时器或帧缓冲。

| 阶段 | 中文标题 / 提示 | 英文标题 / 提示 |
| --- | --- | --- |
| 恢复中 | 正在恢复出厂设置 / 请勿断电 | Restoring factory settings / Keep power connected |
| 失败 | 恢复失败，请长按重试 / 长按5秒重试 | Reset failed. Hold 5s to retry / Hold 5s to retry |
| 重启中 | 恢复完成，正在重启 / 请勿断电 | Reset complete. Restarting / Keep power connected |

20 像素字体含 170 个字符，覆盖固定中文恢复提示，bitmap 共 22,018 字节。
`display/generate-lvgl-font.py` 固定源字体 SHA256，许可和来源记录在 `display/SOURCES.md`。
根目录可执行以下命令检查 UI 文案覆盖，无需源字体或 Pillow：

```sh
python3 bk_solution_ai/components/mybot/mybot_sdk/platforms/bk7259/display/generate-lvgl-font.py --check
```

本次中英文 AP/CP 构建和合包通过。中文版使用原工程 `make`，英文使用
`scripts/build_all.py --language en-US`；该脚本的中文地址校验仍是旧服务地址，未将
默认双语脚本的当前结果记作通过。中文原工程产物为 `package/all-app.bin` 与
`package/app_pack.rbl`，命令和路径见 [README_CN.md](./README_CN.md)。

| 区域 | 中文使用量 | 英文使用量 |
| --- | ---: | ---: |
| AP Flash | 1,941,652 字节 | 1,941,612 字节 |
| AP 静态 RAM | 152,796 字节 | 152,796 字节 |
| AP DTCM | 35,744 字节 | 35,744 字节 |
| CP Flash | 1,295,948 字节 | 1,295,948 字节 |

恢复功能的主机验证包括：

- 9 组真实按键源码手势用例，覆盖松手短按、消抖、5 秒边界、单次长按、延迟扫描、
  开机已按住、时钟回绕、未挂接 SDK 和恢复优先级，ASan/UBSan 通过。
- 产品恢复流程用例覆盖顺序、持久化/SDK/网络/删除失败、新请求和开机续做。
- 31 组持久化故障注入，以及未修改 EasyFlash 源码配合 NOR 模型的 6 组验证。
- 正常配网提交、9 个取消时机及 AP/扫描/STA 停止失败后重试。
- 字体覆盖、最终 `bk7259_*` 接口命名、`git diff --check`、SDK/AOSL 摘要和 AVDK 状态复核。

用户已反馈 R2 屏幕、双模拟麦上行与 AEC、结束会话后屏幕保持点亮、长按恢复出厂真机测试
通过。断电续做、Flash/驱动故障和长时间稳定性仍需专项真机验证；初始 UI 的截图与内存
测试是下方保留的历史结果，本次没有把它们当作已在 R2 全量重跑的结果。

持久恢复的边界：意图由产品任务开始处理请求时才写入，提前断电可能丢失尚未消费的
内存请求。EasyFlash 公共查询不能区分所有物理读取失败或冷扫描 CRC 损坏与不存在；
最终标记删除报错时尝试重建，若重建也失败则不能保证意图保留。真实 EasyFlash 主机
验证观察到既有 `ef_env.c:473` 有符号移位 UBSan 报告，未修改 vendor 源码。
旧边界检查器仍因过期版本、LCD/配网假设及参考资源扫描而失败，未计为通过。

本机验证日志和临时测试保留在 `/tmp/mybot-factory-reset.TPTdGu/`、
`/tmp/mybot-reset-storage.mwz2VO/`，不属于源码或构建输入。

## UI 移植时的基线与契约

本节保留当时的验证版本。后续 SDK 更新以 `components/mybot/mybot_sdk/SDK_REVISION` 为准。

- AVDK：`239c151bf4ba239bf8ebc1a178df0a8b659b55bd`，工作树保持干净，使用其中的 LVGL 9.5.0。
- Solution：`9cbfb976acdf043b01dde91daf1889a54e66d122`，改动留在工作树，未提交或更新根 gitlink。
- ESP32 视图来源：`19af3bdfaf1dd35461af48def8f9558e3503e755`；未修改参考工程。
- MyBot SDK：`4ae239c804257f8b5c557e5879b54d9a88d80847`，RTC 日志级别和 AOSL 恢复已包含在
  上游快照，HTTPS body 日志仍由 `SDK_REVISION` 记录。`include/src` SHA256 复核为
  `84a5a0a015de7be6a91658ebc3de1f6958a8e595750d859426dd3a7dae1aa76c`。
- AOSL 基线：`84e086084ebcd0ae2455a0ce5721950c5fe2e656` 加 `AOSL_REVISION` 已记录的 BK7259 补丁。
  内容 SHA256 复核为 `2e9fc3fa5571972fd31c1ab64546477a98550b6b210e38fec181c0f4daeae64e`。
- 既有 RTSA 包：BK7259 v1.10.1 build 1278380，未更换；完整构建保留其头文件和库的哈希校验。
- 已核对锁定 SDK 的完整 `docs/PORTING.zh-CN.md`、`docs/EMBEDDED.zh-CN.md` 和
  `include/mybot/platform/mybot_lcd.h`，以及 BK 的显示、DPU、帧分配和 RTOS 接口。
  SDK 输入只按平台头文件消费；SDK 销毁只解挂，产品显示持续到平台关闭。

## 2026-09-23 SDK 同步复核

- 通过远端 `main` 确认最新提交为 `83fbcb0969da4c73a5912326d699f90ec634b28e`，版本号仍为 1.2.0。
  全量同步 56 个 `include/src` 文件；唯一目标补丁仍为原有 HTTPS body 日志，补丁内容不变。
  新聚合 SHA256：`3f89476f880518b0f0d68e45d709023a0cb0d6146865d8e32b8a95ceb4e46b24`。
- 此次修改基于 Solution `d717cef1879695cafdc83c436c7abeda692e4196`，未自动提交。
  AVDK、AOSL 和 RTSA 仍使用本记录中的固定版本；AOSL 内容与 RTSA 头文件/静态库摘要复核一致。
  BK7259 平台、圆角 UI 和 UID 设备 ID 源码未改变，无新增 GCC 原子接口依赖。
- 核对最新 `PORTING.zh-CN.md`、`EMBEDDED.zh-CN.md` 和应用生命周期、音频、视频、Wi-Fi、
  LCD、提示音公开头文件。视频控制改由 SDK 控制线程串行执行，提示音资源保留到 RTC 回调
  排空后销毁；现有平台 ops 和目标 CMake 源文件列表无需调整。
- 在隔离宿主目录以 `MYBOT_ENABLE_VIDEO=ON` 构建并运行现有 `ctest`，21/21 通过。
  测试提交 `655bf9c5aa53ea90561b0ad61af0ddd97a1d9c32` 与最终远端合并提交的完整 Git tree
  均为 `7173ad69ca9348319a8131a57053bd9bdb2381f9`。覆盖视频启停/回调、提示音 stop/destroy、
  RTM 三状态/声纹以及设备生命周期和 JSON；这些宿主测试使用上游源，不包含目标 HTTPS 日志补丁。
- 原工程执行下述 clean/build 命令，带目标补丁的 AP/CP 完整编译与合包通过。
  AP Flash 1,926,024 字节（41.00%），AP 静态 RAM 152,900 字节（38.88%）；
  CP Flash 1,296,900 字节（89.95%）。ELF 为 ARM32 小端 hard-float，无未解析符号。
  `all-app.bin` 为 3,433,376 字节，`app_pack.rbl` 为 2,027,888 字节。
- 逐文件验证确认除声明补丁外与上游完全一致，`git diff --check` 通过。
  旧边界检查器仍因固定旧版本和“不使用 LVGL”的旧规则失败，未将其记作通过。
  Kconfig 和链接器既有警告仍存在。此次未重新执行 UI 宿主截图测试或实板音视频验证。

## 初始 UI 移植构建（历史记录）

在根目录执行，第一次启用 LVGL 必须清理旧生成配置：

```sh
env CCACHE_DISABLE=1 make -C bk_solution_ai/projects/mybot clean SDK_DIR="$PWD/bk_avdk_smp"
env CCACHE_DISABLE=1 make -C bk_solution_ai/projects/mybot bk7259 SDK_DIR="$PWD/bk_avdk_smp"
```

AP、CP 完整构建和合包通过，最终增量构建也通过。输出位于
`projects/mybot/build/bk7259/mybot/package/`：`all-app.bin` 用于完整烧录，
`app_pack.rbl` 为 OTA 包。ELF 为 ARM、32 位、小端、hard-float ABI。

| 区域 | 使用量 | 分区占用 |
| --- | ---: | ---: |
| AP Flash | 1,926,128 字节 | 41.00% |
| AP 静态 RAM | 152,908 字节 | 38.89% |
| CP Flash | 1,296,900 字节 | 89.95% |

相对移植前记录，AP Flash 增加 324,144 字节，静态 RAM 增加 792 字节。
运行时另需双扫描缓冲 492,800 字节、绘制条带 12,320 字节和 UI 栈 8,192 字节。
最终链接未引入 vendor LVGL 显示线程、FreeRTOS 绘图线程或 GPU 绘制后端。
工具链仍报告既有 `_gettimeofday` stub、GNU-stack 和 RWX 段警告，不能视为零警告构建。

## 初始 UI 宿主验证（历史记录）

- 真实 LVGL 9 渲染：全部 10 个页面、中英文、深浅主题、三种会话状态、长配对码与
  SSID 滚动、借用输入立即改写、100 次创建/销毁；已检查截图布局。
- 直接包含实际 `bk7259_lcd.c`，替换 HAL/RTOS/LVGL 调用的后端测试：11 组通过。
  覆盖分条/局部旋转和未修改像素保留、双帧占用超时、补绘、提交失败的不同释放语义、
  退出与 deinit 失败后的资源保留和重试、回调退出屏障、快照复制与解挂、首帧失败、
  重新 prepare，以及分配溢出与 OOM。
- 两组测试启用 ASan/UBSan；后端还启用 `-Wall -Wextra -Werror`。
  LSan 因沙箱 ptrace 限制未启用，不作其检测通过的结论。
- 64 位宿主的 LVGL 分配跟踪：创建后 7,286 字节，首次渲染后 7,054 字节，
  真实 UI 工作负载峰值 9,499 字节；三种动画各运行 60 秒，未观察到保留分配增长。
  100 次创建/销毁后基线稳定，最终 `lv_deinit` 后跟踪分配归零。
  这些数值不含帧缓冲、条带、线程栈和分配器开销；宿主使用 24 行条带，目标为 16 行，
  且指针宽度不同，不能替代目标机内存峰值测量。
- LVGL 内部仍会为每次绘制分配/释放任务，动画测得约 240–370 次/秒，单次最大请求
  304 字节；自有动态标签使用固定缓冲。64 KiB 配置只限制绘制层像素，不是 UI 总堆上限。

本机宿主测试及日志保留在 `/tmp/mybot-lvgl-view-test.f9A5v9/` 和
`/tmp/mybot-lcd-regression.M0abjc/`；这些是临时验证文件，不属于固件构建输入。

## 适配边界与上板检查

显示原子变量全部走 BK7259 AOSL HAL。两帧仅由释放回调归还；释放无法确认时保留资源。
若驱动在接收帧前失败又不回调，保守保留可能要求重启，不能擅自假定该帧已释放。
UI 工作线程退出、显示关闭和回调等待均有超时；通知信号量按进程生命周期复用。

锁定 LVGL 的默认绘制分配在 HSRAM 耗尽时回退 PSRAM，但配对释放仍为 HSRAM。
适配层在初始化后只替换默认、字体、图片三组 handler 的分配函数，禁止该回退。
为保留原有像素格式复制、对齐和 stride 回调，使用固定 LVGL 9.5 的
`src/draw/lv_draw_buf_private.h` 访问 handler 字段；升级 LVGL 时需复核此处。

`git diff --check` 通过；SDK/AOSL 摘要和 vendor 干净状态单独复核通过。
旧 `skills/mybot-development/scripts/check_port_boundary.sh` 已运行但未通过：
它仍锁定旧版本/摘要和“不使用 LVGL”的旧方案，亦拒绝既有集成依赖。
本次未修改该本地检查器，也不将它的结果记作通过。

初始 UI 移植时尚未连接实板；当前 R2 功能通过情况见 2026-10-10 复核。
后续回归应覆盖屏幕方向与颜色、中英文恢复提示、配网/配对、listening/thinking/speaking
与声纹通知、反复停止/启动 SDK、恢复时断电和失败重试，以及同时音视频时的动画流畅度、
HSRAM 余量、线程栈高水位和长时间运行情况。
