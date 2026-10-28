ctx:edit('设置闭合绘制', function()
  ctx.state.params.closed = true
end)
local style = { closed = true, width = 3 }
ctx:draw_event('bezier', 4, { type = 'down', button = 1, x = -180, y = -100 }, style)
ctx:draw_event('bezier', 4, { type = 'down', button = 1, x = -180, y = 180 }, style)
ctx:draw_event('bezier', 4, { type = 'move', x = 160, y = 150 }, style)
