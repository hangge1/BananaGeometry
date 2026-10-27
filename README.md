# Drawer

轻量的 Windows 二维绘图与人机交互测试工具。每个绘制用例都是一个完整的 Lua 交互演示，可以定义自己的控件、默认模式、图形数据与鼠标键盘行为。

## 当前状态

截至 2026-10-27，已实现首版 Windows 宿主、Lua SDK、三个示例用例和 CTest 测试。阶段验证与限制见[开发日志](docs/development-log.md)。

## 文档入口

| 文档 | 内容 |
| --- | --- |
| [需求规格](docs/requirements.md) | 已确认的功能、使用流程、首版范围与验收要求 |
| [架构与 SDK 设计](docs/architecture.md) | 技术组合、模块职责、状态模型、插件与 SDK 接口草案 |
| [决策记录](docs/decisions.md) | 用户已做出的选择、备选方案和取舍理由 |
| [实施计划](docs/implementation-plan.md) | 分阶段任务、依赖关系与验证清单 |
| [贡献指南](AGENTS.md) | 后续开发、文档维护与验证约定 |
| [开发规则](docs/development-rules.md) | 项目内 skill、Git、日志和测试原则 |
| [代码与注释规范](docs/coding-style.md) | Google C++、Lua 工程风格、注释契约与检查规则 |
| [模块与目录](docs/modules.md) | 实际文件组织、模块边界、依赖方向与状态所有权 |
| [SDK 使用说明](docs/sdk.md) | 新增 Lua 用例、图形、控件、事务与输入 API |
| [测试与验收](docs/testing.md) | 自动化结果、测量与尚需人工验收的项目 |
| [本轮人工验收](docs/manual-acceptance.md) | 多边形、可变阶数贝塞尔及交互式 Gallery 的验收步骤 |
| [验证工具与产物](docs/validation.md) | 可复用验证入口、场景、运行报告和临时文件管理 |
| [增量版本记录](docs/version-history.md) | 首版分步提交与每步独立验证证据 |

## 方案摘要

- **运行平台**：仅 Windows，独立桌面程序。
- **技术组合**：C++、Win32、Direct3D 11、Dear ImGui、Lua。
- **主要第三方依赖**：Dear ImGui 和标准 Lua；CMake 自动获取固定版本并校验哈希，缓存位于构建目录，第三方源码不入仓库。
- **扩展方式**：Lua 用例插件，修改后通过 F5 或按钮重新加载，无需编译用例。
- **画布模型**：多标签、实例隔离、无限二维视图，统一采用 X 向右、Y 向上的数学坐标。
- **核心功能**：新建、清空、保存、加载、切换用例、撤销、重做。

## 后续维护

需求变化先更新对应文档，并在决策记录中说明影响。已实现 API 见 SDK 使用说明，架构中的概念接口以实际说明为准。早期贡献指南保留了文档基线时的状态描述；当前构建和开发规则以本 README 及开发规则为准。

## 构建、测试与运行

需要 Windows 10/11 x64、Visual Studio 2022 的 C++ 桌面开发工具和 Windows SDK，以及 CMake 3.24+。运行生成的程序无需安装 Lua、Qt、Python 或 .NET。

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Debug --parallel
ctest --test-dir build/windows -C Debug --output-on-failure
.\build\windows\bin\Debug\drawer.exe
```

首次配置自动下载固定版本的 ImGui、Lua 及单元测试依赖 Catch2，后续复用缓存。`-DBUILD_TESTING=OFF` 可只构建应用；性能框架默认关闭。离线复用方式见[依赖策略](third_party/README.md)。

也可以使用一键脚本；所有步骤顺序执行，测试失败时停止：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release -Package
.\out\Drawer\drawer.exe
```

`out/Drawer` 包含程序、Lua 用例、SDK、模板、文档及许可证，不包含依赖源码或编译器。GitHub Actions 工作流位于 `.github/workflows/windows.yml`，通过远程推送触发，运行结果另行确认。

## 快速使用

- 左侧“新建实例”或顶部“新建画布”选择用例；每个标签数据独立。
- 多边形设置 3–256 个顶点，或自由落点后 Enter / 右键完成；贝塞尔设置 1–32 阶，落下阶数加一个控制点。
- Gallery 从空画布选择图元交互绘制，可设置点数、阶数、文字、大小和样式；“插入演示图元”提供参考场景。
- 调整模式单击图形后显示金色高亮和控制点；Gallery 的文字可在整个文字区域内按下拖动，Esc 取消。
- 拖图形主体整体平移，拖控制点调整形状；Alt 强制平移，Shift 锁定方向，方向键微移。
- 中键或空格+左键平移，滚轮缩放，Home 适应内容，Esc 取消，Delete 删除。
- Ctrl+N/O/S/Z/Y：新建、加载、保存、撤销、重做；Ctrl+Shift+S 另存为。
- 仓库内运行直接编辑根目录的 `cases/<用例>/main.lua`；独立发布编辑 exe 旁的 `cases/`。底部显示实际脚本路径并可复制。
- “应用脚本修改 F5”重新读取当前用例及模块，重置当前画布和撤销历史；未保存时先提示，失败保留原实例。
- “更新用例列表”只扫描名称及新增/移除项，已打开的画布保持原样。可用 `drawer.exe --workspace <目录>` 显式选择含 `sdk/`、`cases/` 的工作区。
- 文档扩展名为 `.drawer.json`，加载到新标签，不保留撤销历史。

## 自动化验证

Catch2 + CTest 包含 82 个值、历史、视图、Lua、文档、千级对象、脚本重载及画布输入行为用例，以及一个原生窗口检查。格式与边界检查见[代码规范](docs/coding-style.md)。

需要性能报告时单独启用 Google Benchmark；它不链接到应用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release -Benchmarks
```

该命令执行 84 项 CTest 检查，并测量 100/1000 个对象下的文档、历史、Lua 提取和命中。JUnit 与性能 JSON 按运行编号保存到 `artifacts/validation/`。完整工程验证可运行 `tools/validate.ps1 -Configuration Release -Benchmarks -Screenshots -Package`，详见[验证工具](docs/validation.md)。

可复现窗口检查与截图：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validation/capture-screenshots.ps1 -Executable build/windows/bin/Debug/drawer.exe -Scene polygon-selection
```

截图和临时输入由脚本按运行编号管理。系统保存/加载对话框、跨显示器 DPI 等手工验收项目与本地自动化结果分别记录。
