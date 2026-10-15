# 第三方库许可证清单

**项目名称**：Pico HID 复合设备
**审计日期**：2026-08-20
**审计范围**：固件第三方库、上位机 NuGet 依赖
**总体结论**：✅ 所有第三方库均使用宽松许可证，允许商业使用

---

## 一、许可证兼容性总览

| 许可证类型 | 数量 | 商业使用 | 修改 | 分发 | 专利授权 |  copyleft |
|-----------|------|---------|------|------|---------|-----------|
| MIT | 10 | ✅ 允许 | ✅ 允许 | ✅ 允许 | ❌ 无 | ❌ 无 |
| BSD 3-Clause | 2 | ✅ 允许 | ✅ 允许 | ✅ 允许 | ❌ 无 | ❌ 无 |
| Apache 2.0 | 2 | ✅ 允许 | ✅ 允许 | ✅ 允许 | ✅ 有 | ❌ 无 |

**结论**：所有第三方库均使用宽松许可证（MIT/BSD/Apache），**无 copyleft 许可证**，**允许商业使用、修改和分发**。

---

## 二、固件第三方库

### 2.1 Raspberry Pi Pico SDK

| 项目 | 信息 |
|-----|------|
| **名称** | Raspberry Pi Pico SDK |
| **版本** | 2.3.0 |
| **来源** | https://github.com/raspberrypi/pico-sdk |
| **许可证** | BSD 3-Clause License |
| **商业使用** | ✅ 允许 |
| **用途** | RP2350 硬件抽象层、GPIO、SPI、I2C、ADC、Flash、多核等 |

**包含的子模块**：
- TinyUSB（USB 设备栈）- MIT License
- pico_stdlib - BSD 3-Clause
- pico_unique_id - BSD 3-Clause
- pico_multicore - BSD 3-Clause
- pico_low_power - BSD 3-Clause
- hardware_gpio, hardware_spi, hardware_i2c, hardware_adc, hardware_flash 等 - BSD 3-Clause

**许可证声明**：
```
Copyright (c) 2020 Raspberry Pi (Trading) Ltd.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.
```

**合规要求**：
- ✅ 在产品文档中包含 Pico SDK 的版权声明和许可证文本
- ✅ 不得使用 Raspberry Pi 的名称为产品背书（除非获得授权）

---

### 2.2 TinyUSB

| 项目 | 信息 |
|-----|------|
| **名称** | TinyUSB |
| **版本** | 随 Pico SDK 2.3.0 发布 |
| **来源** | https://github.com/hathach/tinyusb |
| **许可证** | MIT License |
| **商业使用** | ✅ 允许 |
| **用途** | USB 设备栈（HID 类、描述符、端点管理等） |

**许可证声明**：
```
The MIT License (MIT)

Copyright (c) 2019 Ha Thach (tinyusb.org)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

**合规要求**：
- ✅ 在产品文档中包含 TinyUSB 的版权声明和许可证文本

---

### 2.3 Unity Test Framework

| 项目 | 信息 |
|-----|------|
| **名称** | Unity Test Framework |
| **版本** | 随项目源码包含 |
| **来源** | https://github.com/ThrowTheSwitch/Unity |
| **许可证** | MIT License |
| **商业使用** | ✅ 允许 |
| **用途** | 固件单元测试框架（仅测试时使用，不包含在发布固件中） |

**许可证声明**：
```
The MIT License (MIT)

Copyright (c) 2007-23 Mark VanderVoord, Greg Williams, Matt Fletcher

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

**说明**：Unity 仅用于开发和测试阶段，**不包含在发布的固件中**，因此对最终产品的许可证无影响。

---

## 三、上位机 NuGet 依赖

### 3.1 Avalonia UI 框架

| 项目 | 信息 |
|-----|------|
| **名称** | Avalonia |
| **版本** | 11.3.2 |
| **来源** | https://github.com/AvaloniaUI/Avalonia |
| **许可证** | MIT License |
| **商业使用** | ✅ 允许 |
| **用途** | 跨平台 UI 框架（Windows/macOS/Linux） |

**包含的包**：
- Avalonia 11.3.2
- Avalonia.Desktop 11.3.2
- Avalonia.Themes.Fluent 11.3.2
- Avalonia.Controls.DataGrid 11.3.2

