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

使用 Visual Studio 2022 或命令行：

```bash
dotnet build
```

## 运行

```bash
dotnet run --project src/HidConfigTool.App
```
