ctx:edit('验证文本字号', function()
  ctx.state.params.kind, ctx.state.params.font_size = 'text', 24
  ctx:add('text', { { x = -260, y = 170 } }, {
    text = '12px · 小字号',
    font_size = 12,
    color = { 0.65, 0.8, 1, 1 },
  })
  ctx:add('text', { { x = -260, y = 70 } }, {
    text = '24px · 二维图形 SDK',
    font_size = 24,
    color = { 0.5, 0.9, 0.7, 1 },
  })
  ctx:add('text', { { x = -260, y = -70 } }, {
    text = '48px · 大字号',
    font_size = 48,
    color = { 1, 0.65, 0.3, 1 },
  })
end)
ctx:select_object(3, 'edit')
