**该示例工程由 李昌壕 提供，2026年7月15日**

### 工程概述

- 该示例工程演示了基于瑞萨 FSP ，在 RA8P1 MCU CPU0 （Cortex-M85）上同时驱动一个 DVP 接口的 OV5640 摄像头或 OV7725 摄像头。

### 支持的开发板 / 演示板：

- CPKCOR-RA8P1 with CPKEXP-EK8x2 扩展板
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKCOR-RA8P1

- 1 块底板：CPKEXP-EK8x2

- 1 个 OV5640 摄像头模组和 1 个 OV7725 摄像头模组

- 1 根 USB Type A->Type C 或 Type-C->Type C 线（支持Type-C 2.0 即可）

### 硬件连接：

- 调试主机通过 USB Type-C 线连接 CPKHMI-RA8P1 板上的 USB 调试端口JDBG。

### 硬件设置注意事项：

- 无

### 软件开发环境：

- FSP版本
  - FSP 6.4.0
- 集成开发环境和编译器：
  - e2studio v2025-12 + LLVM v21.1.1

**详细的样例程序配置和使用，请参考下面的说明文件。**

[camera_ceu 测试工程使用说明](camera_ceu_cpkcor_ra8p1_ep_readme.adoc)
