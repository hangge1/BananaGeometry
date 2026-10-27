# 性能测量

此目录只放 Google Benchmark 测量，不链接 GUI、不替代正确性测试。默认不构建。

```powershell
cmake -S . -B build/windows -DDRAWER_BUILD_BENCHMARKS=ON
cmake --build build/windows --config Release --parallel
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-benchmarks.ps1
```

每组输入为 100、1000 个三角形，确定性构造且不含文件 I/O。测量按职责拆分：

- `DocumentEncode` / `DocumentDecode`：完整模型校验和 JSON 编解码，包含结果释放。
- `HistoryCommit`：准备和销毁在暂停计时区间，仅测一次真实修改的提交，包含历史大小序列化；不含调用方构造输入的复制。
- `LuaDrawExtraction`：预先恢复会话，测量 Lua 绘图、跨语言转换和批量结果提取。
- `LuaHitTestMiss`：调整模式下点击所有图形之外的位置，测完整控制点和图形扫描。

使用 `UseRealTime` 计时，避免 Windows 短区间 CPU 时钟精度影响主统计；每组重复三次，JSON 保存机器信息、原始数据和聚合统计。脚本检查空报告及 `SkipWithError`，不设置机器相关耗时阈值。比较性能时固定编译配置、输入、机器和负载；这些结果不代表 GPU 帧率。CTest 中的 `benchmark_smoke` 只检查测量入口实际执行。

报告统一保存到 `artifacts/validation/<运行编号>/benchmark-release.json`，同目录记录源码版本、命令及日志。全流程使用 `tools/validate.ps1 -Configuration Release -Benchmarks`；目录与复用约定见[验证规范](../docs/validation.md)。
