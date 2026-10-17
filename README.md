# CI130X SDK 简介

## 概述

CI130X SDK 适用于CI130X系列芯片

官方文档：
https://www.chipintelli.com/zh-cn/page/151.html

## SDK目录结构

| 目录       | 描述             |
| ---------- | ---------------- |
| .vscode    | 编译器配置文件   |
| components | 组件             |
| driver     | 驱动             |
| libs       | 库文件           |
| projects   | 示例工程         |
| startup    | 启动文件         |
| system     | 系统文件         |
| tool       | 固件构建升级工具 |
| utils      | 调试帮助工具     |

## 资料查找

编译环境安装说明、芯片手册、开发板资料等，详见启英泰伦语音AI平台网址：https://aiplatform.chipintelli.com

## Windows PowerShell 构建

CI 本地唤醒 ding（`voice_id=1000`）的 PCM 增益调整为当前 ESP 对话增益的 50%，
其余 CI 本地播报保持 60%。每段音频的比例随播放请求传入播放器 task，支持组合
播报与排队抢占，不改全局用户音量或 DAC/MIC/REF 增益。50 档时对话幅度为 20%，
唤醒音为 10%；ESP 内置播报的 50% 比例由 ESP 固件另行实现，两端都需用户更新。
实机先在 1、50、100 档分别唤醒再听正常回复；CI 应出现 `[AUDIO] local playback
volume`、`gainRatioPercent=50`（50 档时 `pcmGainPermille=100`），ESP 内置播报看
`firmware prompt playback started` 的同一比例。会话内重复唤醒的 ding 抑制规则保留；
完整响度、安静播放、真人打断与下一轮恢复仍需实机验证。

ESP 内置播报与 AI 回复反复爆音的现场原因尚未确认。诊断代码在现有下行 owner
复制左槽 PCM 时采集峰值、非零样本 FNV-1a 摘要与左右槽差异，STOP/FINISH 后打印
`[CI_PCM] stage=dac_queued`；不会调整样本、音量、时钟或播放所有权。用户更新两端
后，只对无中断、无 TX 错误且 `finish complete` 的同次播放，与 ESP 的
`stage=tx_submitted` 对照 `nonzeroSamples/nonzeroHash/peak`，两端应一致且
`stereoMismatches=0`。不一致可定位数字链路差异；一致不能证明静音时长、DAC
供数或扬声器音质正确。主机检查命令：

```sh
python3 script/test_downlink_pcm_trace.py
```

本 SDK 的官方预编译算法库由 Nuclei GCC 9.2.0 生成，必须使用同版本工具链。当前构建日志中的 `D:\Project\gcc\bin` 是 GCC 10.2.0，会在链接 LTO 算法库时报 `generated with GCC compiler older than 10.0`。确认 `D:\Project\gcc\bin` 已换成 GCC 9.2.0 后，在 PowerShell 中依次执行：

```powershell
$env:PATH="E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\tools\build-tools\bin;E:\Project\toolchain-gd32v-v9.2.0-win\bin;$env:PATH"
cd E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\projects\bnpu_core_alg_pro\project_file
make clean
if ($LASTEXITCODE -ne 0) { throw "CI BNPU clean failed" }
cd E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\projects\offline_asr_llm_aiot_iis_sample\project_file
make clean
if ($LASTEXITCODE -ne 0) { throw "CI clean failed" }
make -r -j8
if ($LASTEXITCODE -ne 0) { throw "CI build failed; do not use the previous package" }
```

应用构建会先构建 `projects/bnpu_core_alg_pro`，成功后才生成 MCU 链接脚本、
构建应用并合并两核镜像。AEC 的 `ci_basic_alg.c`、`alg_preprocess.c` 属于 BNPU
算法核，仅重编 MCU 不能让它们生效。算法核失败时整个构建失败；必须等待完整构建
成功，不能把失败后仍留存的旧 `user_code.bin` 当作本次产物。

