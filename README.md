# JiyuToolBox 极域工具箱

当前版本：v5.0

**[官网](https://jiyutool.liyixin.vip/)** | **[使用教程](https://jiyutool.liyixin.vip/how-to-use.html)** | **[问题反馈](https://jiyutool.liyixin.vip/feedback.html)**

一款面向 Windows 的极域电子教室（Mythware）学生端反控制辅助工具，使用 C++ / Win32 原生对话框编写。可以解除断网、解除 U 盘限制、退出黑屏、解除键盘锁、窗口化广播等，适用于学生机房 / 计算机教室环境。

⚠️ 本工具仅用于**学习研究**与**获得授权的环境**。请勿用于违反校规、法律法规的场景。

## 目录

- [功能](#功能)
- [快捷键](#快捷键)
- [使用说明](#使用说明)
- [编译](#编译)
- [参考项目](#参考项目)
- [许可证](#许可证)

## 功能

### 极域控制

- **启动 / 杀死极域**：未运行时降权启动（`CreateProcessWithTokenW`，普通用户权限）；运行时终止**全部**相关进程（StudentMain / MasterHelper / GATESRV）并停止服务，防止极域自动重启
- **挂起 / 恢复极域**：冻结 / 恢复极域进程，画面定格
- **退出黑屏**：仅针对「黑屏安静」窗口（BlackScreen Window），ESC → 隐藏 → 确认，不影响投屏
- **窗口化广播**：全屏广播 ↔ 窗口化双向切换，支持 `CTRL+Q` 快捷键
- **解除键盘锁**：循环低级键盘钩子对抗 + TDKeybd 驱动欺骗（IOCTL 0x220000），Ctrl+Alt+Del 可用
- **防止截屏**：窗口防截屏保护（WDA），防止教师端看到本程序

### 网络与设备

- **解除断网**：驱动欺骗（IOCTL 0x120014）+ 终止网关进程 + 停止限网驱动服务（不卸载）
- **解除 U 盘限制**：停止文件过滤驱动服务（不卸载）
- **恢复限制**：一键恢复网络 / U 盘限制，重新拉起 MasterHelper、GATESRV（STUDSRV 服务）

### 界面与辅助

- **置顶窗口**：`CTRL+W` 快捷键切换，检测到被遮挡才顶回，避免闪烁
- **更新检查**：启动时拉取远端版本号，有新版时提示

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `CTRL+Q` | 窗口化 / 全屏化广播（可在界面勾选启用） |
| `CTRL+W` | 切换置顶窗口（可在界面勾选启用） |

## 使用说明

1. **以管理员身份运行**（终止进程、操作驱动服务需要管理员权限）
2. 界面按功能分组：极域控制 / 高级工具 / 功能开关 / 快捷键

## 编译

- 环境：Visual Studio 2022+（MSVC x64）、Windows SDK
- 构建：打开 `JiyuToolBox.sln`，选择 **Release / x64** 编译
- 输出：`bin\x64\Release\JiyuToolBox.exe`
- 依赖：仅系统库（Win32 API），无第三方依赖

## 参考项目

本项目参考了以下开源项目，在此致谢：

- **[MythwareToolkit](https://github.com/BengbuGuards/MythwareToolkit)** 

## 许可证

[GPL-3.0](LICENSE)
