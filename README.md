# CI130X SDK 简介

## 概述

CI130X SDK 适用于CI130X系列芯片

官方文档：
https://www.chipintelli.com/zh-cn/page/151.html

## 2026-10-21 下行故障安全修复

本节优先于下文历史的 100% 音量、长回复和反复播放验收。此前最后一轮播放出现
强烈爆音、CI UART 失联及随后重新握手；尚无 CI 同时段复位原因与模拟波形，
不能将这次修改解释为已经证明爆音根因或完成实机修复。

下行 worker 不再持有 RX DMA 环裸指针等待 DAC：先复制到静态自有帧，再以零等待
方式尝试写入 DAC 队列。DAC 暂满时锁外让出一个 tick，并重试同一帧；DAC buffer
只在同一锁内借出、入队或归还，不把借出的 buffer 带到锁外等待。样本计数与摘要
只在实际入队成功后提交。正常停止、FINISH、AEC、双槽格式和用户音量保持原样。

START/STOP 的 generation 隔离旧帧和迟到故障事件。START 在原有 PA 稳定准备后
有界丢弃旧 RX 数据，并建立本轮 overrun 基线；空闲继续排空 RX。实际环溢出、
复制失败、DAC 无进展或入队失败会冻结本轮并报告故障，由原 system-message owner
执行停播；worker 不直接操作 PA 或停止 codec。失败事实保留至下一次 START，
迟到 FINISH 不能把失败轮次伪报为完整播放。

启动核对 `[DOWNLINK_GUARD] version=1 rx=owned_copy dac=nonblocking_retry
fault_owner=system`。其中 `resetState` 是 SDK 对正常上电/异常复位的原始分类，
不是具体的看门狗或掉电原因；UART `FIRMWARE_INFO` 的 boot_state 也不能代替它。

保护边界：CPU 临界区不能冻结 DMA，新副本消除了跨无界 DAC 等待的确定风险，
但 DMA/ISR 极端时序仍须实机验证。通用 `cm_stop_codec` 的硬件缓冲回收仍依赖
DMA/IRQ；本路径先执行静音及现有 PA 关闭，再调用它，不能声称全部硬件等待有界，
也不能从软件调用推断功放已经电气静音。新增约一帧 stereo 和一帧 mono 的静态
暂存需要在 Windows 正式链接时核对 RAM 余量。

用户按原 Windows 流程构建和更新 CI，并同步更新 ESP 的取消优先收尾。没有
UART/云端协议变化，不需要为此更新 IoT；两端补齐后再做安全验收：

1. 先确认安全断电条件，在低音量、远离耳朵的短句测试中同时保存 ESP/CI 日志。
   不戴耳机或直接重放导致爆音的长句；异常刺耳声再次出现就停止测试。
2. 正常短句应 FINISH 成功；同次完整播放的两端 `CI_PCM` 非零样本数/摘要一致。
   匹配不代表模拟音质正常，中断轮不作完整摘要比较。
3. 验证真人及唤醒词打断后下一轮：STOP 仍由原 owner 执行；旧代故障事件和音频
   不得关闭或污染新轮。中文、英语、西班牙语都要分别验证。
4. 在可控测试环境检查故障：保留 `[DOWNLINK] fault stopped` 的 generation、
   reason/detail，以及 FINISH 失败、PA 顺序、`AUDIO_DMA`、CI 重启和握手日志。
   将接收环溢出、DAC 等待/入队故障、CI 重启和模拟杂音分别判定，不统称网络故障。
5. 短句与异常停止确认安全后才逐步延长播报。主机回归不生成正式固件，不证明
   DAC/I2S、真实抢占时序或扬声器安全；Windows 构建和实机验收均由用户完成。

可在 macOS 执行以下检查；`-B` 避免修改仓库中已有的 Python 字节码缓存：

```sh
python3 -B script/test_downlink_pcm_ownership.py
python3 -B script/test_downlink_pcm_trace.py
python3 -B script/test_pa_idle_mute.py
python3 -B script/test_iis_peer_recovery.py
python3 -B script/test_wake_ding.py
python3 -B script/test_ci_build_headers.py
```

ownership 回归执行实际 C 函数，覆盖同帧等待后继续播放、旧 generation 隔离、
故障事件队列满与同步消费、DAC buffer 借还、接收环溢出及失败 FINISH 重试；
模拟不包含真实 DMA 与模拟电路。

