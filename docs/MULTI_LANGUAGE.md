# 多语言支持（中文/英文）

## 概述

上位机支持中文和英文两种语言，可在运行时切换，无需重启软件。所有 UI 文本（按钮、标签、提示、错误消息）均通过资源文件管理。

## 资源文件结构

```
windows/HidConfigTool.App/Resources/
├── Strings.zh-CN.resx    # 中文资源
└── Strings.en.resx        # 英文资源
```

### 资源键命名规范

| 前缀 | 用途 | 示例 |
|------|------|------|
| `Nav_` | 导航菜单 | `Nav_Device`, `Nav_Settings` |
| `Btn_` | 按钮文本 | `Btn_Save`, `Btn_Reset` |
| `Label_` | 标签文本 | `Label_Theme`, `Label_Language` |
| `Status_` | 状态消息 | `Status_Connected`, `Status_Saving` |
| `Msg_` | 提示消息 | `Msg_SaveSuccess`, `Msg_DeleteConfirm` |
| `Title_` | 页面/对话框标题 | `Title_Help`, `Title_About` |
| `Theme_` | 主题选项 | `Theme_Dark`, `Theme_Light` |
| `Lang_` | 语言选项 | `Lang_Chinese`, `Lang_English` |

## 核心机制

### LanguageManager

位置：`windows/HidConfigTool.App/Services/LanguageManager.cs`

- 管理当前语言设置
- 持久化语言选择到 `%AppData%\HIDConfigTool\language.txt`
- 提供 `GetString(key)` 静态方法获取本地化字符串
- 通过 `LanguageChanged` 事件通知语言变化

### LocalizationService

位置：`windows/HidConfigTool.App/Localization/LocalizationService.cs`

- 实现 `INotifyPropertyChanged`
- 提供索引器 `this[string key]`，用于 XAML 绑定
- 单例模式，通过 `LocalizationService.Instance` 访问
- 订阅 `LanguageManager.LanguageChanged` 事件，语言切换时自动通知所有绑定刷新

## XAML 中使用本地化

### 1. 注册命名空间

在 XAML 文件顶部添加：

```xml
xmlns:loc="clr-namespace:HidConfigTool.App.Localization"
```

### 2. 绑定静态文本

使用 `{Binding [资源键], Source={x:Static loc:LocalizationService.Instance}}` 方式绑定：

```xml
<!-- 按钮文本 -->
<Button Content="{Binding [Btn_Save], Source={x:Static loc:LocalizationService.Instance}}" />

<!-- 标签文本 -->
<TextBlock Text="{Binding [Label_Theme], Source={x:Static loc:LocalizationService.Instance}}" />

<!-- 窗口标题 -->
<Window Title="{Binding [App_Title], Source={x:Static loc:LocalizationService.Instance}}" />
```

### 3. 使用 StaticResource（推荐，更简洁）

由于在 `App.xaml` 中已注册 `LocalizationService` 为静态资源，可以使用：

```xml
<Button Content="{Binding [Btn_Save], Source={StaticResource LocalizationService}}" />
```

### 4. 带参数的本地化字符串

对于需要动态参数的文本，在 ViewModel 中处理：

```csharp
// 资源文件中定义：Msg_SaveSuccess = "配置已保存到 {0}"
public string SaveSuccessMessage =>
    LocalizationService.Instance.GetString("Msg_SaveSuccess", DeviceName);
```

## ViewModel 中使用本地化

### 1. 直接获取字符串

```csharp
using HidConfigTool.App.Localization;

// 获取简单字符串
string saveText = LocalizationService.Instance.GetString("Btn_Save");

// 获取带参数的字符串
string message = LocalizationService.Instance.GetString("Msg_SaveSuccess", deviceName);
```

### 2. 动态属性（语言切换时自动刷新）

对于需要在 UI 中显示的动态属性，订阅 `PropertyChanged` 事件：

```csharp
public class MyViewModel : ObservableObject, IDisposable
{
    private bool _disposed;

    public MyViewModel()
    {
        // 订阅语言变化事件
        LocalizationService.Instance.PropertyChanged += OnLanguageChanged;
    }

    private void OnLanguageChanged(object? sender, PropertyChangedEventArgs e)
    {
        // 语言变化时刷新所有本地化属性
        OnPropertyChanged(nameof(Title));
        OnPropertyChanged(nameof(StatusMessage));
    }

    public string Title => LocalizationService.Instance.GetString("Title_MyPage");

    public string StatusMessage => LocalizationService.Instance.GetString("Status_Ready");

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        LocalizationService.Instance.PropertyChanged -= OnLanguageChanged;
    }
}
```

### 3. 使用 LanguageManager 静态方法

```csharp
using HidConfigTool.App.Services;

string text = LanguageManager.GetString("Btn_Save");
```

## 语言切换

### 在设置页面切换

设置页面提供语言下拉框，选择后立即切换：

```csharp
// SettingsPageViewModel.cs
partial void OnLanguageChanged(string value)
{
    string language = value == "English" ? LanguageConstants.English : LanguageConstants.Chinese;
    _languageManager.SetLanguage(language);
}
```

### 代码中切换

```csharp
// 切换到中文
LocalizationService.Instance.SetChinese();

// 切换到英文
LocalizationService.Instance.SetEnglish();

// 或直接设置
LocalizationService.Instance.SetLanguage("zh-CN");
```

## 添加新的资源键

### 1. 在中文资源文件中添加

编辑 `Strings.zh-CN.resx`：

```xml
<data name="Btn_NewFeature" xml:space="preserve">
    <value>新功能</value>
</data>
```

### 2. 在英文资源文件中添加

编辑 `Strings.en.resx`：

```xml
<data name="Btn_NewFeature" xml:space="preserve">
    <value>New Feature</value>
</data>
```

### 3. 在 XAML 中使用

```xml
<Button Content="{Binding [Btn_NewFeature], Source={StaticResource LocalizationService}}" />
```

## 语言持久化

语言选择会自动保存到：

```
%AppData%\HIDConfigTool\language.txt
```

文件内容为语言代码（`zh-CN` 或 `en`）。下次启动时自动加载。

## 注意事项

1. **运行时切换**：使用 `LocalizationService` 绑定的文本会在语言切换时自动刷新，无需重启软件
2. **ViewModel 动态属性**：需要手动订阅 `PropertyChanged` 事件并刷新属性
3. **资源键命名**：遵循命名规范，使用前缀分类
4. **缺失资源**：如果资源键不存在，会返回键名本身，便于调试
5. **带参数的字符串**：在 ViewModel 中使用 `GetString(key, args)` 方法

## 迁移指南

将硬编码文本迁移到本地化资源：

### 之前

```xml
<Button Content="保存" />
<TextBlock Text="主题：" />
```

### 之后

```xml
<Button Content="{Binding [Btn_Save], Source={StaticResource LocalizationService}}" />
<TextBlock Text="{Binding [Label_Theme], Source={StaticResource LocalizationService}}" />
```

### ViewModel 中的硬编码消息

### 之前

```csharp
StatusMessage = "配置保存成功";
```

### 之后

```csharp
StatusMessage = LocalizationService.Instance.GetString("Msg_SaveSuccess");
```
