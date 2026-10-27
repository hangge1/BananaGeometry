ctx:edit('验证文本选择', function()
  require('sample_scene').populate(ctx)
end)
ctx:set_mode('edit')
ctx.temp.selected = ctx.state.objects[6].id
