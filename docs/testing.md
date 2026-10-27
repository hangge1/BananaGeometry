# 测试与验收记录

日期：2026-10-27。区分正确性、窗口集成、性能测量及未完成的人工验收。

## 测试组织与覆盖

Catch2 **3.16.1** 提供独立用例、参数化、Section、异常及浮点断言，CTest 自动发现测试。默认共 **37 项**：36 个单元/运行时行为用例及一个实际窗口检查；启用 Google Benchmark **1.10.0** 后增加性能入口检查，共 **38 项**。没有百分比覆盖门槛。

| 文件 / 标签 | 行为覆盖 |
| --- | --- |
| `tests/value_test.cpp` / `[json]` | 数字精度、Unicode、空容器、非法 UTF-8、重复键、语法、深度与非有限值 |
| `tests/history_test.cpp` / `[history]` | 保存点、空编辑、撤销重做、分支截断、容量限制 |
| `tests/view_test.cpp` / `[view]` | 数学坐标往返、缩放锚点、平移、线段距离 |
| `tests/document_test.cpp` / `[document]` | 中文路径、可编辑数据往返、视图恢复、版本及结构拒绝、失败写入保护、自定义对象 |
| `tests/session_test.cpp` / `[lua]` | 连续绘制、实例隔离、拖拽合并、取消、回滚、指令预算、控件事务、示例、模块和失败重载 |
| `tests/lua/` | 通过真实嵌入式运行时执行几何及事务规格，失败保留脚本文件名 |
| `window_smoke` / `integration` | Win32 窗口、D3D11 或 WARP、实际用例与绘图；运行 12 帧并正常退出 |
| `benchmark_smoke` / `performance` | 可选；确认性能场景实际运行并输出测量，无固定耗时门槛 |

测试按模块维护，临时文件由独立 fixture 管理，支持并行。参数化和 Section 可在一个独立测试内覆盖多组输入，数量不等同于断言总数。

## 日常命令

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-format.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-modules.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Configuration Debug
ctest --test-dir build/windows -C Debug -L unit -j 4 --output-on-failure
& ./build/windows/bin/Debug/drawer_tests.exe '[lua]' --reporter console
```

一键脚本依次运行模块检查、配置、构建和 CTest，失败即停止；JUnit 为 `build/windows/test-results-Debug.xml`。也可直接给 CTest 传 `--output-junit test-results-Debug.xml`。`-Package` 在成功后安装到 `out/Drawer`。格式工具版本及安装见[代码规范](coding-style.md)。项目 C++ 启用 `/W4 /WX`，第三方代码独立构建。

## 性能测量

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Configuration Release -Benchmarks
```

对文档编码、解码、历史提交、Lua 绘图提取、未命中扫描各测 100/1000 个对象，每组重复三次。准备和计时边界见 [benchmarks/README.md](../benchmarks/README.md)。使用实际经过时间；JSON 包含机器信息、原始数据和聚合统计，保存到 `build/windows/benchmark-Release.json`。脚本检查空报告和 `SkipWithError`，普通 CI 不使用机器相关时间阈值。

本机一次 Release 测量中，1000 个对象的中位时间约为：编码 12.2 ms、解码 13.8 ms、历史提交 8.38 ms、Lua 绘图提取 4.03 ms、未命中扫描 3.58 ms。该样本来自 16 逻辑处理器环境、每组至少 0.1 秒计时；用于复现和比较，不代表 GPU 帧率或性能承诺。

## 已执行的本地检查

- 格式及模块边界检查通过；故意注入反向 include 和反向 CMake 依赖时均被拒绝，源文件已恢复。
- 重构后的 Debug 一键构建及 **37/37** CTest 通过；Release 构建及 **38/38** CTest 通过。
- Google Benchmark 10 组输入各重复三次，报告无错误；单独验证性能入口确实输出测量。
- 单独关闭 `BUILD_TESTING`、`DRAWER_BUILD_BENCHMARKS`，复用离线运行依赖缓存完成生产构建；配置中无测试或性能框架目标。
- 首版已执行依赖新缓存下载、哈希验证、离线配置和发布目录独立启动。截图确认中文和各类图元；历史结果与提交验证见[开发日志](development-log.md)和[增量版本](version-history.md)。

GitHub Actions 已配置格式、边界、构建、测试、测量及报告归档；尚未推送执行，未取得远程 CI 结果。

## 尚需人工验收

- [ ] 系统打开/保存对话框的取消、覆盖和权限错误。
- [ ] 实际鼠标键盘完成三角形及曲线的创建、选择、拖动和快捷键。
- [ ] 关闭、重载、退出的保存/放弃/取消流程，多标签逐项保存。
- [ ] 跨显示器、不同 DPI、长时间交互及实际用户算法的资源占用。

这些行为保留为人工验收项，不能由单元测试或静态截图替代。