## 唤醒提示音

每次唤醒都发送 `0x10 WAKE_DETECTED` 并播放 ding；仅在 ESP 续约的对话租约
（`SET_CONVERSATION_STATE` active/waiting）有效时跳过 ding，仍打断旧播放、恢复识别，
不伪造 DING_DONE。原夜间静音唤醒（`0x2A SET_WAKE_PROMPT_POLICY`、
`0x1A QUIET_WAKE_DETECTED`）已于 2026-10-22 移除，类型值保留不复用；旧 ESP 若仍发送
`0x2A`，CI 按未知命令回 UNSUPPORTED，因此需先升级 ESP 再升级 CI。

`python3 -B script/test_wake_ding.py` 只做实际 C 函数的主机回归，不是正式固件构建。

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

## 持续播放杂音：REF 原始波形取证（2026-10-19）

最新用户确认：滋滋声伴随唤醒 ding 和整段回复，空闲没有；此前“只在首尾”
的描述已被更正。当前 `refPgaReg=0x0c`、窗口削顶报告 0‰、双端 PCM 摘要
一致、`resumedGaps=0`，正确 PA/HPOUT 顺序仍有杂音。因此不能再用延长开关
等待或调整模拟增益猜修；0‰ 是取整统计，也不等于所有采样及模拟输出无失真。

本轮保持实际 PCM、IIS/DMA、输出增益、MIC/REF/AEC 及播放 owner 不变，只增加
一次有界原始 REF 快照。每次播放跳过前八帧，在其后 REF 绝对值总和最大的
一帧内，保存峰值附近连续 64 个原始样本；结束后由既有心跳 task 打印一次。
音频/mailbox ISR 仅累计并拷贝固定快照，不打印、不分配堆内存、不增加 Task。
原始 MIC/REF 在重采样开启时为 32kHz，处理后的 DST/IIS 为 16kHz，不能混用。
version=4 的 REF 日志曾将采样率写死为 16000；当前工程实际 32000，64 点仅约
2ms，应按 32kHz 重新解释已有 hex。version=5 按原始 ADC 配置打印采样率；
关闭重采样时才是 16kHz/4ms。此次只纠正诊断标注，不改变音频数据和播放行为。
该片段可检查反馈波形有无平顶/不连续，不能证明整段没有高频底噪，也不能
单独定位 HPOUT、PA 或供电；极短播放、报告队列满时可能没有快照。

已有 version=4 日志可直接按当前实际 32kHz 重新分析，无需为标注修正单独刷机。
后续 Windows 按既有下节流程重建更新 CI，启动应为
`[AUDIO_DIAG] version=5 ... ref=peak_frame64 ref_rate=adc`，`REF_PCM` 应显示
`sampleRate=32000`；本轮无需改变 UART/I2S 契约或配置。以 50% 和 100% 分别唤醒，
安静听同内容短回复和长回复，再真人插话并开始下一句，最后退出后重新唤醒。
此次元数据修正不应改变任何声音；日志采样率正确也不代表杂音已修复。

| 观察 | 预期及失败区分 |
| --- | --- |
| `[REF_PCM]` | 每次有效播放结束最多一条；`playback` 对齐 AEC，`selectedFrame` 为采样帧号，`frameAbs` 为该帧 REF 绝对值总和，`samples=64 sampleRate=32000 encoding=s16le hex=...`（当前重采样配置；关闭时 16000）。保留完整 hex（256 个十六进制字符），不要截断。 |
| 完整自然播放 | `finish complete` 的计数与 ESP 一致；`resumedGaps=0`。一致只证明非零数字内容到达队列，仍需听感与 REF/输出波形。 |
| 缺少快照 | 先看 version、`droppedReports`、播放是否超过八帧；不把缺日志当作静音或正常波形。 |
| 提前停止/自言自语 | 同 turn 对照服务端 `[BARGE]`、ASR partial/final 与 ESP STOP；真人/唤醒打断仍应生效。主动中断摘要可不同，不当作丢包。 |
| 音量差异/语言错误 | ESP 的实时音量与 IoT 的短句多语复核分别交付，见各自 README；本 CI 诊断不改变它们的行为。 |

