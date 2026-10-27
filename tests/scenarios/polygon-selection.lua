ctx:edit('验证五边形', function()
  ctx.state.params.vertices = 5
  ctx:add('polygon', {
    { x = -160, y = -70 },
    { x = -170, y = 70 },
    { x = -30, y = 150 },
    { x = 110, y = 60 },
    { x = 70, y = -100 },
  }, { fill = true, color = { 0.35, 0.65, 1, 1 }, width = 2 })
end)
ctx:set_mode('edit')
ctx.temp.selected = ctx.state.objects[1].id
