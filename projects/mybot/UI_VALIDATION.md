# BK7259 LVGL UI 验证记录

本次在原工程接入 `mybot-esp32` 的语义状态视图，默认中文、深色、启用状态动画。
实现位于 `components/mybot/mybot_sdk/platforms/bk7259/{bk7259_lcd.c,display/}`；
项目配置为 `ap/lv_conf_custom.h` 和 `ap/config/bk7259_ap/defconfig`。
SSID 由配网模块复制投递。SDK 的 `include/src` 快照已单独同步到锁定版本；AOSL 和 AVDK 源码
未因本次 UI 移植修改。
旧直接渲染字体及其独立许可证已删除，可从 Git 历史恢复；新资源许可见
`components/mybot/mybot_sdk/platforms/bk7259/display/SOURCES.md`。

## 基线与契约

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

## 构建

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

## 宿主验证

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

尚未连接实板验证。烧录后重点检查：屏幕方向和 RGB565 颜色、中英文切换、配网/配对、
listening/thinking/speaking 与声纹通知、反复停止/启动 SDK，以及同时开启音视频时的
动画流畅度、HSRAM 余量、线程栈高水位和长时间运行情况。
