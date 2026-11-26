**该示例工程由 李昌壕 提供，2026年11月26日**

### 工程概述

- 该示例工程演示了基于瑞萨 FSP ，在 RA8P1 MCU CPU0 （Cortex-M85）上运行 LVGL Benchmark。

### 支持的开发板 / 演示板：

- CPKHMI-RA8P1
  
### 硬件要求：

- 1 块 Renesas RA8 开发板：CPKHMI-RA8P1
- 1 个 EK79007AD2 屏幕模组

### 硬件连接：

- 调试主机通过 USB Type-C 线连接 CPKHMI-RA8P1 板上的 USB 调试端口JDBG。
- 屏幕使用 FPC 排线连接至核心板上的 MIPI 接口

### 硬件设置注意事项：

- 无

### 软件开发环境：

- FSP版本
  - FSP 6.6.0
- 集成开发环境和编译器：
  - e2studio v2026-07 + LLVM v22.1.0
  - rasc v2026-07 + Keil 5.43 + AC6.24

**详细的样例程序配置和使用，请参考下面的说明文件。**

[lvgl_benchmark 测试工程使用说明](lvgl_benchmark_cpkhmi_ra8p1_ep_readme.adoc)
