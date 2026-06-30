**该示例工程由 李昌壕 提供，2026年6月25日**

### 工程概述

- 该示例工程演示了基于瑞萨 FSP ，在RA8P1 MCU CPU0 （Cortex-M85）驱动 EK79001 屏幕显示彩条，使用 RGB 接口。

### 支持的开发板 / 演示板：

- CPKCOR-RA8P1 with CPKEXP-EK8x2 扩展板
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKCOR-RA8P1

- 1 块 CPKEXP-EK8x2 扩展板

- 1 个 EK79001 屏幕模块，型号 RTKLCDPAR1，分辨率 1024x600

- 1 根 USB Type A->Type C 或 Type-C->Type C 线（支持Type-C 2.0 即可）

### 硬件连接：

- 调试主机通过 USB Type-C 线连接 CPKOR-RA8P1 板上的 USB 调试端口JDBG。

### 硬件设置注意事项：

- 无

### 软件开发环境：

- FSP版本
  - FSP 6.4.0
- 集成开发环境和编译器：
  - e2studio v2025-12 + LLVM v21.1.1

**详细的样例程序配置和使用，请参考下面的说明文件。**

[lcd_ek79001_rgb工程使用说明](lcd_ek79001_rgb_cpkcor_ra8p1_ep_readme.adoc)