**许可证声明**：
```
The MIT License (MIT)

Copyright (c) Avalonia UI Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

### 3.2 CommunityToolkit.Mvvm

| 项目 | 信息 |
|-----|------|
| **名称** | CommunityToolkit.Mvvm |
| **版本** | 8.4.2 |
| **来源** | https://github.com/CommunityToolkit/dotnet |
| **许可证** | MIT License |
| **商业使用** | ✅ 允许 |
| **用途** | MVVM 框架（ObservableObject、RelayCommand、源生成器等） |

**许可证声明**：
```
The MIT License (MIT)

Copyright (c) .NET Foundation and Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

### 3.3 Microsoft.Extensions.*

| 项目 | 信息 |
|-----|------|
| **名称** | Microsoft.Extensions.DependencyInjection |
| **版本** | 10.0.0 / 10.0.10 |
| **来源** | https://github.com/dotnet/runtime |
| **许可证** | MIT License |
| **商业使用** | ✅ 允许 |
| **用途** | 依赖注入容器 |

| 项目 | 信息 |
|-----|------|
| **名称** | Microsoft.Extensions.Hosting |
| **版本** | 10.0.10 |
| **来源** | https://github.com/dotnet/runtime |
| **许可证** | MIT License |
| **商业使用** | ✅ 允许 |
| **用途** | 应用程序宿主、生命周期管理 |

**许可证声明**：
```
The MIT License (MIT)

Copyright (c) .NET Foundation and Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

### 3.4 HidSharp

| 项目 | 信息 |
|-----|------|
| **名称** | HidSharp |
| **版本** | 2.1.0 |
| **来源** | https://www.zer7.com/software/hidsharp |
| **许可证** | Apache License 2.0 |
| **商业使用** | ✅ 允许 |
| **用途** | 跨平台 HID 设备访问（Windows/macOS/Linux） |

**许可证声明**：
```
Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/

Copyright 2012-2024 James F. Bellinger

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

**合规要求**：
- ✅ 在产品文档中包含 HidSharp 的版权声明和许可证文本
- ✅ 如修改了 HidSharp 源码，需在修改文件中注明
- ✅ Apache 2.0 包含专利授权，使用更安全

---

### 3.5 测试框架（仅开发/测试时使用）

> **说明**：以下包仅用于开发和测试阶段，**不包含在发布的应用程序中**，因此对最终产品的许可证无影响。

| 名称 | 版本 | 许可证 | 商业使用 | 用途 |
|-----|------|--------|---------|------|
| xunit | 2.9.3 | Apache 2.0 | ✅ 允许 | 单元测试框架 |
| xunit.runner.visualstudio | 3.1.4 | MIT | ✅ 允许 | xUnit 测试运行器 |
| Moq | 4.20.72 | BSD 3-Clause | ✅ 允许 | Mock 框架 |
| Microsoft.NET.Test.Sdk | 17.14.1 | MIT | ✅ 允许 | .NET 测试 SDK |
| coverlet.collector | 6.0.4 | MIT | ✅ 允许 | 代码覆盖率收集器 |

---

## 四、许可证兼容性分析

### 4.1 宽松许可证兼容性矩阵

| 许可证 | MIT | BSD 3-Clause | Apache 2.0 |
|-------|-----|-------------|------------|
| **MIT** | ✅ 兼容 | ✅ 兼容 | ✅ 兼容 |
| **BSD 3-Clause** | ✅ 兼容 | ✅ 兼容 | ✅ 兼容 |
| **Apache 2.0** | ✅ 兼容 | ✅ 兼容 | ✅ 兼容 |

**结论**：本项目使用的所有许可证（MIT、BSD 3-Clause、Apache 2.0）互相兼容，可以安全地组合使用。

### 4.2 商业使用风险评估

| 风险项 | 评估 | 说明 |
|-------|------|------|
| copyleft 风险 | ✅ 无风险 | 无 GPL/LGPL/AGPL 等 copyleft 许可证 |
| 专利风险 | ✅ 低风险 | Apache 2.0 包含明确的专利授权；MIT/BSD 未明确但无专利诉讼历史 |
| 署名要求 | ⚠️ 需注意 | 所有许可证都要求保留版权声明和许可证文本 |
| 商标使用 | ⚠️ 需注意 | BSD 3-Clause 禁止使用版权持有者名称背书产品 |
| 测试依赖 | ✅ 无风险 | 测试框架仅在开发时使用，不包含在发布产品中 |

