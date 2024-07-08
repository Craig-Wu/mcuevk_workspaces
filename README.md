## 瑞萨MCU EVK 样例代码提交仓库

本仓库专门用于EVK样例代码的提交，不作为对外发布用。 如果您希望为瑞萨的EVK提供样例代码或参考应用代码，可以向瑞萨申请本仓库的合作者权限。

作为合作者，您可以直接clone整个仓库，并直接拉取/推送代码。推送代码前请测试代码的准确性和可用性。

样例代码的提交分为三个主要步骤。

### I. 查看瑞萨已经发布的样例代码，避免重复发布，请先查看以下页面：
1. [瑞萨官网](www.renesas.com)上各个产品页面上已经发布的样例代码
2. [Github](https://github.com/renesas/ra-fsp-examples/tree/master/example_projects)和[Gitee](about:blank)上的RA FSP Example
   - [RA FSP Example的Github Release页面](https://github.com/renesas/ra-fsp-examples/releases)上还有对应各个FSP历史版本的样例代码
3. [Github](https://github.com/renesas/cpk_examples)或[Gitee镜像](https://gitee.com/recn-mcu-ae/cpk_examples)上已发布的 CPK Example。
4. 本代码仓库，以确认是否已经有人认领某个样例代码的开发
   
### II. 创建样例代码目录，声明该样例代码已有人在负责编写测试
1. 直接clone/拉取本仓库(确保您看到的是最新版本的代码)
2. 在对应的目标板目录下(例如cpkcor_ra8d1b)创建样例代码的目录
3. 在样例代码目录下创建readme.md，声明您已经在开发中，并先将readme.md推送到远程仓库
4. 开发和调试您的样例代码

详见[样例代码开发提交流程](docs/project_handling.md)
   
### III. 上传/推送开发完成的样例代码
1. 编写详细的样例代码和示例工程使用指南，您可以参考代码仓库中的其他样例
2. 修改readme.md，表示该样例代码已开发完成。
3. 推送样例代码到远程仓库(本仓库)，开发完成
   - 推送前请先拉取最新的仓库文件

详见[样例代码开发提交流程](docs/project_handling.md)
   
瑞萨相关人员看到readme.md更新到完成状态后，会将该样例代码发布到[Github](https://github.com/renesas/cpk_examples)和[Gitee](about:blank)上的 CPK Examples，并
${\color{red}{\text{删除本代码仓库下的样例代码目录}}}$
。

如有必要，您可以在本地电脑保留一个样例代码备份，或去Github和Gitee上下载已发布的样例代码。
