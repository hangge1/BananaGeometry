local catalog = require('catalog')
local samples = require('sample_scene')
local defaults = {
  kind = 'line',
  vertices = 5,
  degree = 3,
  free = false,
  width = 2,
  fill = false,
  palette = 1,
  radius = 5,
  text = '二维图形 SDK',
  size = 25,
}
local function initialize(ctx)
  ctx:ensure_parameters(defaults)
  catalog.register(ctx)
end

return {
  id = 'drawer.gallery',
  name = '图元与自定义对象 / Gallery',
  data_version = 1,
  default_mode = 'draw',
  on_init = initialize,
  on_load = initialize,
  on_ui = function(ctx)
    ctx:mode_buttons()
    local item, index = catalog.current(ctx.state.params.kind)
    local selected, changed = ctx.ui.combo('图元', index, catalog.labels)
    if changed then
      ctx:finish_parameter()
      ctx:edit('切换绘制图元', function()
        ctx.state.params.kind = catalog.items[selected].kind
        if ctx.state.params.kind == 'polygon' then
          ctx.state.params.vertices = math.max(3, ctx.state.params.vertices)
        end
      end)
      ctx:set_mode('draw')
      item = catalog.current(ctx.state.params.kind)
    end
    if item.kind == 'polygon' or item.kind == 'polyline' then
      ctx:creation_parameter('自由点数', 'free', 'checkbox')
      if not ctx.state.params.free then
        ctx:creation_parameter(
          '点数',
          'vertices',
          'integer',
          item.kind == 'polygon' and 3 or 2,
          256
        )
      end
    elseif item.kind == 'bezier' then
      ctx:creation_parameter('阶数', 'degree', 'integer', 1, 32)
    elseif item.kind == 'text' then
      ctx:creation_parameter('文字', 'text', 'input')
    elseif item.kind == 'point' then
      ctx:creation_parameter('点半径', 'radius', 'slider', 2, 20)
    elseif item.kind == 'cross' then
      ctx:creation_parameter('十字大小', 'size', 'integer', 1, 200)
    end
    ctx:creation_parameter('线宽', 'width', 'slider', 1, 8)
    ctx:creation_parameter('配色', 'palette', 'combo', { '蓝色', '紫色', '绿色', '橙色' })
    if
      item.kind == 'polygon'
      or item.kind == 'triangle'
      or item.kind == 'rect'
      or item.kind == 'circle'
      or item.kind == 'ellipse'
    then
      ctx:creation_parameter('填充', 'fill', 'checkbox')
    end
    ctx.ui.separator()
    if ctx.mode == 'draw' then
      ctx.ui.text(item.hint)
      if catalog.count(item, ctx.state.params) == 0 then
        ctx.ui.text('Enter / 右键完成，Esc 取消。')
        if ctx.ui.button('完成绘制') then
          ctx:finish_drawing()
        end
      end
      ctx.ui.text(
        string.format(
          '已落点：%d / %s',
          ctx:drawing_count(),
          catalog.count(item, ctx.state.params) == 0 and '自由'
            or tostring(catalog.count(item, ctx.state.params))
        )
      )
    else
      ctx.ui.text('拖图形/文字整体移动，拖控制点调整；Alt 强制平移。')
    end
    if ctx.ui.button('插入演示图元') then
      ctx:finish_parameter()
      ctx:edit('插入演示图元', function()
        samples.populate(ctx)
      end)
      ctx:set_mode('edit')
    end
    ctx.ui.text('对象数量：' .. #ctx.state.objects)
  end,
  on_event = function(ctx, event)
    if ctx.mode == 'edit' then
      ctx:edit_event(event)
    else
      local item = catalog.current(ctx.state.params.kind)
      if item.kind == 'text' and ctx.state.params.text == '' then
        return
      end
      ctx:draw_event(
        item.kind,
        catalog.count(item, ctx.state.params),
        event,
        catalog.style(item, ctx.state.params)
      )
    end
  end,
  on_draw = function(ctx)
    ctx:render()
  end,
}
