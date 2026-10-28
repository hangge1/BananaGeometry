ctx:edit('设置平滑绘制', function()
  ctx.state.params.degree, ctx.state.params.closed, ctx.state.params.smooth = 4, true, true
end)
local style = { closed = true, smooth = true, width = 3 }
ctx:draw_event('bezier', 5, { type = 'down', button = 1, x = 0, y = -100 }, style)
ctx:draw_event('bezier', 5, { type = 'down', button = 1, x = 180, y = -100 }, style)
ctx:draw_event('bezier', 5, { type = 'move', x = 0, y = 200 }, style)
