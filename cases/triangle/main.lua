local defaults =
  { vertices = 3, free = false, width = 2, fill = true, label = '多边形', palette = 1 }
local colors = { { 0.35, 0.65, 1, 1 }, { 0.3, 0.85, 0.65, 1 }, { 1, 0.65, 0.3, 1 } }
local function initialize(ctx)
  ctx:ensure_parameters(defaults)
end

return {
  -- Preserve the original ID so saved triangle documents continue to load.
  id = 'drawer.triangle',
  name = '多边形 / Polygon',
  data_version = 1,
  default_mode = 'draw',
  edit_mode = 'edit',
  on_init = initialize,
  on_load = initialize,
  on_ui = function(ctx)
    ctx:mode_buttons()
    ctx.ui.separator()
    ctx.ui.text('新多边形设置')
    ctx:creation_parameter('自由顶点数', 'free', 'checkbox')
    if not ctx.state.params.free then
      ctx:creation_parameter('顶点数', 'vertices', 'integer', 3, 256)
    end
    ctx:creation_parameter('线宽', 'width', 'slider', 1, 8)
    ctx:creation_parameter('填充', 'fill', 'checkbox')
    ctx:creation_parameter('配色', 'palette', 'combo', { '蓝色', '绿色', '橙色' })
    if ctx.mode == 'draw' then
      if ctx.state.params.free then
        ctx.ui.text('依次落点, Enter / 右键完成, Esc 取消。')
        if ctx:drawing_count() >= 3 and ctx.ui.button('完成多边形') then
          ctx:finish_drawing()
        end
      else
        ctx.ui.text(
          string.format(
            '落下 %d 个顶点自动完成，可连续绘制。',
            ctx.state.params.vertices
          )
        )
      end
      ctx.ui.text('已落点：' .. ctx:drawing_count())
    else
      ctx.ui.text('拖动图形整体平移；拖动顶点调整。')
      ctx.ui.text('Alt 强制平移，Shift 锁定方向；Esc 取消。')
    end
    local selected = ctx:object(ctx.temp.selected)
    if selected then
      ctx.ui.text('选中对象顶点：' .. #selected.points)
    end
    ctx.ui.text('对象数量：' .. #ctx.state.objects)
  end,
  on_event = function(ctx, event)
    if ctx.mode == 'edit' then
      ctx:edit_event(event)
    else
      ctx:draw_event('polygon', ctx.state.params.free and 0 or ctx.state.params.vertices, event, {
        color = colors[ctx.state.params.palette],
        width = ctx.state.params.width,
        fill = ctx.state.params.fill,
      })
    end
  end,
  on_draw = function(ctx)
    ctx:render()
  end,
}
