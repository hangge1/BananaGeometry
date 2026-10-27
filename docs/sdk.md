# Lua SDK v1 使用说明

## 快速新增用例

1. 将 `templates/case/` 复制到 `cases/my_case/`。
2. 修改 `main.lua` 中的 `id` 为唯一且稳定的字符串，修改显示名称。
3. 点击程序的“更新用例列表”，再点击该用例的“新建实例”。
4. 使用外部编辑器修改脚本，在对应标签按 F5 或点击“应用脚本修改 F5”。修改用例无需运行 CMake 或编译器。

程序从 exe 所在目录向上查找最近的源码仓库（包含 `.git` 文件或目录、`CMakeLists.txt`、`sdk/drawer.lua` 和 `cases/`），找到后直接读取仓库根目录的 SDK 和用例；没有源码仓库时读取 exe 旁的资源。因此仓库内运行 `build/` 或 `out/Drawer/` 的程序，都可以直接修改根目录的 `cases/`，无需复制或构建。

底部“当前脚本”显示实际路径，按钮可复制路径。可通过 `drawer.exe --workspace <目录>` 覆盖自动选择；该目录必须含 `sdk/drawer.lua` 和 `cases/`，无效路径报错，不自动回退。自动选择不依赖启动工作目录；显式相对路径按启动工作目录解释。

## 用例定义与生命周期

入口固定为用例目录中的 `main.lua`，返回一个表：

```lua
return {
    id = "example.my_case",
    name = "我的用例",
    data_version = 1,
    default_mode = "draw",
    on_init = function(ctx) end,
    on_ui = function(ctx) end,
    on_event = function(ctx, event) end,
    on_draw = function(ctx) ctx:render() end,
    -- 可选：在清空事务内处理额外数据；不要在此嵌套 ctx:edit。
    on_clear = function(ctx) ctx.state.results = nil end,
    -- 可选：加载后补齐兼容默认值、验证或恢复非持久化缓存。
    on_load = function(ctx) end,
}
```

必填字段为 `id`、`name`、正整数 `data_version`；回调可按需要省略。`on_init` 直接初始化数据和注册自定义类型，无需为初始数据建立历史。刷新用例列表会加载定义并执行初始化以校验用例，请勿在初始化中进行外部副作用。

文档通过 `id` 匹配插件，要求 `data_version` 相同，不自动迁移不兼容文档。修改显示名称不会改变文档归属。

`ctx:ensure_parameters(defaults)` 在初始化或 `on_load` 中补齐缺失参数，不覆盖已有值。加载回调完成后，宿主再次校验数据并建立干净历史；回调不能遗留未提交事务。旧三角形实例显示名改为 Polygon，但保留 `drawer.triangle` 标识。

`require("helper")` 加载同目录 `helper.lua`，`require("math.geometry")` 加载 `math/geometry.lua` 或 `math/geometry/init.lua`。模块缓存与可变全局变量按实例隔离。中文目录支持 UTF-8 文件名。

## 数据与编辑事务

- `ctx.state`：可保存且可撤销的数据。初始包含 `objects` 数组、`params` 对象和 `next_id`。
- `ctx.temp`：选择、拖拽、临时缓存与预览；不保存、不进入历史。
- `ctx.mode`：当前交互模式；不保存、不进入历史。
- `ctx.zoom`：世界单位到屏幕像素的缩放，只读使用。

持久化数据仅支持有限数字、布尔、UTF-8 字符串、连续数组和字符串键对象。不支持循环引用、混合键表、稀疏数组、函数、userdata 或 JSON null。空数组用 `ctx.array()` 创建，空对象使用 `{}`。

```lua
ctx:edit("修改实验参数", function()
    ctx.state.params.size = 30
    ctx.state.results = { passed = true, count = 3 }
end)
```

运行中修改持久化数据必须通过事务，直接改 Lua 表不会自动进入历史和保存快照。事务不可嵌套；失败或取消会恢复整个持久化状态，`ctx.state` 表可能被替换，因此不要跨回调缓存其中的可变表引用。

长交互可调用 `ctx:begin_edit(label)`、在多次事件中更新状态，最后 `ctx:commit_edit()`；取消调用 `ctx:cancel_edit()`。未提交的事务不能保存，宿主保存命令会先取消预览。

每个实例保留最多 100 步历史，并按序列化大小约 64 MiB 控制保留快照；这不是进程内存上限。一次拖动生成一步历史。保存点按数据内容比较，撤销回已保存内容会消除脏标记。

