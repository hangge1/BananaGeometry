# 模块与目录

## 原生模块

依赖只能从应用流向基础能力；“边界清晰”指通过公开接口协作、明确状态所有权，不要求模块没有依赖。

| 路径 / CMake 目标 | 职责与公开接口 | 可依赖的项目模块 |
| --- | --- | --- |
| `src/core/` / `drawer_core` | 通用值、JSON、几何与视图、历史、文档校验和编解码 | 无；仅 C++ 标准库 |
| `src/runtime/` / `drawer_runtime` | `Session` 生命周期、独立 Lua VM、事务和事件；`detail/` 负责栈转换 | core；外部依赖 Lua |
| `src/platform/` / `drawer_platform` | 文档原子读写、Windows 文件对话框 | core；Windows API |
| `src/render/` / `drawer_render` | `RenderCanvas` 将图元和视图绘制到 ImGui | core；ImGui |
| `src/app/` / `drawer_app` | 标签与命令编排、控件适配、画布输入适配 | runtime、platform、render、core |
| `src/main.cpp` / `drawer` | 程序组合入口、Win32 窗口、D3D11 设备和后端生命周期 | app、ImGui 后端、系统图形 API |

原生实现按职责分文件：文档模型不读写文件，渲染器不运行用例，运行时不调用 ImGui，系统对话框不管理标签。工作区中的 `Application` 协调流程；`CanvasView` 仅借用会话处理输入；`DrawLuaWidget` 只接收与返回通用值。

`render/text_metrics.*` 测量当前渲染字体，`CanvasView` 通过 `Session::SetTextMeasurer` 注入像素尺寸回调；Lua 对象模块将尺寸换算为世界坐标边界。运行时不包含字体或界面 API，选择覆盖层不写入文档。

`runtime/session_feedback.cpp` 在 Lua 预算内获取通用模式信息并提供单调时间；`app/canvas_feedback.*` 绘制非交互模式标签和软件十字。反馈由 `ctx.mode` 和 SDK 状态派生，不另存一份界面模式，也不进入文档历史。

交互模块通过 `Session::GetPointerHint` 提供语义提示，由 `CanvasView` 映射光标；运行时不持有界面状态。`Session::ExecuteFile` 负责可信验证夹具的受限执行，`Application::SmokeCheck` 负责用例选择，桌面入口仅处理参数与图像捕获。

`app/workspace_paths.*` 按显式参数、记忆、内置目录选择路径；`app/case_library.*` 扫描并校验用例；`Application` 管理画布来源与重载。`DesktopServices` 可替换目录选择、编辑与记忆操作；`platform/desktop_actions.*` 实现 Windows 选择器和编辑动作，`platform/preferences.*` 管理偏好校验与原子保存。

`core/workspace_document.*` 只处理版本 2 容器与版本 1 导入，`platform/document_io.*` 负责文件读写。`app/working_document.*` 拥有管理历史及共享运行实例，不依赖 ImGui；结构历史共享 VM，不能回退几何编辑。`runtime/session_objects.cpp` 提供受保护的选择、属性和边界查询，不拼接 Lua 源码。

`Application` 拆为 `application.cpp`（来源/实例编排）、`application_documents.cpp`（加载/保存/确认/重试）、`application_menu.cpp`、`application_tree.cpp`、`application_inspector.cpp` 和 `application_frame.cpp`（布局/帧调度）。树命令在帧末执行，控件回调仅在调用期间借用。`DesktopServices` 的布局回调与目录回调共用本机偏好，互不覆盖。

公共头文件声明必要契约；`runtime/detail/` 是私有实现，其他模块不得包含。新接口必须说明坐标、所有权、失效与错误规则。各会话独立拥有数据和 Lua 状态；只有桌面入口持有窗口回调所需的图形资源。

## Lua 模块

用例只使用 `ctx`，入口 `sdk/drawer.lua` 负责组装：

| `sdk/drawer/` 文件 | 职责与边界 |
| --- | --- |
| `geometry.lua` | 纯计算，不读取宿主或上下文 |
| `bezier.lua` | 单段贝塞尔闭合/C1 镜像约束、切向补齐、接点手柄及约束切换；不读取界面或交互临时状态 |
| `bezier_ui.lua` | 贝塞尔/Gallery 共用的新曲线设置与选中操作；使用公开对象和事务接口 |
| `state.lua` | 数据复制、数组、编辑事务和清空 |
| `objects.lua` | 对象、绘图、命中与类型注册；不读取交互临时状态 |
| `selection.lua` | 收集标准/自定义图元绘制边界，生成外侧选择框，不重画主体或读取交互状态 |
| `interaction.lua` | 选择、控制点调整、整体平移和选择反馈 |
| `modes.lua` | 模式元数据、切换取消、动态反馈和通知；通过公开对象与绘制接口查询状态 |
| `drawing.lua` | 固定/自由落点、完成、取消和构造预览；通过公开对象 API 绘制 |
| `ui.lua` | 控件请求和参数编辑事务 |
| `inspector.lua` | 树对象选择、通用属性事务、对象边界与扩展回调；通过公开对象/手柄接口协作 |

`drawer.*` 为 SDK 保留模块名；用例的 `require('helper')` 从自身目录加载。`ctx.state` 属于文档，`ctx.temp` 属于临时交互；回滚、撤销与恢复可能替换状态表，不保留旧引用。

## 支持目录

- `cases/`：完整交互示例，每例以 `main.lua` 为入口；`templates/case/` 为新用例模板。

- `tests/`：按行为拆分的 Catch2 测试、`lua/` 可执行规格；`scenarios/` 为可重放场景，`support/artifacts.*` 由 Catch2 listener 管理夹具成功清理和失败保留；独立 CMake 配置。
- `benchmarks/`：独立 Google Benchmark 场景与说明，默认关闭，不复用测试内部 fixture。
- `tools/`：格式、边界、性能脚本和 `validate.ps1` 总入口；`validation/` 管理证据、场景捕获及检查器自检；`cmake/`：构建约束；`.github/workflows/`：复用本地入口的 CI。
- `docs/`：需求、决定、规范、接口和开发日志；`third_party/`：依赖策略，源码缓存在忽略的 `build/` 中。
- `build/`：构建树及缓存；`out/`：发布产物；`artifacts/validation/`：验证证据；`.tmp/validation/`：临时输入和失败夹具，均不进入 Git。

Gallery 内部分为 `main.lua`（控件与事件）、`catalog.lua`（种类和自定义行为）、`sample_scene.lua`（可选参考场景），默认空白并由用户交互创建。原 `triangle/` 目录保留，显示名为多边形，维持旧文档标识。

增加模块或依赖时同步更新 CMake、`tools/check-modules.ps1` 及本文。按职责拆分，避免为了行数机械切文件，也避免所有实现堆入公共工具类、全局服务或单个入口文件。
