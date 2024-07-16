该示例工程由 瑞萨电子-黄国爵 提供，2024年7月16日

#### 工程概述:
* 该示例工程演示了基于 CPKDIS-RA8D1B 的 HDMI 和 CEU 驱动测试，HDMI 显示基于 RA8D1 的 RGB LCD 控制器实现。同时通过 CEU 采集 OV7725 摄像头模组数据，实时在显示器显示图像

#### 支持的开发板 / 演示板：
* CPKDIS-RA8D1B

#### 硬件要求：
* 1块 Renesas RA8开发板：CPKDIS-RA8D1B
* 1根 USB Type A->Type C 或 Type-C->Type C 线 （支持 Type-C 2.0 即可）
* 1跟 HDMI<->HDMI 线
* 1个 HDMI 显示器
* 1个 OV7725 摄像头模组


#### 硬件连接：
* 通过 USB Type-C 线连接调试主机和 CPKDIS-RA8D1B 板上的 USB 调试端口

#### 硬件设置注意事项：
* 无

#### 软件开发环境：
* FSP版本
  * FSP 5.3.0
* 集成开发环境和编译器：
  * e2studio v2024-04 + 13.2.1.arm-13-7 或者
  * e2studio v2024-04 + LLVM for ARM 17.0.1

#### 第三方软件
* 无

#### 操作步骤：
* 打开工程
* 注意 board_sdram.c 中的宏：BSP_PRV_SDRAM_SDADR_ROW_ADDR_OFFSET 和 BSP_PRV_SDRAM_BUS_WIDTH，该板子分别对应的是 8 和 1，
  如果需要适配其他SDRAM，根据 spec 修改这两个宏
* 工程默认使用10.1inch屏，若要测试7inch屏，需要修改配置参数，具体修改步骤，请看详细的样例程序配置和使用文档
* 编译，烧录


### 详细的样例程序配置和使用，请参考下面的文件。
[hdmi_cpkdis_ra8d1b_ep_readme](hdmi_cpkdis_ra8d1b_ep_readme.md)

