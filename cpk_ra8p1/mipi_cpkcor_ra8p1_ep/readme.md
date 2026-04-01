该示例工程由 瑞萨电子-黄国爵 提供，2025年1月23日

#### 工程概述:
* 该示例工程演示了基于CPKCOR-RA8P1的 MIPI 驱动测试，支持 10.1inch触摸屏

#### 支持的开发板 / 演示板：
* CPKCOR-RA8P1

#### 硬件要求：
* 1块 Renesas RA8开发板：CPKHMI-RA8P1
* 1根 USB Type A->Type C 或 Type-C->Type C 线 （支持 Type-C 2.0 即可）
* 1块 10.1inch 屏，型号为：STL10.1-32-331-A，驱动 IC 为：Ilitek-ILI9881C。

#### 硬件连接：
* 通过 USB Type-C 线连接调试主机和 CPKCOR-RA8P1 板上的 USB 调试端口。

#### 硬件设置注意事项：
* 无

#### 软件开发环境：
* FSP版本
  * FSP 6.0.0
* 集成开发环境和编译器：
  * e2studio v2025-07 + LLVM for ARM 18.1.3

#### 第三方软件
* 无

#### 操作步骤：
* 打开工程
* 注意board_sdram.c中的宏：BSP_PRV_SDRAM_SDADR_ROW_ADDR_OFFSET 和 BSP_PRV_SDRAM_BUS_WIDTH，该板子分别对应的是 8 和 1，
  如果需要测试其他SDRAM，根据spec修改这两个宏
* 工程默认使用10.1inch屏，具体修改步骤，请看详细的样例程序配置和使用文档
* 编译，烧录


### 详细的样例程序配置和使用，请参考下面的文件。
[mipi_cpkcor_ra8p1_ep](mipi_cpkcor_ra8p1_ep.md)

