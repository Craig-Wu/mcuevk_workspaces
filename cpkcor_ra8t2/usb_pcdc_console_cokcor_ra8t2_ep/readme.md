**该示例工程由 李昌壕 提供，2026年8月21日**

### 工程概述

- 该示例工程演示了基于瑞萨 FSP ，在 RA8P1 MCU CPU0 （Cortex-M85）上将 C stdio 重定向到 USB HS PCDC。

### 支持的开发板 / 演示板：

- CPKNET-RA8T2
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKNET-RA8T2
- 2 根 USB Type A->Type C 或 Type-C->Type C 线（支持Type-C 2.0 即可）

### 硬件连接：

- 调试主机通过 USB Type-C 线连接 CPKNET-RA8T2 板上的 USB 调试端口JDBG。
- 主机通过 USB Type-C 线连接 CPKNET-RA8T2 板上的 JUSB 端口

### 硬件设置注意事项：

- 无

### 软件开发环境：

- FSP版本
  - FSP 6.4.0
- 集成开发环境和编译器：
  - e2studio v2025-12 + LLVM v21.1.1

**详细的样例程序配置和使用，请参考下面的说明文件。**

[usb_pcdc_console 测试工程使用说明](usb_pcdc_console_cpknet_ra8p1_ep_readme.adoc)
