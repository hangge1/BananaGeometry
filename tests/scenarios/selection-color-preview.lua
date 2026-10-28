ctx:edit('验证改色预览', function()
  ctx:add('rect', { { x = -170, y = -80 }, { x = 170, y = 90 } }, {
    color = { 0.25, 0.6, 1, 1 },
    width = 12,
    fill = true,
  })
  ctx:add(
    'text',
    { { x = -170, y = 150 } },
    { text = '选中时直接查看真实颜色', color = { 0.8, 0.85, 0.95, 1 } }
  )
end)
ctx:select_object(1, 'edit')
ctx:begin_edit('修改颜色')
ctx.temp.parameter = 'object:1:color'
ctx.state.objects[1].color = { 0.95, 0.2, 0.3, 0.55 }