`python3 script/test_audio_noise_diagnostics.py` 使用实际 C 采集与打印函数，验证
样本不变、ISR 不打印、原始十六进制编码、有界环、16k/32k 原始采样率标注，
以及整窗结束也能交付快照。
主机检查不等于 Windows 正式固件构建或音质验收；持续杂音仍待下一轮实机取证。

## 播放开关杂音与 HPOUT 所有权（2026-10-19）

最新 `esp-d262925d` 的 REF 前级修正已生效：`refPgaReg=0x0c`，报告窗口的
`refClipPermille=0`，下行摘要一致、FINISH 完整；这没有解决用户反馈的杂音。
上一轮曾描述为首尾杂音；最新确认见上节，播报中间也持续伴随。输出切换
时序修正未解决杂音，不能再将 REF 削顶与扬声器杂音当作同一个已证实根因。

原实现先开 PA、再开 HPOUT；结束先关 HPOUT、再关 PA，codec 的 DMA 生命周期
回调还会额外切 HPOUT。本轮将主动 PA 控制模式的 HPOUT 操作收口到
`audio_play_hw_pa_da_ctl`：板级不再给通用 DMA start/stop 注册 HPOUT 回调，
只移除重复的模拟输出副作用，保留 DMA/时钟/缓冲启动停止本身。该变更仅用于
`SIMPLE_AUDIO_PLAYER_ENABLE && PLAYER_CONTROL_PA`，其他播放器路径保持原路由。

起播先确认 PA 关闭，应用原增益、开启 HPOUT，等待 20ms，再开 PA 并保留原
100ms 稳定等待；结束先关 PA，等待 20ms，再关 HPOUT。下行在关输出后才停 DAC
DMA；自然 FINISH 仍先等待音频播完。组合提示音保持 PA/HPOUT 开启，换段不
重写 DAC 增益，避免第一段 PCM 开始后将 digital code 从 235 再写回 255。
实际输出增益、用户音量、提示音比例、MIC/REF/AEC 参数及真人打断均不变。
20ms 是待实机核验的输出稳定时间，不能视为已测定电容或功放的真实稳定时间。
每次冷起播和关闭分别新增约 20ms；ESP 现有 START 600/1500ms 预算保持原样。

本轮 Windows 按下方流程完整重建 CI；ESP 无需重编。启动和播放日志应包含：

```text
[AUDIO_DIAG] version=3 ... gain=ref_frontend_0db output=pa_guarded_hpout
[AUDIO_OUT] phase=active hpoutMuted=0 ... switchOrder=hpout_then_pa elapsedMs=...
[AUDIO_OUT] phase=idle hpoutMuted=1 ... switchOrder=pa_then_hpout elapsedMs=...
```

| 实机步骤 | 预期与失败区分 |
| --- | --- |
| 100% 音量听内置提示、短 ding、短/长回复各三次 | 首尾完整、开关时无滋滋声；冷起播 `elapsedMs` 通常至少约 120ms，关闭至少约 20ms；组合换段不重复 PA 边沿。日志是软件顺序，声学效果仍需听感/示波器确认。 |
| 听完后安静等待 | PA 关闭、HPOUT 静音，持续上行采音；没有为了降噪停止 ADC/AEC。自然结束有 `finish complete`，双端 PCM 摘要一致。 |
| 播放中真人/唤醒词打断，再说下一句、退出后重唤醒 | 原 owner 执行一次 STOP；重复 STOP 不关掉新的 ding，下一轮能正常播放。主动中断不要求双端摘要一致。 |
| 空闲/播放中单独重启 ESP | peer 清理后 PA 关闭，时钟恢复后上行和下一次播放正常，不需要 CI 重新上电。 |
| `version=3` 与正确 `switchOrder` 下仍有开关杂音 | 测 PA 控制脚、HPOUT、PA 输入耦合及供电瞬态；若 `pcm_timeout`/双端摘要不同，仍按数字链路错误单独排查，不继续盲调增益。 |

主机模拟使用真实硬件控制函数和板级回调表，直接断言每次 HPOUT 位变化均发生
在 PA 关闭时，覆盖双有效电平、400 次自然结束、组合换段、重复 STOP、超时与
失败清理；它不证明模拟电压或实际音质正常：

```sh
python3 script/test_pa_idle_mute.py
```

