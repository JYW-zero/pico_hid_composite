# HID配置工具

Windows 平台的 HID 复合设备配置工具。

## 功能特性

- 设备识别与连接
- 按键映射配置（普通层 + Fn层）
- 鼠标DPI配置
- 摇杆死区配置
- 编码器方向配置
- 宏功能配置与录制
- 性能监控
- 错误日志查看
- 按键统计

## 文档

完整文档请查看项目根目录的 [docs/](../../docs/) 文件夹。

## 编译

跨平台配置工具（Mac / Windows）：

```bash
cd pc_tool
dotnet run --project src/HidConfigTool.Desktop
```

Windows 专用 WPF 版：

```bash
dotnet build src/HidConfigTool.App
```

## 运行

```bash
dotnet run --project src/HidConfigTool.App
```

## 联系作者

- **作者**：JYW
- **邮箱**：[J.YW@outlook.com](mailto:J.YW@outlook.com)
