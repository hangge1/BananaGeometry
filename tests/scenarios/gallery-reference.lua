ctx:edit('验证参考场景', function()
  require('sample_scene').populate(ctx)
end)
ctx:set_mode('edit')
ctx.temp.selected = ctx.state.objects[3].id