## 图形对象与低层绘图

在事务中创建持久化对象：

```lua
ctx:edit("创建三角形", function()
    ctx:add("triangle", {
        {x=0, y=0}, {x=100, y=0}, {x=50, y=80}
    }, {color={0.3,0.7,1,1}, width=2, fill=true})
end)
```

`ctx:add(kind, points, style)` 返回具有稳定数字 `id` 的对象。`ctx:object(id)` 按标识查找对象。

| kind | points 的含义 |
| --- | --- |
| `point` | 一个点；`radius` 为屏幕像素半径 |
| `line` / `polyline` | 两点线段 / 连续折线 |
| `triangle` / `polygon` | 三角形 / 简单多边形顶点；多边形填充不支持自交轮廓 |
| `rect` | 两个对角点 |
| `circle` | 圆心与圆周上一点 |
| `ellipse` | 中心与定义 X/Y 半径的点 |
| `bezier` | 2–33 个控制点，对应 1–32 阶；阶数由各对象的控制点数量决定 |
| `text` | 一个锚点；文字由 `text` 字段指定 |

通用样式为 RGBA `color`（0–1）、屏幕像素 `width` 和布尔 `fill`。坐标统一 X 向右、Y 向上；首版文档约束为 ±1e12，极端缩放和超大几何并非高精度 CAD 渲染保证。

`ctx:render()` 批量绘制标准对象、自定义对象和选择控制点。也可调用 `ctx:emit(shape)`、`ctx:draw_batch(shapes)`、`ctx:line(a,b,color,width)`、`ctx:point(p,color,radius)` 和 `ctx:text(p,text,color)` 绘制临时辅助内容，不写入文档。

文本以锚点为显示矩形的左上角，字体使用宿主当前字体，字号保持屏幕像素大小。`ctx:text_bounds(object)` 返回世界坐标的 `left/right/top/bottom`；中文、多行和 `##` 按实际文字测量，`top` 大于 `bottom`。空文字或没有字体测量器时返回 `nil`，拾取仍可使用锚点。

原生宿主在画布帧中注入字体测量器；独立运行 `Session` 的调用方可用 `SetTextMeasurer` 提供屏幕像素宽高，不让运行时依赖 ImGui。未配置时尺寸为零；无头测试应显式注入测量器后验证文字范围。

## 模式、输入与编辑辅助

`ctx:set_mode(name)` 取消未完成操作并清理临时状态。`ctx:draw_event(kind,count,event,style)` 收集固定数量的点击，完成后创建对象并继续绘制；`count=0` 为自由多边形/折线，Enter、右键或 `ctx:finish_drawing()` 完成，至少需要 3/2 个点。上限 256 点自动完成，贝塞尔固定点数为 2–33。

`ctx:drawing_count()` 返回已落点数量；`ctx:reset_drawing()` 清除未完成构造，保留其他临时状态。切换种类或点数也会清除未完成点列，Esc 取消。`ctx:render()` 显示临时图元预览及控制点；`ctx:draw_object(object)` 可复用标准或自定义对象的绘制，无需加入文档。

贝塞尔渲染与拾取使用同一自适应折线，目标误差 0.75 屏幕像素，最多 1024 段以限制极端缩放的开销；纯数学求值使用 De Casteljau 方法。达到细分上限时不承诺仍满足误差目标。

`ctx:edit_event(event)` 提供图形选择、控制点拖动、Esc 回滚和 Delete 删除。默认处理左键，右键留给用例自定义。

选中图形叠加金色高亮，显示控制点及编号；点击空白或切换模式清除反馈。高亮不更改持久化颜色或产生历史。普通对象拖动控制点，文本可以从整个文字矩形范围直接拖动；拖拽保留按下位置与控制点的偏移，避免跳位，一次拖动只提交一步。重叠对象按绘制顺序从前向后拾取，当前选中的控制点优先响应。

自定义对象的高亮复用 `draw` 回调及 `ctx:emit`、`ctx:draw_batch`、`ctx:line/point/text` 等公开助手。使用回调传入的上下文绘图；回调应只绘图，不修改文档或依赖上下文表身份。

调整模式拖动图形主体整体平移，拖控制点修改形状；封闭图形内部可拾取，公开 `ctx:hit()` 仍遵循原填充规则。`ctx:hit_body()` 提供编辑主体拾取。Alt 强制整体平移，Shift 锁定水平/垂直；拖动阈值为 3 屏幕像素。方向键每步移动 1 屏幕像素，Shift 为 10 像素。

