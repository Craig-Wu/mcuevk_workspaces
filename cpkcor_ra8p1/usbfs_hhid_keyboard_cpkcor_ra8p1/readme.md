**该示例工程由 李昌壕 提供，2026年11月20日**

### 工程概述

- 该示例工程演示了基于瑞萨 FSP ，在 RA8P1 MCU CPU0 （Cortex-M85）上识别并读取 USB 键盘的输入，然后显示于串口终端中。

### 支持的开发板 / 演示板：

- CPKCOR-RA8P1
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKCOR-RA8P1
- 1 根 USB OTG 数据线
- 1 个 USB 有线键盘，无线键盘则务必使用 2.4G 接收器

### 硬件连接：

- 调试主机通过 USB Type-C 线连接 CPKCOR-RA8P1 板上的 USB 调试端口JDBG。
- USB 通过 USB OTG 数据线连接至 CPKCOR-RA8P1 板上的 JUSB 接口。

### 硬件设置注意事项：

- 无

### 软件开发环境：

- FSP版本
  - FSP 6.6.0
- 集成开发环境和编译器：
  - e2studio v2026-07 + LLVM v22.1.0

**详细的样例程序配置和使用，请参考下面的说明文件。**

[usbfs_hhid 测试工程使用说明](usbfs_hhid_cpkcor_ra8p1_ep_readme.adoc)
