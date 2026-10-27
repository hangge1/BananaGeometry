ctx:edit('验证整体平移', function()
  ctx.state.params.vertices = 5
  local points = {
    { x = -160, y = -70 },
    { x = -170, y = 70 },
    { x = -30, y = 150 },
    { x = 110, y = 60 },
    { x = 70, y = -100 },
  }
  ctx:add('polygon', points, { color = { 0.4, 0.5, 0.6, 1 }, width = 1 })
  ctx:add('polygon', points, { fill = true, color = { 0.35, 0.65, 1, 1 }, width = 2 })
end)
ctx:set_mode('edit')
ctx:edit_event({ type = 'down', button = 1, x = 0, y = 0 })
ctx:edit_event({ type = 'move', x = 100, y = -50 })
ctx:edit_event({ type = 'up', button = 1, x = 100, y = -50 })
