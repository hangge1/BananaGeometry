# 暂存文件树的独立验证

`tools/validation/check-index.ps1` 将当前 Git 暂存区导出为源码快照，执行格式、文档、模块正反检查及构建/CTest。未暂存的应用源码不参与验证，脚本不提交、不推送，也不修改暂存内容。

```powershell
tools/validation/check-index.ps1 -Step core
tools/validation/check-index.ps1 -Step ui -RunId <同一运行编号> -DependencyCache build/windows
```

`-Configuration` 支持 Debug/Release；文档步骤可用 `-SkipBuild`，报告明确记录未构建。格式器可通过 `-ClangFormat`、`-StyLua` 指定，版本沿用工程固定要求。

首次默认按项目固定 URL/哈希获取依赖。`-DependencyCache` 可显式复用某个已有 CMake 构建树的 `FETCHCONTENT_SOURCE_DIR_*` 路径；调用方须确认依赖版本相同，路径和命令进入报告，不借用该构建树的应用源码。

同一运行编号使用稳定的隔离源码路径及 `build/index-review/<运行编号>/` 构建目录，便于增量验证。下一步导出前，将上一步源码及失败夹具移至同一临时目录内留存，检查路径后移动，不递归删除旧证据。

- 暂存树标识、命令、退出码与 JUnit：`artifacts/validation/<运行编号>/`。
- 导出 ZIP、当前源码、历史输入及失败夹具：`.tmp/validation/<运行编号>/`。
- 验证过程中暂存树发生变化会失败，必须重新验证。

分步补录已验收成果时，每步验证通过后再提交；最终文件树应与验收内容一致。完整验证约定见[验证工具与产物](validation.md)。
