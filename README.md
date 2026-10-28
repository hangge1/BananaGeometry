# Drawer

轻量的 Windows 二维绘图与人机交互测试工具。每个绘制用例都是一个完整的 Lua 交互演示，可以定义自己的控件、默认模式、图形数据与鼠标键盘行为。

## 当前状态

截至 2026-10-28，已实现菜单、文档/实例/对象管理树、右侧属性、多实例文档，以及 Lua SDK、三个示例、模式反馈和普通/平滑闭合贝塞尔。本轮 UI 待人工验收；验证与限制见[开发日志](docs/development-log.md)。

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
| [用例目录与编辑](docs/case-directories.md) | 统一加载、记住目录、编辑当前脚本 |
| [交互模式反馈](docs/interaction-mode-proposal.md) | 模式选中状态、画布标识、动态提示和十字光标 |
| [菜单与多实例文档](docs/workspace-ui-plan.md) | 布局、管理树、属性、兼容保存与管理历史 |
| [贝塞尔闭合方案](docs/bezier-closure-proposal.md) | 开闭创建与普通闭合接点编辑 |
| [贝塞尔平滑接点](docs/bezier-smooth-proposal.md) | 四阶起的平滑闭合、切向联动与数据契约 |
| [测试与验收](docs/testing.md) | 自动化结果、测量与尚需人工验收的项目 |
| [本轮人工验收](docs/manual-acceptance.md) | 多边形、可变阶数贝塞尔及交互式 Gallery 的验收步骤 |
| [验证工具与产物](docs/validation.md) | 可复用验证入口、场景、运行报告和临时文件管理 |
| [增量版本记录](docs/version-history.md) | 首版分步提交与每步独立验证证据 |

## 方案摘要

- **运行平台**：仅 Windows，独立桌面程序。
- **技术组合**：C++、Win32、Direct3D 11、Dear ImGui、Lua。
- **主要第三方依赖**：Dear ImGui 和标准 Lua；CMake 自动获取固定版本并校验哈希，缓存位于构建目录，第三方源码不入仓库。
- **扩展方式**：Lua 用例插件，修改后通过 F5 或用例菜单重新加载，无需编译用例。
- **画布模型**：文档标签、每文档多实例、实例隔离、无限二维视图，统一采用 X 向右、Y 向上的数学坐标。
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

- 顶部按文件、编辑、用例、视图、帮助分组。文件 → 新建文档创建空文档；用例 → 添加用例实例选择演示，每个实例独立画布和历史。
- 左侧树按文档 → 实例 → 对象组织，单击切换，双击对象定位，右键提供属性、删除等命令。右侧属性承接名称、样式、文字、控制点及原用例工具；树选择对象进入已声明的调整模式。
- 启动、加载文件、切换文档标签或点击文档节点时仅预览，不自动启用实例交互；先选实例或对象再绘制/调整。预览允许视图平移、缩放，使用箭头光标并停用 F5、删除、清空和实例撤销；底部明确显示交互目标或预览实例。
- 多边形设置 3–256 个顶点，或自由落点后 Enter / 右键完成；贝塞尔设置 1–32 阶，落下阶数加一个控制点。
- 贝塞尔和 Gallery 可选择闭合曲线（3–32 阶），点击阶数个独立控制点自动回到起点；调整面板提供闭合/断开，拖动闭合接点时首尾联动。
- 闭合后可勾选“平滑接点”（4–32 阶），四阶只需点击三个独立点。两侧切向镜像联动，拖接点连同两侧一起移动；取消平滑保留形状，断开同时解除平滑。默认仍为普通闭合。
- 贝塞尔采样点默认关闭，需要观察采样算法时在用例工具中勾选“显示采样点”，再设置数量。
- Gallery 从空画布选择图元交互绘制，可设置点数、阶数、文字、大小和样式；“插入演示图元”提供参考场景。
- 调整模式单击图形后显示金色外侧选择框和空心控制点，保留实际描边、填充及文字颜色，改色无需取消选择；Gallery 的文字可在整个文字区域内按下拖动，Esc 取消。
- 模式按钮标记“当前”，画布左上角显示模式和下一步提示；绘制时画布内显示十字，临时平移显示移动光标。重复点击当前模式不取消操作。
- 拖图形主体整体平移，拖控制点调整形状；Alt 强制平移，Shift 锁定方向，方向键微移。
- 中键或空格+左键平移，滚轮缩放，Home 适应内容，Esc 取消，Delete 删除。
- Ctrl+N/O/S：新建、加载、保存整个当前文档；Ctrl+Shift+S 另存为。Ctrl+Z/Y 操作当前实例；实例增删及文档/实例重命名使用编辑 → 撤销/重做文档管理。
- 开发和发布都用“打开用例目录”选择脚本来源；开发本项目时选择仓库根目录的 `cases/`。程序记住选择，“使用内置用例”返回 exe 旁示例。
- 用例 → 编辑当前实例脚本打开绑定的 `main.lua`，没有编辑关联时使用记事本；视图 → 诊断可查看完整目录与路径。
- 用例 → 应用脚本修改并重置当前实例（F5）重新读取其脚本与模块，仅重置该实例的数据和历史；未保存时先提示，失败保留原实例。
- “更新用例列表”只扫描当前目录，已打开画布和 F5 的来源保持原样。`drawer.exe --cases-dir <目录>` 覆盖本次启动；外部用例无需复制 SDK。详见[用例目录](docs/case-directories.md)。
- 文档扩展名 `.drawer.json`，版本 2 保存文档名称和全部实例，采用两空格缩进的 UTF-8 JSON。版本 1 导入为单实例，首次保存默认另存；缺失插件保留原始数据，可选择对应目录后重试。加载到新标签，各实例历史重新开始。
- 视图菜单可隐藏管理器、属性或诊断；拖动分隔条调整宽度，恢复默认布局可复位。布局仅记在本机，不写入文档。
- 可用 `drawer.exe --document <文件路径>` 直接打开文档；插件仅从当前明确选择的用例目录匹配。

## 自动化验证

Catch2 + CTest 覆盖值、历史、视图、Lua、多实例文档、管理撤销、属性、曲线、千级对象、模式、偏好和画布输入，以及原生窗口检查。当前数量与结果见[测试记录](docs/testing.md)；格式与边界检查见[代码规范](docs/coding-style.md)。

需要性能报告时单独启用 Google Benchmark；它不链接到应用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release -Benchmarks
```

该命令增加性能入口检查，并测量 100/1000 个对象下的文档、历史、Lua 提取和命中。JUnit 与性能 JSON 按运行编号保存到 `artifacts/validation/`。完整工程验证可运行 `tools/validate.ps1 -Configuration Release -Benchmarks -Screenshots -Package`，详见[验证工具](docs/validation.md)。

可复现窗口检查与截图：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validation/capture-screenshots.ps1 -Executable build/windows/bin/Debug/drawer.exe -Scene polygon-selection
```

截图和临时输入由脚本按运行编号管理。系统保存/加载对话框、跨显示器 DPI 等手工验收项目与本地自动化结果分别记录。
