return {
  id = 'drawer.triangle',
  name = '三角形 / Triangle',
  data_version = 1,
  default_mode = 'draw',
  on_init = function(ctx)
    ctx.state.params = { width = 2, fill = true, label = '三角形', palette = 1 }
  end,
  on_ui = function(ctx)
    ctx.ui.text('三角形交互演示')
    if ctx.ui.button('绘制模式') then
      ctx:set_mode('draw')
    end
    ctx.ui.same_line()
    if ctx.ui.button('调整模式') then
      ctx:set_mode('edit')
    end
    ctx.ui.separator()
    ctx.ui.text(
      ctx.mode == 'draw' and '依次点击三个点，完成后继续绘制。'
        or '点击图形或顶点；拖动控制点，Esc 取消。'
    )
    ctx:parameter('线宽', 'width', 'slider', 1, 8)
    ctx:parameter('填充', 'fill', 'checkbox')
    ctx:parameter('标签', 'label', 'input')
    ctx:parameter('配色', 'palette', 'combo', { '蓝色', '绿色', '橙色' })
    ctx.ui.text('对象数量：' .. #ctx.state.objects)
  end,
  on_event = function(ctx, event)
    if ctx.mode == 'edit' then
      ctx:edit_event(event)
    else
      local colors = { { 0.35, 0.65, 1, 1 }, { 0.3, 0.85, 0.65, 1 }, { 1, 0.65, 0.3, 1 } }
      ctx:draw_event('triangle', 3, event, {
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
