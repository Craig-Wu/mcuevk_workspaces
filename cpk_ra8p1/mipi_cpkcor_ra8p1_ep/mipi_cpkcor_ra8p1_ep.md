## 1.参考例程概述
该示例项目演示了基于瑞萨 RA8P1 MIPI 驱动的功能

### 1.1 打开工程
### 1.2 可以使用这三个参数显示不同的颜色 pattern
```
typedef enum
{
    simple = 0, 
    partition = 1,
    gradient = 2
} color_pattern_t;
```
![alt text](images/code.jpg)

### 1.3 连接屏幕，如下：

![alt text](images/10inch.png)

### 1.4 编译，下载，运行


## 2. 修改参数，适配 10inch 屏

### 2.1 修改 mipi_dsi_ep.c 第 55 行的代码，改成 1 ，将配置 10inch 的参数

![alt text](images/10inch_config.jpg)

### 2.2 修改 FSP 中 glcdc stack 中的参数，具体要改分辨率大小，以及 glcdc 的 timing。这里列出 10inch 的配置，大家自行参考


10.1inch:

![alt text](images/10inch_resolution.jpg)

![alt text](images/10inch_timing.jpg)



## 3. 支持的电路板：
CPKCOR-RA8P1

## 4. 硬件要求：
1块瑞萨 RA8P1 COR板：CPKCOR-RA8P1

1根 Type-C USB 数据线

1块 10.1inch 屏

## 5. 硬件连接：
通过Type-C USB 数据线将 CPKCOR-RA8P1板上的 USB 调试端口（JDBG）连接到主机 PC
连接屏幕到板子