共享 UART/I2S 生成头文件位于 `components/ci_common/ci_conversation.generated.h`，
MCU 和 BNPU 都通过现有 `source_file.prj` 搜索此目录。该文件由 `protocols` 生成并
随 SDK 同步，Windows 构建无需另行手写常量或引用 MCU 应用目录；从旧版本更新时
须同步新增头文件，并移除旧 `app/app_main/ci_conversation.generated.h`。BNPU 的
PWK 与日志覆盖值放在 SDK 默认配置之前，避免头文件包含顺序引起宏重定义，原值
`USE_PWK=1`、`CONFIG_CI_LOG_UART=0` 保持不变。

旧流程直接拷贝 `libs/ci130x_asr_alg_bnpu.a`，该文件虽以后缀 `.a` 命名，实际是
算法核二进制。仓库原有版本没有包含源码中的 MIC/REF 独立增益修正。首次使用新
构建流程时必须重新生成两核镜像；不要直接使用此前打包的固件。

完成构建后，可在同一个 PowerShell 窗口核对算法核和最终合包均含修正后的日志标记：

```powershell
riscv-nuclei-elf-strings ../../../libs/ci130x_asr_alg_bnpu.a | Select-String -SimpleMatch '[AEC] fixed_gain'
riscv-nuclei-elf-strings ../firmware/user_code/user_code.bin | Select-String -SimpleMatch '[AEC] fixed_gain'
```

两处均应匹配；该检查只证明相关代码进入产物，不证明 AEC 效果正常。现有
`fixed_gain` 日志仅在关闭原本开启的 ALC 时输出；ALC 已关闭时没有这行，不能单凭
运行日志缺行认定增益修正未执行。

macOS 可运行构建依赖和 AEC 增益的主机回归，不生成可刷写固件：

```sh
python3 script/test_bnpu_build_dependency.py
python3 script/test_aec_codec_gain.py
python3 script/test_ci_build_headers.py
```

Windows 正式构建并更新设备后，保持原音量，只说一次“听得见吗”后安静等待，
服务端应仅处理这一次用户问题，不应把设备的回答识别为新的用户轮次。观察
`[AEC_PATH]` 的 `refPeak`、`refAbs` 和 `dstAbs`；若参考仍长期接近满幅或设备仍
识别自己的播报，需要继续采集 MIC/REF/DST 定位，不能以 `work_state=1` 认定消回声
成功。随后在播报中真人插话，应正常打断并识别完整问题；再验证连续对话和返回
待唤醒。`FINISH ... pcm_timeout` / 非零计数不一致属于下行完整性错误，应单独记录。

关闭捕捉
#define ASR_MIDDLE_RESULT_OUT_EN       0  

## ESP 重启与 I2S 恢复验证

`offline_asr_llm_aiot_iis_sample` 的掉线清理由现有系统消息任务串行执行。
心跳任务只提交合并的超时事件；清理期间不重新开放 PCM 生产。
外部 IIS0 从机 TX 先停止本通道 DMA、清除本通道中断、归还队列和硬件持有的缓冲区，
再由 codec owner 记录/完成 IIS 禁用。缺少 ESP BCLK/LRCK 时不等待寄存器装载，
时钟恢复后先完成旧禁用再启用 TX；不复位共享 IISDMA、ADC 或本地 DAC。
内部 DAC 的正常播放、FINISH 排空和 AEC 尾音流程保留。

上行启动恢复修改前 `895785f` 的硬件顺序：预填至少 11 帧后，先设置 DMA
地址并使能 DMA/中断，再提交 IIS TX 启用。`95a2be6`、`b9e6547` 中先等待
TX 装载完成才开 DMA 的顺序已修正。STARTING 期间 DMA 中断正常轮转缓冲区；
后续装载轮询不重复初始化 DMA，也不重新要求积满 11 帧。启动中断链仍按同一
取消路径归还缓冲区，保留无时钟时的非阻塞 OTA 恢复能力。

AIOT 音频数据会经核间 mailbox 回调进入主核，IIS 上行回调同时支持中断和任务
上下文。回调只同步复制 PCM，使用二值信号量及对应的 `FromISR` 接口与清理互斥，
codec 和队列初始化完成后才开放，不执行 IIS 启停，不保存算法输入指针。
现有心跳任务每五十毫秒检查尚未完成的上行启动，
只提交一份合并的启动事件；系统消息任务和 codec 状态负责首次预填、实际启动及
缺时钟时的后续重试，已消耗预填的 STARTING 不再被预填门槛拦住。
队列已填满、消息队列繁忙、旧握手已失效时仍能正确重试或丢弃过期启动工作。