## WF0153 样板 AEC 参考增益（2026-10-22）

MN-WF-0153 量产板的 AEC 参考取自 SPK+，经 R2 180K / R1 4.7K（约 −32dB）、C25
进 MICPR，MICNR 悬空（与官方 AB 类参考电路一致），但分压比官方 22K/2.2K 弱
11dB。下节开发板调参后的 REF（前级 0dB、AEC 期 PGA 0dB、数字 +1dB）在样板上
只剩约 −30dBFS：BF24 在 10–15% 音量播放期 `micAbs≈2000–3400`、
`refAbs≈100–180`、`dstAbs≈micAbs`（AEC 消除量约 0dB），ASR 把自身回复识别为
用户话语。开发板同固件 mic/ref 约 1:1，消除量 30dB 以上。

`LENWELL_AEC_REF_WF0153`（`user_config.h`，`sdk_default_config.h` 同默认值 1）
选择板型：1 为量产 0153，REF 恢复 V3.0.13 基线（前级 12dB、AEC 期 PGA 4dB 即
3dB 档、数字 +6dB），比上一版高约 20dB；0 为开发板，保持原值。MIC 增益、AEC
算法参数、DAC/音量、IIS/DMA、协议和打断策略不变。样板功放先于 REF ADC 饱和，
最强播放时 REF 峰值约 780×10≈8000，不会削顶；开发板若要使用必须改为 0。

刷入后启动日志应为：

```text
[AUDIO_DIAG] version=6 ... ref_board=wf0153 ref_gain=front12db_aecpga4db_dig6db ...
```

播放窗口的 `[AUDIO_GAIN]` 应为 `refPgaReg=0x6e`（空闲 0x6c），MIC 仍为
`0x71/0x79`。BF24 验收：10%、60%、90% 音量各问长回复，播放进入稳定后
`[AEC_PATH]` 中 `refAbs` 与 `micAbs` 同一量级、`dstAbs` 明显低于 `micAbs`
（目标 20dB 以上），`refClipPermille=0`；服务端不再出现无人说话的 `[BARGE]
confirmed` 和自身回复被识别为 ASR final；近、远距离真人插话仍能打断。若 ref 仍
远低于 mic 或消除量仍接近 0dB，需排查结构密封与 MIC 削顶，不能宣布已修复。

## REF 削顶、自回声与播放杂音（2026-10-19）

当前实现将实际板上右 ADC 的模拟 REF 前级从 12dB 改为 0dB，并将初始 REF
PGA 固定为 0dB。该通道接的是经分压的功放输出，不能继续套用第二支麦克风的
前级放大。修改仅限内部 ADC 右路为 REF 的拓扑；其他输入拓扑保留原设置。
MIC 前级、播放/空闲 PGA、数字增益、用户音量、提示音比例、DAC、IIS、DMA、
AEC 算法参数和真人打断策略保持原样。新日志已验证该设置生效，报告窗口不再出现此前持续的 REF 满幅；不代表所有模拟失真已消除。

本次 `esp-b4be67b2` 的证据：CI REF 满幅样本占比最高 56.6%，唤醒 ding 为
31.6%；真实 `refPgaReg=0x6c` 表明 REF PGA 已是 0dB，但高位前级仍有 12dB。
部分 MIC 窗口也有削顶，不能认定 REF 是所有回声的唯一来源。唤醒 ding 的
`LOCAL_PCM clippedSamples=0`，所有已记录播放的 `resumedGaps=0`，未记录到
DAC 中途切换零缓冲。两次提前停播分别由临时文字“本”“这是”触发服务端
`[BARGE] source=partial reason=recognized_partial`，同期 VAD 无语音候选。
后续 ASR final 中混入与天气回复近似的文字，并被对话历史再次引用；自回声是
明确排查方向，不能把整段混合识别丢弃而误删后半句真人问题。

之前同一报时回复中，服务端 Opus 解码后按现有音量及淡入淡出处理的 PCM，与
ESP 和 CI 的 `nonzeroSamples=31731 nonzeroHash=d204c3ce peak=9056` 一致。
这只支持非零内容完整到达 DAC 队列，不能证明模拟输出、时钟和声音干净。REF
削顶足以破坏 AEC 参考的线性关系，但是否也造成扬声器滋滋声仍未证明；本轮先
修已证实的输入余量问题，不以降低播放音量、过滤固定词或关闭真人打断处理。
前级与 PGA 是两级独立模拟增益，不能用 ADC 后的数字衰减恢复已经削顶的波形。