### 4.3 发布产品时的合规要求

1. **文档中包含许可证声明**
   - 在产品手册、关于页面、或附带的文档中包含所有第三方库的版权声明和许可证文本
   - 建议创建 `THIRD_PARTY_LICENSES.txt` 文件随产品分发

2. **二进制分发时的注意事项**
   - .NET 应用程序发布时，NuGet 包的 DLL 会随应用程序一起分发
   - 确保所有 NuGet 包的许可证允许二进制分发（本项目所有包均允许）

3. **修改第三方库时的注意事项**
   - 如修改了任何第三方库的源码，需在修改文件中注明修改内容
   - Apache 2.0 要求保留修改记录
   - MIT/BSD 无此要求，但建议注明

4. **商标使用**
   - 不得使用 Raspberry Pi、Microsoft、Avalonia 等名称为产品背书
   - 可以说"基于 Raspberry Pi Pico 开发"，但不能暗示获得官方认证

---

## 五、建议的合规操作

### 5.1 立即执行

1. ✅ **创建 `THIRD_PARTY_LICENSES.md` 文件**（本文件）
2. ✅ **在产品关于页面中添加第三方库致谢**
3. ✅ **在发布包中包含许可证文本**

### 5.2 后续维护

1. **定期审计依赖**
   - 每次添加新的 NuGet 包或固件库时，检查其许可证
   - 建议在 CI 中添加许可证检查步骤（如使用 `dotnet-project-licenses` 工具）

2. **更新版本时重新检查**
   - 第三方库版本更新时，许可证可能发生变化
   - 重大版本升级时需重新审计

3. **监控许可证变更**
   - 关注所用库的许可证变更公告
   - 如某库从宽松许可证改为 copyleft，需及时处理

### 5.3 CI 集成建议

可在 CI 中添加自动许可证检查：

```yaml
- name: Check NuGet licenses
  run: |
    dotnet tool install -g dotnet-project-licenses
    dotnet-project-licenses -i pc_tool/HidConfigTool.slnx -o licenses.json
    # 检查是否有 GPL/LGPL/AGPL 等不兼容许可证
```

---

## 六、参考链接

### 固件库
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [TinyUSB](https://github.com/hathach/tinyusb)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)

### 上位机库
- [Avalonia UI](https://avaloniaui.net/)
- [CommunityToolkit.Mvvm](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/)
- [Microsoft.Extensions](https://learn.microsoft.com/dotnet/core/extensions/)
- [HidSharp](https://www.zer7.com/software/hidsharp)
- [xUnit](https://xunit.net/)
- [Moq](https://github.com/moq/moq)
- [coverlet](https://github.com/coverlet-coverage/coverlet)

### 许可证信息
- [MIT License](https://opensource.org/licenses/MIT)
- [BSD 3-Clause License](https://opensource.org/licenses/BSD-3-Clause)
- [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
- [Choose a License](https://choosealicense.com/)

---

## 七、审计结论

### 7.1 总体评估

| 评估项 | 结果 |
|-------|------|
| 第三方库总数 | 14 个（固件 3 个，上位机 11 个） |
| 许可证类型 | MIT (10)、BSD 3-Clause (2)、Apache 2.0 (2) |
| copyleft 许可证 | ❌ 无 |
| 商业使用兼容性 | ✅ 全部兼容 |
| 总体风险等级 | ✅ 低风险 |

### 7.2 最终结论

✅ **本项目所有第三方库均使用宽松许可证（MIT/BSD/Apache），允许商业使用、修改和分发。无 copyleft 许可证，无许可证兼容性问题。**

**发布商业产品时需注意**：
1. 在产品文档中保留所有第三方库的版权声明和许可证文本
2. 不得使用第三方库的版权持有者名称为产品背书
3. 如修改了第三方库源码，需在修改文件中注明（Apache 2.0 要求）

**建议**：
- 在产品发布包中包含本文件（或其简化版）
- 在 CI 中添加自动许可证检查步骤
- 定期审计依赖，特别是在重大版本升级时

---

**审计完成日期**：2026-08-20
**审计人员**：开源合规专家（AI 辅助）
**文档版本**：v1.0
