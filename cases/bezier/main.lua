local defaults = { degree = 3, width = 3, samples = 24, palette = 1 }
local colors = { { 0.65, 0.5, 1, 1 }, { 0.3, 0.85, 0.65, 1 }, { 1, 0.65, 0.3, 1 } }
local function initialize(ctx)
  ctx:ensure_parameters(defaults)
end

return {
  id = 'drawer.bezier',
  name = '贝塞尔曲线 / Bezier',
  data_version = 1,
  default_mode = 'draw',
  on_init = initialize,
  on_load = initialize,
  on_ui = function(ctx)
    ctx:mode_buttons()
    ctx.ui.separator()
    ctx.ui.text('新曲线设置')
    ctx:creation_parameter('阶数', 'degree', 'integer', 1, 32)
    ctx:creation_parameter('线宽', 'width', 'slider', 1, 8)
    ctx:creation_parameter('配色', 'palette', 'combo', { '紫色', '绿色', '橙色' })
    ctx.ui.text(
      string.format(
        '%d 阶曲线需要 %d 个控制点。',
        ctx.state.params.degree,
        ctx.state.params.degree + 1
      )
    )
    if ctx.mode == 'draw' then
      ctx.ui.text('已落点：' .. ctx:drawing_count() .. '；Esc 取消。')
    else
      ctx.ui.text('选中后拖动控制点，显示控制多边形。')
    end
    ctx:parameter('采样点数', 'samples', 'integer', 2, 256)
    local selected = ctx:object(ctx.temp.selected)
    if selected and selected.kind == 'bezier' then
      ctx.ui.text('选中曲线阶数：' .. (#selected.points - 1))
    end
  end,
  on_event = function(ctx, event)
    if ctx.mode == 'edit' then
      ctx:edit_event(event)
    else
      ctx:draw_event('bezier', ctx.state.params.degree + 1, event, {
        color = colors[ctx.state.params.palette],
        width = ctx.state.params.width,
      })
    end
  end,
  on_draw = function(ctx)
    ctx:render()
    local selected = ctx:object(ctx.temp.selected)
    if selected and selected.kind == 'bezier' then
      local count = math.floor(ctx.state.params.samples)
      for i = 0, count do
        ctx:point(ctx.math.bezier(selected.points, i / count), { 0.3, 0.9, 0.8, 1 }, 2)
      end
    end
  end,
}
