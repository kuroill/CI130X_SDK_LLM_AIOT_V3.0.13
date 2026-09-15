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

直接复用 `ci_audio` 使用的 Nuclei GCC 9.2，在 PowerShell 中依次执行：

```powershell
$env:PATH="D:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\tools\build-tools\bin;D:\Project\nuclei-gcc-9.2.0\gcc\bin;$env:PATH"
cd D:\Project\CI130X_SDK_LLM_AIOT_V3.0.13\projects\offline_asr_llm_aiot_iis_sample\project_file
make clean; make -r -j8
```

*版权归chipintelli公司所有，未经允许不得使用或修改*
