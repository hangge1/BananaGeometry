# 测试与验收记录

日期：2026-10-27。区分正确性、窗口集成、性能测量及未完成的人工验收。

## 测试组织与覆盖

Catch2 **3.16.1** 提供独立用例、参数化、Section、异常及浮点断言，CTest 自动发现测试。默认共 **76 项**：75 个单元/运行时/画布输入/场景及夹具行为用例和一个实际窗口检查；启用 Google Benchmark **1.10.0** 后增加性能入口检查，共 **77 项**。没有百分比覆盖门槛。

| 文件 / 标签 | 行为覆盖 |
| --- | --- |
| `tests/value_test.cpp` / `[json]` | 数字精度、Unicode、空容器、非法 UTF-8、重复键、语法、深度与非有限值 |
| `tests/history_test.cpp` / `[history]` | 保存点、空编辑、撤销重做、分支截断、容量限制 |
| `tests/view_test.cpp` / `[view]` | 数学坐标往返、缩放锚点、平移、线段距离 |
| `tests/document_test.cpp` / `[document]` | 中文路径、可编辑数据往返、视图恢复、版本及结构拒绝、失败写入保护、自定义对象 |
| `tests/session_test.cpp` / `[lua]` | 连续绘制、实例隔离、拖拽合并、取消、回滚、指令预算、控件事务、示例、模块和失败重载 |
| `tests/interaction_test.cpp` / `[interaction]`、`[text]` | 标准/自定义对象高亮、控制点、样式隔离、文本范围、缩放、抓取偏移、空点击、取消、撤销重做、前后顺序和无效测量 |
| `tests/canvas_view_test.cpp` / `[app]`、`[render]` | 实际 ImGui 字体和鼠标事件链；多行中文、字面 `##`、缩放与字体比例；主体平移、画布外捕获、光标和方向键 |
| `tests/translation_test.cpp` / `[translation]` | 全图元平移、形状及样式保持、Alt/Shift、抖动阈值、撤销重做、Esc、键盘与自定义坐标回调 |
| `tests/bezier_test.cpp` / `[bezier]` | 多阶求值、细分误差、回折、输入限制、保存与拾取 |
| `tests/drawing_test.cpp` / `[drawing]` | 自由完成、点数上限、预览取消、工具切换、配置事务、加载默认值 |
| `tests/case_configuration_test.cpp` / `[cases]` | 多边形数量、自由模式、可变阶数曲线与旧配置兼容 |
| `tests/gallery_tools_test.cpp` / `[gallery]` | 11 种图元实际选择并落点、设置、切换、文本参数编辑、保存加载与参考场景撤销 |
| `tests/scenario_test.cpp` / `[scenarios]` | 六个维护场景的真实用例重放、夹具错误定位及大小限制 |
| `tests/fixture_probe_test.cpp` / `[artifacts]` | 临时夹具生命周期；维护脚本分别验证成功清理和故障保留 |
| `tests/lua/` | 通过真实嵌入式运行时执行几何及事务规格，失败保留脚本文件名 |
| `window_smoke` / `integration` | Win32 窗口、D3D11 或 WARP、实际用例与绘图；运行 12 帧并正常退出 |
| `benchmark_smoke` / `performance` | 可选；确认性能场景实际运行并输出测量，无固定耗时门槛 |

测试按模块维护，临时文件由独立 fixture 管理，支持并行；成功清理，失败保留并输出目录。参数化和 Section 可在一个独立测试内覆盖多组输入，数量不等同于断言总数。

