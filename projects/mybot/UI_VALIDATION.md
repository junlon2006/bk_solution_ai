# BK7259 LVGL UI 验证记录

本文保留原始 LVGL 移植的历史验证记录，并补充当前 R2 硬件与恢复出厂功能的验证。
状态视图来源于 `mybot-esp32`，默认中文、深色、启用状态动画。
实现位于 `components/mybot/mybot_sdk/platforms/bk7259/{bk7259_lcd.c,display/}`；
项目配置为 `ap/lv_conf_custom.h` 和 `ap/config/bk7259_ap/defconfig`。
SSID 由配网模块复制投递。SDK 的 `include/src` 快照已单独同步到锁定版本；AOSL 和 AVDK 源码
未因原始 UI 移植修改；后续 R2 屏驱动版本见下节。
旧直接渲染字体及其独立许可证已删除，可从 Git 历史恢复；新资源许可见
`components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md`。

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
