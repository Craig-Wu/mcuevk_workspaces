## 1.参考例程概述
该示例项目演示了基于瑞萨 FSP 的瑞萨 RA MCU在FreeRTOS下作为PHID的基本功能。当代码运行时，RA8D1可以作为USB PHID设备和主机通信。
代码流程说明如下：
上电后，MCU作为PHID设备和PC通信，在PC端打开设备管理器，可以看到识别为HID设备。

### 1.1 创建新工程，BSP选择“CPK-RA8D1B Core Board”，RTOS选择FreeRTOS。
### 1.2 Stack中添加“USB PHID”，详细的属性设置请参考例程
### 1.3 利用一根USB线连接芯片的JUSB和PC。
### 1.4 在e2 studio中调试代码，代码自由运行。PC端打开设备管理器，可以看到接入了HID设备，根据RTT Viewer中的提示信息，打开本地Notepad，在RTT Viewer中键入任一字符，则会在Notepad中打印a～z1～0：
![alt text](images/Picture1-1.png)
MCU作为PHID时，会在RTT Viewer接收到的输入数据后，将一串数据（a~z1~0）发给Host（PC），因此会在PC端Notepad显示这些内容。

## 2. 支持的电路板：
CPKCOR-RA8D1B

## 3. 硬件要求：
1块瑞萨 RA核心板：CPKCOR-RA8D1B

2根Type-C USB 数据线，用于连接JDBG和PC。

## 4. 硬件连接：
通过Type-C USB 电缆将 CPKCOR-RA8D1B板上的 USB 调试端口（JDBG）连接到主机 PC。另一根USB线缆连接板上的JUSB和PC。