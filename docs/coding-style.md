# 代码与注释规范

状态：2026-10-27 起生效。本文是当前规范，替代早期贡献指南中四空格缩进和手写测试工具的约定；保留既有 `AGENTS.md` 文件，不生成替代文件。

## C++

以 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) 为基准，使用 C++17、两空格缩进、80 列与 Google 的访问标签缩进。通过仓库 `.clang-format` 执行格式检查，不依赖个人编辑器设置。

- 类型使用 `UpperCamelCase`，普通函数使用 `UpperCamelCase`；局部变量、参数和数据字段使用 `snake_case`，类成员增加尾部下划线，常量使用 `kUpperCamelCase`。
- 名称表达业务含义。几何公式可以使用约定俗成的局部坐标或插值参数；控制流程和资源管理使用完整名称。
- 头文件自包含，采用项目路径对应的 include guard；实现文件首先包含对应头文件，避免传递包含依赖。
- 优先 RAII、值语义、`unique_ptr` 和明确所有权。非拥有指针需要能从类型、名称或契约判断生命周期。
- 单参数构造函数默认 `explicit`。`Value` 的标量/容器转换是数据模型的有意便利接口，作为明确例外保留并在类注释说明。
- 一个函数处理一个明确职责，拆分复合条件和难以理解的嵌套表达式；同一行不堆积多个控制语句或多个有初始化的变量声明。
- 公共 API 明确坐标单位、状态失效、事务边界和错误行为。实现细节不泄露进用例操作界面。

项目例外必须明确：保留 `.cpp` 扩展名；Windows API 所需宽字符、回调与系统句柄遵循平台接口；`windows.h` 必须先于依赖它的系统头，因此格式化器保留 include 顺序，人工按组审查。项目使用 C++ 异常报告解析和文件错误，但异常必须在应用或 Lua 绑定边界被捕获，不得越过 C 回调接口。

## Lua

参考大型 Lua 工程 [Neovim 的风格说明](https://github.com/neovim/neovim/blob/master/runtime/doc/dev_style.txt)及其 [StyLua 配置](https://github.com/neovim/neovim/blob/master/.stylua.toml)，仅采用适合标准 Lua 5.4 的规则，不引入 Neovim 运行时或 Luau 语法。

- 使用两空格、100 列、LF、优先单引号；由 `.stylua.toml` 和固定版本 StyLua 统一格式。
- 模块局部变量使用 `local`，模块通过返回值导出；宿主注入的 `_host` 是 SDK 的唯一约定全局入口，用例不直接依赖它。
- 函数、变量、模块文件使用 `snake_case`；实例方法使用冒号调用。已有 `on_init` 等入口及稳定用例 ID 保持兼容。
- 优先小函数、提前返回和明确的状态变更。禁止将多条控制语句挤在一行，也不依赖非预期的全局变量。
- 仅在类型保证后使用 `and/or` 作为表达式简化；可能返回 `false` 或 `nil` 的分支使用显式条件，避免模拟三元表达式产生歧义。
- 几何、事务和对象注册接口使用 LuaLS 风格的类型与参数注解，帮助作者获得补全；不为每个局部变量添加重复注解。
- 持久化数据写入 `ctx.state` 并经过编辑事务；模式、选择和缓存写入临时状态。回滚后不保留旧状态表引用。

## 注释

代码首先通过准确命名、类型、常量、较短函数和清晰控制流表达意图。注释补充代码本身无法可靠表达的信息：

- 对外 API 的用途、前置条件、所有权、单位、错误和失效规则。
- Lua 长跳转与 C++ 析构、回调生命周期、原子文件替换等跨边界约束。
- 算法中非直观的数学假设、序列化限制以及性能优化依据。
- 必须兼容的平台行为或有意偏离通用规范的原因。

不写逐行翻译代码的注释，不用注释掩盖模糊命名，不保留失效的实现说明或注释掉的代码。公共类和复杂函数写契约，明显的访问器无需重复描述。TODO 必须包含可追踪问题或责任标识及完成条件。

## 模块与文件组织

按职责组织模块和目录，不按文件长度机械切割。基础数据与几何不依赖 Lua 或界面；运行时依赖数据和 Lua；渲染处理绘图；平台层处理窗口、设备和系统对话框；应用层组合这些能力。

- CMake 目标体现模块边界和单向依赖，禁止反向依赖或循环依赖。
- 模块公共头只声明必要契约；Lua 栈转换、绑定回调等实现细节留在内部头或实现文件。
- 一个文件处理一个内聚职责。解析、文件 I/O、交互历史、控件适配和界面编排分别组织。
- Lua SDK 拆分纯数学、状态事务、对象绘图、交互和控件模块，通过入口组合。模块间使用明确接口，不相互读取未约定的内部状态。
- 测试按模块和行为拆分，独立管理 fixture 与临时文件；性能测量单独放置。
- 共享功能通过小接口复用，不为每个类新增独立工程、全局服务或单例。重构保持用例接口和文档兼容。

## 检查与测试约定

格式检查仅覆盖本项目源码和用例，不格式化依赖缓存或生成文件。格式化只能证明排版一致；命名、契约和注释仍需差异审查。

单元测试采用 Catch2，性能测试采用 Google Benchmark，统一由 CMake 配置；两者属于开发依赖，不进入发布程序。CTest 保留为运行和报告入口。Lua 行为通过实际嵌入式引擎测试，并提供可独立维护的 Lua 测试脚本。

单元测试描述可观察行为，使用独立 fixture、参数化数据和异常断言，禁止依赖执行顺序或共享临时文件。性能测试独立于正确性测试，区分准备工作与计时区间，导出机器信息、重复测量和 JSON 结果，不以固定机器阈值作为普通 CI 的通过条件。

### 本地检查

固定工具版本为 clang-format **19.1.5**、StyLua **2.6.0**。可以使用 Visual Studio 附带的同版本 clang-format，或通过开发用 Python 环境执行 `python -m pip install clang-format==19.1.5`；这些工具不属于应用运行依赖。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/setup-stylua.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-format.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-modules.ps1
```

`check-format.ps1 -Fix` 执行格式修正并验证 Lua AST；不传 `-Fix` 时只检查。工具不在 PATH 时可传 `-ClangFormat <路径>` 或 `-StyLua <路径>`。StyLua 安装脚本从官方发行包获取并校验 SHA-256，缓存留在 `build/tools/`。

模块约束同时由 CMake 的目标依赖检查及源码 include/import 检查执行，具体职责见[模块与目录](modules.md)。GitHub Actions 会运行格式检查、模块检查、CTest，并保存 JUnit 和性能 JSON；远程执行状态见测试记录。
