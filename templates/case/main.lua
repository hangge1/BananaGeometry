-- Copy this folder under cases/, give it a unique id, then refresh the case list.
return {
  id = 'your.unique.id',
  name = '我的用例',
  data_version = 1,
  default_mode = 'draw',
  edit_mode = 'edit',
  on_init = function(ctx)
    ctx.state.params = { width = 2 }
  end,
  on_ui = function(ctx)
    if ctx.ui.button('绘制') then
      ctx:set_mode('draw')
    end
    ctx.ui.same_line()
    if ctx.ui.button('调整') then
      ctx:set_mode('edit')
    end
    ctx:parameter('线宽', 'width', 'slider', 1, 8)
  end,
  on_event = function(ctx, event)
    if ctx.mode == 'edit' then
      ctx:edit_event(event)
    else
      ctx:draw_event('line', 2, event, { width = ctx.state.params.width })
    end
  end,
  on_draw = function(ctx)
    ctx:render()
  end,
}
