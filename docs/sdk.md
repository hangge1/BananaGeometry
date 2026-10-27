# Lua SDK v1 使用说明

## 快速新增用例

1. 将 `templates/case/` 复制到 `cases/my_case/`。
2. 修改 `main.lua` 中的 `id` 为唯一且稳定的字符串，修改显示名称。
3. 点击程序的“刷新用例”，再点击该用例的“新建实例”。
4. 使用外部编辑器修改脚本，在对应标签按 F5 或点击“重载”。修改用例无需运行 CMake 或编译器。

构建目录和发布目录都有自己的 `cases/` 和 `sdk/`。运行哪个目录的程序，就编辑那个目录下的用例。源码仓库中的 `cases/` 在构建应用时复制到程序旁边；日常直接修改程序旁的脚本即可用 F5 生效。

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
    -- 可选：加载持久化数据后验证或恢复非持久化缓存。
    on_load = function(ctx) end,
}
```

必填字段为 `id`、`name`、正整数 `data_version`；回调可按需要省略。`on_init` 直接初始化数据和注册自定义类型，无需为初始数据建立历史。刷新用例列表会加载定义并执行初始化以校验用例，请勿在初始化中进行外部副作用。

文档通过 `id` 匹配插件，要求 `data_version` 相同，不自动迁移不兼容文档。修改显示名称不会改变文档归属。

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
| `bezier` | 四个三次贝塞尔控制点 |
| `text` | 一个锚点；文字由 `text` 字段指定 |

通用样式为 RGBA `color`（0–1）、屏幕像素 `width` 和布尔 `fill`。坐标统一 X 向右、Y 向上；首版文档约束为 ±1e12，极端缩放和超大几何并非高精度 CAD 渲染保证。

`ctx:render()` 批量绘制标准对象、自定义对象和选择控制点。也可调用 `ctx:emit(shape)`、`ctx:draw_batch(shapes)`、`ctx:line(a,b,color,width)`、`ctx:point(p,color,radius)` 和 `ctx:text(p,text,color)` 绘制临时辅助内容，不写入文档。

## 模式、输入与编辑辅助

`ctx:set_mode(name)` 取消未完成操作并清理临时状态。`ctx:draw_event(kind,count,event,style)` 收集指定数量的点击，完成后创建对象并继续下一次绘制；Esc 取消未完成点列。

`ctx:edit_event(event)` 提供图形选择、控制点拖动、Esc 回滚和 Delete 删除。默认处理左键，右键留给用例自定义。

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
ctx.ui.checkbox(label, value)
ctx.ui.input(label, value)
ctx.ui.combo(label, index, {"选项一", "选项二"}) -- 索引从 1 开始
```

持久化参数推荐使用 `ctx:parameter(label,key,kind,...)`，它绑定 `ctx.state.params[key]`，并将一次滑动或文本编辑组合为一个事务。参数须先在初始化中赋默认值。例如 `ctx:parameter("线宽","width","slider",1,8)`。

## 自定义对象与几何辅助

`ctx:register_type(name, definition)` 注册自定义对象，定义 `draw(ctx,object)`、`hit(ctx,object,point,tolerance)`、`handles(ctx,object)`、`move_handle(ctx,object,index,position)`。注册在初始化中执行，加载文档后仍使用该注册行为。完整示例见 `cases/gallery/main.lua`。

`ctx:hit(object,point,tolerance)` 支持标准对象与自定义命中；`ctx:handles(object)` 获取控制点。

`ctx.math` 提供 `distance`、`segment_distance`、`bezier`、`add`、`sub`、`scale` 和 `dot`。额外算法可用 Lua 实现；首版不加载原生算法模块。

## 错误与限制

回调错误显示在底部可滚动诊断区，并恢复未提交数据。每次回调设置约一千万条 Lua 指令的预算，避免普通死循环持续阻塞。该预算不约束 C 函数中的耗时，也不是完整的安全沙箱；用例是本地受信任代码。

F5 只重载当前实例。成功时数据与历史清空并恢复默认模式；失败保留原实例。当前实现不会自动刷新其他同名标签，也不会自动保存源代码或迁移脚本状态。
