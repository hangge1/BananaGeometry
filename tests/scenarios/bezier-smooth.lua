ctx:edit('验证平滑接点', function()
  ctx.state.params.degree, ctx.state.params.closed, ctx.state.params.smooth = 4, true, true
  local curve = ctx:add('bezier', {
    { x = 0, y = -100 },
    { x = 180, y = -100 },
    { x = 0, y = 200 },
    { x = -120, y = -100 },
    { x = 0, y = -100 },
  }, { closed = true, width = 3, color = { 0.65, 0.5, 1, 1 } })
  ctx:set_bezier_smooth(curve, true)
end)
ctx:set_mode('edit')
ctx.temp.selected = ctx.state.objects[1].id
