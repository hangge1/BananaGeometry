ctx:edit('验证闭合曲线', function()
  ctx.state.params.degree, ctx.state.params.closed = 5, true
  ctx:add('bezier', {
    { x = -160, y = -140 },
    { x = -300, y = 80 },
    { x = -100, y = 120 },
    { x = 120, y = 100 },
    { x = 160, y = -80 },
    { x = -160, y = -140 },
  }, { color = { 0.65, 0.5, 1, 1 }, width = 3, closed = true })
end)
ctx:set_mode('edit')
ctx.temp.selected = ctx.state.objects[1].id