此前 `start failed ... invalid_slave_output` 将非法配置和中断上下文混为一类。
连续出现该日志与 `no pcm buffer` 时，可能是启动在 mailbox 回调中被拒绝，导致
固定 PCM 池全部积压，不能据此判断堆内存耗尽。新代码区分 `interrupt_context`
与 `invalid_slave_output`；正常启动日志明确包含 `owner=system context=task`。

macOS 可执行主机模拟回归，不构建 CI 镜像：

```sh
python3 script/test_iis_peer_recovery.py
```

Windows 正式构建后，先在单台验证机检查：

1. 启动后先不唤醒，确认出现
   `[AI_I2S] uplink armed codec=0 dma=on order=dma_then_tx`，随后出现
   `[AI_I2S] continuous uplink started bufferedFrames=... owner=system context=task`，
   无持续 `start failed` / `no pcm buffer`。然后唤醒并说完整短句，联网后应有非空
   ASR 转写和语音回复；连续十轮验证回复播放、播放期打断与连续对话，音量和 AEC
   行为应与升级前一致。未配网开发板先用此启动日志验证 CI 本地上行启动。
2. 保持 CI 供电，让 ESP 单独重启；分别在空闲和播放后测试。CI 本地唤醒仍可用，
   UART PING 持续，日志依次出现 `peer timeout state=stopping`、
   `uplink canceled dma=off buffers=reclaimed`、`peer cleanup complete`。
3. ESP 恢复后出现 `peer ready`、`continuous uplink started`，再次唤醒可正常收音和播放。
   `buffer_ownership` 表示缓冲区所有权异常；停在 `state=stopping` 应检查清理 owner/锁；
   已 `cleanup complete` 但未恢复上行，应检查 BCLK/LRCK 和 IIS 装载状态。
4. 再验证 ESP OTA：下载校验后新版本启动，必须收到 CI PING 和本次启动的
   `FIRMWARE_INFO`，日志出现 `confirmed healthy`、`ciFirmwareObserved=true`，
   云端成功回执后仍需重新验证语音；出现 `ciReady=false` 优先查 UART，
   `ciReady=true` 但 `ciBootHealthReady=false` 优先查 CI 初始化/清理，
   Guru Meditation/WDT/Backtrace 则按 ESP 崩溃排查。

没有 `uplink armed` 时，检查 `peer ready`、预填生产和系统启动事件；有 `uplink armed`
但没有 `continuous uplink started` 时，检查 ESP BCLK/LRCK、IIS 装载状态和任务重试。
出现启动完成日志仍持续 `no pcm buffer` 时，检查 TX DMA 中断与缓冲区轮转；该日志
表示固定 PCM 池借不到缓冲区，不能单凭它认定堆分配失败或整颗 CI 崩溃。

失败时分别记录 CI 的启动/清理日志和 ESP 的 CI 版本、peer ready、WAKE_DETECTED、
语音上传日志：`interrupt_context` 表示启动调用者错误，`invalid_slave_output`
表示 codec 注册/板级配置与该接口不匹配，`buffer_ownership` 表示缓冲区归还异常。
已经启动但服务端仍只有 `NoMatch` 时，继续核对 ESP 接收到的 PCM 电平和声道，
不能再只凭 UART 心跳或 OTA 成功回执认定收音正常。

主机回归覆盖实际中断/任务 PCM 生产代码、任务专用 API 误用、启动事件合并、
消息队列压力及抢占、过期事件、DMA 先于 TX、STARTING 中断/取消、预填耗尽后继续
完成启动、共享 RX 装载、满预填池恢复和两千次反复重连。
它不验证芯片实际时序、音质或固件链接尺寸。CI 正式镜像必须在 Windows SDK
环境重新生成，并核对新启动日志/镜像版本；本次启动顺序修复没有修改 ESP32 源码，
无需为此单独重建 ESP。原有已打包文件不包含这些源码修改。

*版权归chipintelli公司所有，未经允许不得使用或修改*
