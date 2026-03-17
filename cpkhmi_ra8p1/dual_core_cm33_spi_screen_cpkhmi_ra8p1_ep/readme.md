**该示例工程由 李昌壕 提供，2026年3月17日**

### 工程概述

- 该示例工程基于瑞萨FSP， 演示了在RA8P1 MCU上，将原运行在 CPU0 上的代码迁移至 CPU1 上运行。

### 支持的开发板 / 演示板：

- CPKHMI-RA8P1
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKHMI-RA8P1

- 1 根 USB Type A->Type C 或 Type-C->Type C 线（支持Type-C 2.0 即可）

### 硬件连接：

- 通过 USB Type-C 线连接 CPKHMI-RA8P1 板上的 USB 调试端口和调试用主机。

### 硬件设置注意事项：

- 无

### 软件开发环境：

- FSP版本
  - FSP 6.3.0
- 集成开发环境和编译器：
  - e2studio v2025-12 + LLVM v21.1.1

**详细的样例程序配置和使用，请参考下面的文件。** 

[dual_core_cm33_spi_screen_ra8p1_ep_read](dual_core_cm33_spi_screen_ra8p1_ep_read.adoc) 