`ctx:translate_object(object, delta, source?)` 在编辑事务内平移对象。默认移动全部 `points`，保持样式、半径、自定义参数及标识；可传原始快照 `source`，使连续预览始终从初始几何计算。`ctx:can_translate(object)` 判断是否支持。自定义非点数据实现 `translate(ctx, object, delta, source)`，只修改目标对象，不修改原始快照，例如：

```lua
translate = function(_, object, delta, source)
  object.origin = { x = source.origin.x + delta.x, y = source.origin.y + delta.y }
end
```

拖动开始、松开提交和 Esc 回滚由交互层统一处理。`Session::GetPointerHint()` 只读取瞬态反馈供宿主显示光标，不引入运行时对界面的依赖。

事件包含：

- `type`：`down`、`move`、`up`、`key` 或 `key_up`。
- `x`、`y`：数学世界坐标下的鼠标位置。
- `button`：鼠标事件的 1 左键、2 右键，其他事件为 0。
- `key`：小写键名，例如 `escape`、`delete`、`a`、`enter`。
- `ctrl`、`shift`、`alt`：修饰键状态。

中键和空格+左键保留给视图平移，滚轮保留给缩放；通用键盘事件在鼠标位于画布且非文本编辑时发送，Ctrl 快捷键由宿主处理。活动拖拽在鼠标移出画布后继续接收移动和松开。

## 控件

`ctx.ui.button(label)` 返回是否点击；`text`、`separator`、`same_line` 用于布局。

输入控件返回 `value, changed, finished`：

```lua
ctx.ui.slider(label, value, min, max)
ctx.ui.number(label, value, min, max)
ctx.ui.integer(label, value, min, max) -- 整数输入，例如阶数和点数
ctx.ui.checkbox(label, value)
ctx.ui.input(label, value)
ctx.ui.combo(label, index, {"选项一", "选项二"}) -- 索引从 1 开始
```

持久化参数推荐使用 `ctx:parameter(label,key,kind,...)`，它绑定 `ctx.state.params[key]`，并将一次滑动或文本编辑组合为一个事务。参数须先在初始化中赋默认值。例如 `ctx:parameter("线宽","width","slider",1,8)`。

创建设置使用 `ctx:creation_parameter(...)`，参数相同，但变化时取消未完成绘制。`ctx:mode_buttons()` 提供绘制/调整按钮；离散操作隐藏正在编辑的参数之前，可调用 `ctx:finish_parameter()` 提交该参数，避免事务跨控件混合。

Gallery 的 `catalog.lua` 声明稳定种类、名称、落点数量、样式和自定义注册，`sample_scene.lua` 提供可选参考场景。新增一种图元时，在目录中添加条目并注册对应自定义对象即可进入选择器；特有参数由该用例控件设置，不需要修改宿主。

## 自定义对象与几何辅助

`ctx:register_type(name, definition)` 注册自定义对象，定义 `draw(ctx,object)`、`hit(ctx,object,point,tolerance)`、`handles(ctx,object)`、`move_handle(ctx,object,index,position)`。注册在初始化中执行，加载文档后仍使用该注册行为。完整示例见 `cases/gallery/main.lua`。

`ctx:hit(object,point,tolerance)` 支持标准对象与自定义命中；`ctx:handles(object)` 获取控制点。

`ctx.math` 提供 `distance`、`segment_distance`、`bezier`、`add`、`sub`、`scale` 和 `dot`。额外算法可用 Lua 实现；首版不加载原生算法模块。

## 错误与限制

回调错误显示在底部可滚动诊断区，并恢复未提交数据。每次回调设置约一千万条 Lua 指令的预算，避免普通死循环持续阻塞。该预算不约束 C 函数中的耗时，也不是完整的安全沙箱；用例是本地受信任代码。

“应用脚本修改 F5”只重载当前实例，包括 `require` 的模块。成功时数据与历史清空并恢复默认模式，当前标签和列表名称同步更新；失败保留原实例。其他同名标签继续运行原脚本，不自动保存源代码或迁移状态。

“更新用例列表”只重新扫描用例，更新名称及新增/移除项，不替换已打开实例、不清空数据，也不应用其控件或绘图代码变化。新增用例后更新列表；修改已打开用例后使用 F5。两种按钮均有说明提示。