REF 修正的实机寄存器为 `0x0c`。当前输出时序版本需按下节重建两核；
会话误退出的 IoT 修正另需用户部署服务，UART/I2S 协议不变。当前启动标记：

```text
[AUDIO_DIAG] version=3 aec=windowed_task_logs dma=scheduled_blocks gain=ref_frontend_0db output=pa_guarded_hpout
```

保持 100% 音量，保存 ESP/CI 完整日志；先安静听启动内置播报和唤醒 ding，再问
天气等长回复并听完，最后近距、远距、轻声真人插话，退出后再次唤醒并重复。

| 检查步骤 | 预期与异常区分 |
| --- | --- |
| 播放窗口的 `[AUDIO_GAIN]` | `refPgaReg=0x0c`，MIC 仍为 `0x71`，两路 ADC digital code 仍为 199，DAC 仍为 `31/255`。REF 仍为 `0x6c` 先检查构建、刷入镜像和实际配置；启动 version 标记不能代替寄存器核对。 |
| `[AEC_PATH]` 覆盖 ding、长回复与结束尾音 | 目标是 `refClipPermille=0`，同时查看 `refPeak/refAbs` 是否仍过大、`micClipPermille` 是否仍有削顶和 `dstAbs/dstPeak` 的回声残留。0dB 生效仍持续削顶时，需要量 REF/MIC 引脚波形与功放输出；软件统计不能区分芯片输入过载和上游模拟失真。 |
| 安静听完整短/长回复 | 不应出现自发 interrupt 或新用户轮次；正常结束有 `finish complete expectedNonzero=... receivedNonzero=...`。对齐双端 PCM 摘要；中断时因在途缓冲和主动清理，不要求两端摘要一致。 |
| 真人插话、下一句、退出再唤醒 | 保留正常打断及完整识别，新回复正常播放；同看 `[BARGE] source/reason/evidence_end_ms/boundary_ms`、ESP `stop_requested` 和 CI 停播日志，区分真人、临时识别、硬件超时及自然结束。 |
| 听感仍有滋滋声 | 若 PCM 一致、`resumedGaps=0` 且 REF 不再削顶，杂音仍独立存在，继续测 DAC/HPOUT、PA 供电、输出及实测时钟；不能宣布模拟链路已经修复。 |

已有诊断保留：`LOCAL_PCM` 为本地增益后样本；`AEC_PATH` 首窗 8 帧、其后每 64 帧
汇总所有 MIC/REF/DST 有效样本，结束保留不足一窗的数据。`mic/ref/dstClipPermille`
为 16bit 满幅样本比例，均值字段辅助观察直流；`AUDIO_GAIN` 是窗口结束的真实寄存器
快照。`AUDIO_DMA startupZeroBlocks/tailZeroBlocks` 区分起止补零，`resumedGaps`
仅统计 PCM 之间补零后又恢复 PCM 的情况，属于已调度 DMA 的证据。`droppedReports`
是有界诊断环丢弃报告数，不是音频丢帧。打印仍在原心跳 task，音频/mailbox ISR
只累计快照；没有新增音频堆缓冲、Task 或跨端接口。

macOS 主机回归只检查实际 C 配置、寄存器写入和现有状态/队列路径，不生成正式固件：

```sh
python3 script/test_aec_ref_headroom.py
python3 script/test_audio_noise_diagnostics.py
python3 script/test_iis_peer_recovery.py
python3 script/test_pa_idle_mute.py
python3 script/test_downlink_pcm_trace.py
python3 script/test_aec_codec_gain.py
python3 script/test_ci_build_headers.py
```

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

共享 UART/I2S 定义在 SDK 内部的 `components/ci_common/ci_conversation.h`，
MCU 和 BNPU 都通过现有 `source_file.prj` 搜索此目录，不依赖 `protocols` 或外部
生成流程。修改时同步检查 ESP 内部的 `src/wire/ci_conversation.rs` 及本地 C
头文件；两端命令值、状态、租约和 I2S 参数必须一致。BNPU 的
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
