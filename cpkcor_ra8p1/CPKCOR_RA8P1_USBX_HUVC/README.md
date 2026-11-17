# CPKCOR_RA8P1_USBX_HUVC_AutoNegotiate

**该示例工程由 瑞萨电子-阎志刚 提供，2026年11月12日**

## 工程概述

- 该示例工程演示了基于瑞萨 FSP、Azure RTOS ThreadX 和 USBX Host Video Class，使用 RA8P1 MCU 驱动 USB UVC 摄像头进行 MJPEG 视频取流。
- 工程可自动识别摄像头 VideoStreaming 接口使用的 Isochronous 或 Bulk 传输方式，扫描摄像头支持的 MJPEG 分辨率和帧率，并完成模式协商、兼容性评估、性能统计、JPEG 抓图及热插拔处理。
- 工程不根据摄像头型号或 VID/PID 固定取流路径，而是根据 VideoStreaming Endpoint 的类型自动选择 Isochronous 或 Bulk 处理流程。
- 工程支持 J-Link RTT Viewer 和 SCI9_B UART 两种 Debug Console，可通过编译配置选择 Console 类型，并使用相同的命令接口进行模式选择、视频取流、JPEG 抓图和状态查询。
- 关于如何烧录已编译的镜像文件，请参考 [RA8 MCU 的程序烧录]()。
- 如果不需要同步代码库及版本控制，也可以直接下载本样例程序的 ZIP 压缩包，其中包含工程文档和代码。

## 支持的开发板 / 开发套件 / 演示板

- CPKCOR-RA8P1 核心板

## 硬件要求

- 1 块 CPKCOR-RA8P1 核心板
- 1 个支持 USB Video Class（UVC）和 MJPEG 格式的 USB 摄像头
- 1 根用于摄像头连接的 USB 数据线或转接线，具体接口形式根据摄像头接口确定
- 1 根 USB Type-A 转 Type-C 或 Type-C 转 Type-C 数据线，用于连接 CPKCOR-RA8P1 的 JDBG 调试端口
- 1 台安装了 J-Link RTT Viewer、串口终端工具和 e² studio 的调试主机

## 硬件连接

- 调试主机通过 USB Type-C 数据线连接 CPKCOR-RA8P1 板上的 USB 调试端口 JDBG。
- USB UVC 摄像头连接至 CPKCOR-RA8P1 的 USB High-Speed Host Type-C 接口。
- 运行后可根据工程配置，通过 J-Link RTT Viewer 或 SCI9_B UART Console 查看摄像头信息、模式列表、取流状态和性能报告，并输入操作命令。

## 硬件设置注意事项

- 工程使用 RA8P1 CPU0，运行 Azure RTOS ThreadX；CPU1 和 NPU 不参与本示例功能。
- USB 使用 USBHS Module 1，配置为 Host 和 High-Speed 模式。
- USBHS VBUS 和 VBUS Enable 分别使用 P408 和 P407；P615 配置为 `FUSB_EN_N` 并输出高电平。
- USB 摄像头属于总线供电设备。频繁物理插拔时应保证开发板和摄像头供电稳定，避免因瞬态电压扰动导致 J-Link、RTT 或 UART Console 连接异常。
- 默认 JPEG Frame Buffer 位于内部 SRAM，每个 Buffer 为 384 KB，共 2 个。2560 × 1440 MJPEG @ 30 fps 已完成实机验证；复杂场景下，4K 分辨率的单帧 JPEG 数据可能超过单个默认缓冲区的容量。未启用 SDRAM 时，软件将根据 Frame Buffer 容量把预计超出缓冲区的 Mode 标记为 `NG-B`。
- SDRAM Frame Buffer 默认关闭。

## 软件开发环境

- FSP 版本
  - FSP 6.6.0
- RTOS 和中间件
  - Azure RTOS ThreadX 6.4.0 + FSP 6.6.0
  - Azure RTOS USBX HUVC 6.4.0 + FSP 6.6.0
- 集成开发环境和编译器
  - e² studio Version: 2026-04.2 (26.4.2)
  - LLVM for Arm 21.1.1
- 工程配置
  - MCU：R7KA8P1KFLCAC
  - CPU Core：CPU0 / Arm Cortex-M85
  - USB：USBHS Host，High-Speed
  - DMAC：Channel 0/1，传输数据宽度：4 字节
  - Debug Console：J-Link RTT / SCI9_B UART
  - SCI9_B UART：Channel 9，115200 baud，8N1

**详细的样例程序配置和使用，请参考下面的说明文件。**

[CPKCOR_RA8P1_USBX_HUVC_AutoNegotiate 工程使用说明](CPKCOR_RA8P1_USBX_HUVC_AutoNegotiate_工程使用说明.adoc)