## 日常命令

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-format.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-modules.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate.ps1 -Configuration Release -Benchmarks -Screenshots -Package
ctest --test-dir build/windows -C Debug -L unit -j 4 --output-on-failure
& ./build/windows/bin/Debug/drawer_tests.exe '[lua]' --reporter console
```

`build.ps1` 运行模块检查、配置、构建和 CTest；`tools/validate.ps1` 还组织格式、文档、检查器故障路径、夹具保留及可选截图。失败即停止，JUnit 为 `artifacts/validation/<运行编号>/junit-debug.xml`，日志与源码标识同目录保存；临时输入在 `.tmp/validation/<运行编号>/`。直接调用 CTest 时也将 `--output-junit` 指向证据目录。`-Package` 成功后安装到 `out/Drawer`。完整约定见[验证工具](validation.md)，格式工具见[代码规范](coding-style.md)。项目 C++ 启用 `/W4 /WX`，第三方代码独立构建。

## 性能测量

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate.ps1 -Configuration Release -Benchmarks
```

对文档编码、解码、历史提交、Lua 绘图提取、未命中扫描各测 100/1000 个对象，每组重复三次。准备和计时边界见 [benchmarks/README.md](../benchmarks/README.md)。使用实际经过时间；JSON 包含机器信息、原始数据和聚合统计，保存到 `artifacts/validation/<运行编号>/benchmark-release.json`。脚本检查空报告和 `SkipWithError`，普通 CI 不使用机器相关时间阈值。

此前一次 Release 测量中，1000 个对象的中位时间约为：编码 12.2 ms、解码 13.8 ms、历史提交 8.38 ms、Lua 绘图提取 4.03 ms、未命中扫描 3.58 ms。该历史样本来自 16 逻辑处理器环境、每组至少 0.1 秒计时；用于复现和比较，不代表 GPU 帧率或性能承诺。最新运行结果以证据目录中的 JSON 为准。

## 已执行的本地检查

- 格式及模块边界检查通过；反向 include 和反向 CMake 依赖在独立临时工程中均被拒绝，不再修改项目源文件。
- 当前 Release **77/77** CTest 通过，含窗口和性能入口；一键流程完成测量、安装、原生失败诊断、六个截图及夹具清理/保留检查。
- Debug **76/76** CTest 通过（不启用性能入口），完整格式、文档、边界及夹具生命周期检查通过。
- 无 `build/`、路径含空格的源码副本通过文档及边界检查；未将此静态/脚本检查表述为全新缓存编译。
- 画布输入测试使用真实 ImGui 字体及事件，字体比例变化模拟字体尺寸差异；不等同于物理跨屏 DPI 验收。
- 截图由 `tests/scenarios/manifest.json` 和 Lua 夹具重放，保存在 `artifacts/validation/<运行编号>/screenshots/`；涵盖五边形、五阶曲线、Gallery 空白/参考场景、文本选择及整体平移。发布用例没有预设选择。
- 旧散落证据已归档，50 个条目可由归档轮次 `legacy-map.json` 追溯。用户已完成人工审核并授权本轮完成后提交；交互复验步骤保留在[验收指南](manual-acceptance.md)。
- Google Benchmark 10 组输入各重复三次，报告无错误；单独验证性能入口确实输出测量。
- 单独关闭 `BUILD_TESTING`、`DRAWER_BUILD_BENCHMARKS`，复用离线运行依赖缓存完成生产构建；配置中无测试或性能框架目标。
- 当前生产安装目录独立执行 `--smoke-test` 成功；只包含应用可执行文件和运行所需脚本、文档、许可证，不安装测试或性能程序。
- 首版已执行依赖新缓存下载、哈希验证、离线配置和发布目录独立启动。截图确认中文和各类图元；历史结果与提交验证见[开发日志](development-log.md)和[增量版本](version-history.md)。

GitHub Actions 已配置格式、边界、构建、测试、测量及报告归档；尚未推送执行，未取得远程 CI 结果。

## 尚需人工验收

- [ ] 系统打开/保存对话框的取消、覆盖和权限错误。
- [x] 用户已确认当前版本人工审核，并授权提交；本地自动化结果按上述范围记录。
- [ ] 关闭、重载、退出的保存/放弃/取消流程，多标签逐项保存。
- [ ] 跨显示器、不同 DPI、长时间交互及实际用户算法的资源占用。

其余环境专项仍保留为人工验收项，不能由单元测试或静态截图替代；用户本轮审核不推定覆盖所有设备与权限环境。
