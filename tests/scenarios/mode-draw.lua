ctx:edit('配置绘制反馈场景', function()
  ctx.state.params.vertices = 5
end)
ctx:set_mode('draw')
ctx:draw_event('polygon', 5, { type = 'down', button = 1, x = -100, y = 0 })
ctx:draw_event('polygon', 5, { type = 'down', button = 1, x = 0, y = 80 })
