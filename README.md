# CI1306 固件常用命令

## Windows x86/x64 重新编译

```powershell
cd E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13
$env:PATH="E:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\tools\build-tools\bin;E:\Project\toolchain-gd32v-v9.2.0-win\bin;$env:PATH"
make -C projects/bnpu_core_alg_pro/project_file clean
if ($LASTEXITCODE -ne 0) { throw 'CI BNPU clean failed' }
make -C projects/offline_asr_llm_aiot_iis_sample/project_file clean
if ($LASTEXITCODE -ne 0) { throw 'CI application clean failed' }
make -C projects/offline_asr_llm_aiot_iis_sample/project_file -r -j8
if ($LASTEXITCODE -ne 0) { throw 'CI build failed' }
```

## Windows 11 ARM64 重新编译

此虚拟机已将完整的 `z_other\gcc` 工具链复制到 Windows 本地 `%LOCALAPPDATA%\Lenwell\gcc-9.2.0`。SDK 仍在 Mac 共享目录。

```powershell
cd C:\Mac\Home\Documents\Work\Lenwell\CI130X_SDK_LLM_AIOT_V3.0.13
$gccBin = "$env:LOCALAPPDATA\Lenwell\gcc-9.2.0\bin"
$tools = (Resolve-Path '.\tools\build-tools\bin').Path
$env:PATH = "$gccBin;$tools;$env:PATH"
if ((& "$gccBin\riscv-nuclei-elf-gcc.exe" -dumpfullversion) -ne '9.2.0') { throw 'Expected GCC 9.2.0' }
& "$tools\make.exe" -C projects/bnpu_core_alg_pro/project_file clean
if ($LASTEXITCODE -ne 0) { throw 'CI BNPU clean failed' }
& "$tools\make.exe" -C projects/offline_asr_llm_aiot_iis_sample/project_file clean
if ($LASTEXITCODE -ne 0) { throw 'CI application clean failed' }
& "$tools\make.exe" -C projects/offline_asr_llm_aiot_iis_sample/project_file -r -j8
if ($LASTEXITCODE -ne 0) { throw 'CI build failed' }
```

## 合成分区和打包（两种 Windows 通用）

编译成功后，在同一个 PowerShell 窗口执行。确认分区合成成功，再运行最后一行打开打包工具。

```powershell
cd .\projects\offline_asr_llm_aiot_iis_sample\firmware
.\合成分区bin文件.bat
.\打包升级.bat
```

## 生成 OTA 包（Windows ARM64 / x64）

打包工具生成 `firmware\full_ci1306_V3.0.<n>.bin` 后，在同一个 PowerShell 窗口执行。`ota_v4\make_ota.ps1` 是 `spilt_firmware`（Linux 程序）的 PowerShell 版，只用系统自带 PowerShell，不需要 C 编译器；版本号按 `firmware\config.ini` 的 `soft_version_3` 填写。

```powershell
cd C:\Mac\Home\Documents\Work\Lenwell\CI130X_SDK_LLM_AIOT_V3.0.13\ota_v4
$ver = '3.0.29'
Copy-Item "..\projects\offline_asr_llm_aiot_iis_sample\firmware\full_ci1306_V$ver.bin" .
powershell -ExecutionPolicy Bypass -File .\make_ota.ps1 ".\full_ci1306_V$ver.bin"
```

输出 `ota_ci1306_V<版本>.bin`（user_code2、asr、dnn、voice、user_file 五个分区），用于 ESP32 侧 OTA。

首次使用先自检一次：用已有的 V3.0.28 生成到临时目录，`sha256` 应为 `7AA72A7E822BE12C74C2D914F90484CC71AD156ED7F3D4D27B9A72AA801D67E6`（与 `ota_v4\ota_ci1306_V3.0.28.bin` 相同）。

```powershell
powershell -ExecutionPolicy Bypass -File .\make_ota.ps1 .\full_ci1306_V3.0.28.bin -OutputDir $env:TEMP
```

## macOS 查看并保存串口日志

```sh
cd /Users/y/Documents/Work/Lenwell/CI130X_SDK_LLM_AIOT_V3.0.13
ls /dev/cu.*
python3 monitor_ci1306_ai_uart.py /dev/cu.usbserial-110 | tee -a ci1306-latest.log
```

端口号按实际连接替换。CI 板载 Type-C（UART0）输出文本日志；右侧 UART1 的 PB7/TX1 输出协议帧，两者均为 921600。需要检查原始字节时，在端口后加 `--raw`。

## Windows 查看并保存串口日志

```powershell
cd C:\Mac\Home\Documents\Work\Lenwell\CI130X_SDK_LLM_AIOT_V3.0.13
.\monitor_ci1306_ai_uart.bat list
.\monitor_ci1306_ai_uart.bat COM3 921600 2>&1 | Tee-Object -FilePath .\ci1306-windows.log -Append
```

将 `COM3` 换成上一步列出的 CI 端口。日志实时显示并追加到 SDK 根目录的 `ci1306-windows.log`，按 Ctrl+C 结束。

## 说明

- CI1306 负责本地唤醒、提示音、回声消除和音频采集播放，通过 UART 和 I2S 连接 ESP32-S3。串口速率为 921600；将示例端口换成 CI 转接器的实际端口，按 Ctrl+C 退出监视。
- 编译命令检查 Nuclei GCC 9.2.0，清理并编译 BNPU 和 MCU，合并生成 `firmware/user_code/user_code.bin`。编译失败时先解决报错，再打包。
- 排查漏响提示音时，依次看 `WAKE_DETECTED` 和 `DING_DONE`。后者表示播放完成回调，扬声器是否响仍需实听。有过爆音的设备先用低音量短句验证。

[启英泰伦文档中心](https://document.chipintelli.com/) · [ESP32 命令](../lenwell-firmware/README.md) · [UART / I2S 协议](../lenwell-firmware/UART_I2S_PROTOCOL.md)
