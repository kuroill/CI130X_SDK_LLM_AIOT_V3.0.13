# CI130X SDK 简介

## 概述

CI130X SDK 适用于CI130X系列芯片

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

本 SDK 的官方预编译算法库由 Nuclei GCC 9.2.0 生成，必须使用同版本工具链。当前构建日志中的 `D:\Project\gcc\bin` 是 GCC 10.2.0，会在链接 LTO 算法库时报 `generated with GCC compiler older than 10.0`。确认 `D:\Project\gcc\bin` 已换成 GCC 9.2.0 后，在 PowerShell 中依次执行：

```powershell
$env:PATH="E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\tools\build-tools\bin;E:\Project\toolchain-gd32v-v9.2.0-win\bin;$env:PATH"
cd E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\projects\offline_asr_llm_aiot_iis_sample\project_file
make clean; make -r -j8
```

关闭捕捉
#define ASR_MIDDLE_RESULT_OUT_EN       0  

## ESP 重启与 I2S 恢复验证

`offline_asr_llm_aiot_iis_sample` 的掉线清理由现有系统消息任务串行执行。
心跳任务只提交合并的超时事件；清理期间不重新开放 PCM 生产。
外部 IIS0 从机 TX 先停止本通道 DMA、清除本通道中断、归还队列和硬件持有的缓冲区，
再由 codec owner 记录/完成 IIS 禁用。缺少 ESP BCLK/LRCK 时不等待寄存器装载，
时钟恢复后先完成旧禁用再启用 TX；不复位共享 IISDMA、ADC 或本地 DAC。
内部 DAC 的正常播放、FINISH 排空和 AEC 尾音流程保留。

AIOT 音频数据会经核间 mailbox 回调进入主核，IIS 上行回调同时支持中断和任务
上下文。回调只同步复制 PCM，使用二值信号量及对应的 `FromISR` 接口与清理互斥，
codec 和队列初始化完成后才开放，不执行 IIS 启停，不保存算法输入指针。
现有心跳任务每五十毫秒检查上行预填，
只提交一份合并的启动事件；系统消息任务负责实际启动及缺时钟时的后续重试。
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
   `[AI_I2S] continuous uplink started prefillFrames=... owner=system context=task`，
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

失败时分别记录 CI 的启动/清理日志和 ESP 的 CI 版本、peer ready、WAKE_DETECTED、
语音上传日志：`interrupt_context` 表示启动调用者错误，`invalid_slave_output`
表示 codec 注册/板级配置与该接口不匹配，`buffer_ownership` 表示缓冲区归还异常。
已经启动但服务端仍只有 `NoMatch` 时，继续核对 ESP 接收到的 PCM 电平和声道，
不能再只凭 UART 心跳或 OTA 成功回执认定收音正常。

主机回归覆盖实际中断/任务 PCM 生产代码、任务专用 API 误用、启动事件合并、
消息队列压力及抢占、过期事件、满预填池恢复和两千次反复重连。
它不验证芯片实际时序、音质或固件链接尺寸。CI 正式镜像必须在 Windows SDK
环境重新生成，并使用可区分的新版本号；本次 mailbox 修复没有修改 ESP32 源码，
无需为此单独重建 ESP。原有已打包文件不包含这些源码修改。

*版权归chipintelli公司所有，未经允许不得使用或修改*
