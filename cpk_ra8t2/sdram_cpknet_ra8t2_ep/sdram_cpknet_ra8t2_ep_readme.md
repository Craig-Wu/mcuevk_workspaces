## 1.参考例程概述
该示例项目演示了基于瑞萨 RA8T2 SDRAM 性能测试的功能，本工程通过 DWT counter 计算读写 SDRAM 的时间，并通过 J-Link RTT 打印输出对应的结果。
![alt text](images/image3.png)
### 1.1 打开工程


### 1.2 编译，下载，运行


## 2. 结果分析

### 2.1 读写速率测试
<font color="red">发现把 D-Cahce 打开的时候，会有读写一致问题。因为这批板上用的是 RA8T2 用的是 WS2 的批次的芯片。据说是 Cache 还有一些问题。所以后续会在 ES3 批次的芯片通一步验证!</font>

* 打开 BENCH_MARK 这个宏定义会对 SDRAM 的读写速率进行测试，关闭这个宏定义会对 SDRAM 做全功能的读写测试
![alt text](images/image1.png)


## 4. 支持的电路板：
CPKNET-RA8T2

## 5. 硬件要求：
1 块瑞萨 RA8D1 HMI板：CPKNET-RA8T2

1 根 Type-C USB 数据线

## 6. 硬件连接：
通过 Type-C USB 数据线将 CPKNET-RA8T 板上的 USB 调试端口（JDBG）连接到主机 PC
连接屏幕到板子