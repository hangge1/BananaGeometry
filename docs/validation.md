# 自动化验证与产物规范

## 当前评估与处理

原来可复用的格式、模块和性能脚本已在 `tools/`，但文档链接检查、截图构造、检查器反向验证只留在忽略的 `build/` 中，无法随仓库拉取复现；截图依赖复制/改写用例，报告分散在构建顶层和各构建树。

现已完成这些调整：

| 能力 | 工程入口 |
| --- | --- |
| 全流程调度 | `tools/validate.ps1` |
| 运行编号、日志、源码标识、退出码 | `tools/validation/common.psm1` |
| UTF-8 与本地 Markdown 链接 | `tools/validation/check-docs.ps1` |
| 模块/目标边界的正反向检查 | `tools/validation/check-guards.ps1` |
| Native 场景截图与错误诊断 | `capture-screenshots.ps1`、`check-native-fixtures.ps1` |
| 成功清理/失败保留测试夹具 | Catch2 listener、`check-fixture-retention.ps1` |
| 源码副本与含空格路径复用 | `check-portability.ps1` |
| 旧散落文件的可审查归档 | `archive-legacy.ps1` |

后六项 PowerShell 文件均位于 `tools/validation/`。旧截图、日志及一次性迁移材料已按来源归档，50 个旧条目的映射保存在对应运行目录的 `legacy-map.json`。没有移动依赖缓存或当前构建树。

## 运行入口

CI 固定使用 `windows-2022`，与构建脚本的 `Visual Studio 17 2022` 生成器对应。GitHub 已将 `windows-latest` / `windows-2025` 迁移至 VS 2026，不能将该别名与固定 VS 2022 生成器混用；见[官方迁移说明](https://github.blog/changelog/2026-05-14-github-actions-upcoming-image-migrations/)。

`build.ps1` 在配置前运行 `tools/validation/check-toolchain.ps1`，检查 CMake 版本和 VS 2022 C++ 组件，并将版本及安装路径写入本轮 `toolchain.json`。工作流提供手动触发入口；checkout/artifact 使用 Node.js 24 版本，避免旧运行时弃用告警。

需要 Windows、PowerShell 5.1+、CMake 3.24+、Visual Studio 2022 C++ 工具和 Windows SDK。格式检查固定 clang-format 19.1.5 与 StyLua 2.6.0，安装方式见[代码规范](coding-style.md)。不需要额外 Python 来运行这些验证脚本。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate.ps1 -Configuration Release -Benchmarks -Screenshots -Package
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validation/check-portability.ps1
```

脚本自行定位仓库，可从其他工作目录通过绝对脚本路径执行。格式器不在 PATH 时，完整入口接受 `-ClangFormat`、`-StyLua`。`-SkipFormat` 和 `-SkipBuild` 只运行所选择的检查，任务 JSON 中保留实际命令；检查失败返回非零并停止后续工作。

构建入口 `build.ps1` 也使用相同产物布局。单独性能入口为 `tools/run-benchmarks.ps1`；单独截图入口可用 `-Scene polygon-selection` 选择一个场景，以及 `-Executable` 指定已构建的程序。

构建入口在编译后显式同步本次配置目录中的 SDK 和用例，记录 `stage-sdk` / `stage-cases` 命令；仅改 Lua 时也会更新，避免原生截图或窗口检查读取未重新链接时遗留的副本。闭合专项可用 `-Scene bezier-closed,bezier-closed-draw`。

平滑专项为 `-Scene bezier-smooth,bezier-smooth-draw`，分别覆盖选中后的接点/切向显示和四阶 2/3 落点预览；场景和低层/交互断言均进入仓库，图片生成不等同于人工验收通过。

`-Scene selection-color-preview` 保持选中矩形的颜色事务尚未结束，验证真实 RGBA 预览、外侧框与空心控制点；结合 `text-selection`、`bezier-smooth` 检查文字及曲线不会被金色重画。

原生 smoke 模式忽略本机偏好，默认验证 exe 旁示例。`capture-screenshots.ps1 -CaseDirectory cases` 可验证指定用例目录，SDK 仍来自宿主；`-Workspace` 保留兼容。所选目录会记录到命令证据中。测试使用替代桌面操作验证编辑路径，不自动启动真实编辑器或系统目录选择器。

## 路径与命名

```text
tools/validation/                         维护的验证代码
tests/scenarios/manifest.json             稳定场景编号、Lua/JSON 夹具
artifacts/validation/<UTC时间-随机后缀>/    运行证据，不进入 Git
  run.json / source-files.json            源码提交、脏状态、文件哈希、环境摘要
  task-*.json / logs/                     每步命令、退出码、时间及诊断
  junit-release.xml / benchmark-release.json
  screenshots/*.png
.tmp/validation/<同一运行编号>/           临时输入与失败测试夹具，不进入 Git
build/                                   CMake 构建树、依赖和工具缓存
```

运行编号示例：`20261027T092905782Z-f571af4c`。场景使用小写连字符，如 `object-translation`。直接调用 CTest 时，临时夹具归入 `.tmp/validation/standalone/ctest/`，目录名含测试名称、进程和唯一后缀；正式入口按整轮运行分组。

成功测试由 listener 清理自身夹具；失败测试保留目录并在输出中列出路径。成功截图保留 PNG、删除自身临时 BMP，失败输入及日志保留。脚本不递归清空通用 `build/`、文档或用户文件；归档先检查路径边界，记录每个移动状态。

## 场景复用与验证边界

manifest 场景可选 `pointer: {x: 0.8, y: 0.5}`，由截图入口映射为 `--smoke-pointer x y`，按客户区归一化坐标注入测试指针，取值 0–1。此选项仅用于 smoke 模式；宿主继续忽略真实输入，因此绘制十字的截图不依赖用户鼠标位置。

新增截图只需添加 Lua 场景及 manifest 条目，使用公开 SDK，不修改发布用例。宿主测试入口支持 `--smoke-test --case-id <id> --fixture <Lua> --screenshot <BMP> --error-log <日志>`；夹具受现有指令、大小限制和回滚约束。重放期间不接收现场鼠标键盘输入，避免操作干扰已构造场景。

多实例场景 `workspace-multi` 使用版本 2 JSON 固定文档；manifest 标记 `document: true`，截图入口改用 `--document <JSON>`，与 Lua `--fixture` 互斥。该场景包含多边形、贝塞尔及缺失插件实例，Catch2 同时验证可用实例与不可用数据保留。独立程序也支持 `--document`；不允许文档指定可执行脚本路径。新增场景仍放 `tests/scenarios/`，不能写一次性构造脚本到构建树。

主入口验证构建、测试、检查器故障路径和截图生成；图像记录为“已捕获”，视觉正确性由人工或 AI 复核。源码副本检查覆盖无 `build/` 目录及含空格路径的脚本定位；完整编译由主入口承担。CI 调用同一入口并上传整个运行证据目录，工作流失败时另行上传 `.tmp/validation/` 的输入和夹具。

旧归档脚本默认只生成计划，`-Apply` 才执行移动；同一批文件移走后再次运行应为空计划。历史资料目录可通过 `legacy-map.json` 追溯原路径。
