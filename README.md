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

本 SDK 可以直接复用 `ci_audio` 已使用的 Nuclei GCC 9.2，不需要另行下载新版工具链。

当前试验工程为 `offline_asr_llm_aiot_iis_sample`。下面的命令会在
`D:\Project\nuclei-gcc-9.2.0` 中自动查找编译器，避免 PATH 误指向旧的
`ci_audio` 工具目录。

如果 SDK 或工具链位于其他目录，只修改开头的 `$SdkRoot` 和
`$ToolchainRoot`：

```powershell
$ErrorActionPreference = "Stop"

$SdkRoot = "D:\Project\CI130X_SDK_LLM_AIOT_V3.0.13"
$ToolchainRoot = "D:\Project\nuclei-gcc-9.2.0"

$BuildTools = Join-Path $SdkRoot "tools\build-tools\bin"
$ProjectFile = Join-Path $SdkRoot "projects\offline_asr_llm_aiot_iis_sample\project_file"
$Make = Join-Path $BuildTools "make.exe"

if (-not (Test-Path $Make)) {
    throw "找不到 SDK make.exe: $Make"
}
if (-not (Test-Path $ProjectFile)) {
    throw "找不到 IIS 工程目录: $ProjectFile"
}

$Compiler = Get-ChildItem -Path $ToolchainRoot `
    -Filter "riscv-nuclei-elf-gcc.exe" -File -Recurse |
    Select-Object -First 1

if (-not $Compiler) {
    throw "在 $ToolchainRoot 中找不到 riscv-nuclei-elf-gcc.exe，请先安装官方 Nuclei GCC 工具链"
}

$env:PATH = "$BuildTools;$($Compiler.DirectoryName);$env:PATH"

Write-Host "make:     $Make"
Write-Host "compiler: $($Compiler.FullName)"
& $Make --version
& $Compiler.FullName --version

Set-Location $ProjectFile
& $Make clean
if ($LASTEXITCODE -ne 0) {
    throw "make clean 失败，退出码 $LASTEXITCODE"
}

& $Make -r -j8
if ($LASTEXITCODE -ne 0) {
    throw "固件编译失败，退出码 $LASTEXITCODE"
}

Write-Host "编译完成: $ProjectFile\build"
```

成功时，开头打印的 `make` 路径必须属于当前 V3.0.13 SDK，编译器路径必须指向
`riscv-nuclei-elf-gcc.exe`。如果日志出现
`D:/Project/ci_audio/tools/build-tools/bin/sh`，说明当前终端仍在使用旧 SDK 的
工具路径，应关闭该终端并重新执行上面的完整命令。

编译完成后，可在 PowerShell 中生成分区固件：

```powershell
$SdkRoot = "D:\Project\CI130X_SDK_LLM_AIOT_V3.0.13"
$FirmwareDir = Join-Path $SdkRoot "projects\offline_asr_llm_aiot_iis_sample\firmware"
Set-Location $FirmwareDir
& ".\合成分区bin文件.bat"
if ($LASTEXITCODE -ne 0) {
    throw "分区固件合成失败，退出码 $LASTEXITCODE"
}
```

*版权归chipintelli公司所有，未经允许不得使用或修改*
