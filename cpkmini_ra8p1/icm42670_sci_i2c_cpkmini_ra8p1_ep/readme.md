**该示例工程由 李昌壕 提供，2026年8月4日**

### 工程概述

- 该示例工程演示了基于瑞萨 FSP ，在 RA8P1 MCU CPU0 （Cortex-M85）上使用 I2C(r_sci_i2c) 驱动 ICM42670 六轴传感器。

r_sci_i2c 是 FSP 基于 SCI 外设实现的 I2C 主从控制器，它可以作为 rm_comms_i2c 的底层操作接口。使用 rm_comms_i2c 的例程如下：

[icm42670_comms_i2c](../ep_icm42670_comms_i2c/readme.md) 

### 支持的开发板 / 演示板：

- CPKHMI-RA8P1 with CPKEXP-Mini 扩展板
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKHMI-RA8P1
- 1 块底板：CPKEXP-Mini
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

[icm42670_sci_i2c 测试工程使用说明](icm42670_sci_i2c_cpkhmi_ra8p1_ep_readme.adoc) 
