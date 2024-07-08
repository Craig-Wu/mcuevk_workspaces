## 1.参考例程概述
该示例项目演示了基于瑞萨 FSP 的瑞萨 RA MCU在AzureRTOS下作为PCDCACM的基本功能。当代码运行时，RA8D1可以作为USB PCDCACM设备和主机通信。
代码流程说明如下：
上电后，MCU作为PCDCACM设备和PC通信，在PC端显示为USB COM口，可以接收COM的输入数据，在COM上ECHO。

### 1.1 创建新工程，BSP选择“CPK-RA8D1B Core Board”，RTOS选择AzureRTOS。
### 1.2 Stack中添加“USB PCDCACM”，详细的属性设置请参考例程
### 1.3 利用一根USB线连接芯片的JDBG和PC，另一根USB线连接芯片的JUSB和PC。
### 1.4 在e2 studio中调试代码，代码自由运行。PC端打开资源管理器，可以看到已经识别为USB COM口，在串口工具如Tera Term中打开相应串口，随意键入一个字符，则该字符可以在本COM口ECHO回来：
![alt text](images/Picture1-1.png)

## 2. 支持的电路板：
CPKCOR-RA8D1B

## 3. 硬件要求：
1块瑞萨 RA核心板：CPKCOR-RA8D1B

2根Type-C -> Type-A USB 数据线，用于连接JDBG和PC。

## 4. 硬件连接：
通过Type-C USB 电缆将 CPKCOR-RA8D1B板上的 USB 调试端口（JDBG）连接到主机 PC。另一根USB线缆连接板上的JUSB和PC。