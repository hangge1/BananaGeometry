ctx:edit('验证五阶曲线', function()
  ctx.state.params.degree = 5
  ctx:add('bezier', {
    { x = -240, y = -80 },
    { x = -200, y = 180 },
    { x = -80, y = 200 },
    { x = 10, y = -180 },
    { x = 160, y = -160 },
    { x = 220, y = 100 },
  }, { color = { 0.65, 0.5, 1, 1 }, width = 3 })
end)
ctx:set_mode('edit')
ctx.temp.selected = ctx.state.objects[1].id
