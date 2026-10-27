local before = ctx.state.params.width
ctx:edit('parameter', function()
  ctx.state.params.width = before + 1
end)
assert(ctx.state.params.width == before + 1)
local success = pcall(function()
  ctx:edit('nested', function()
    ctx:edit('inner', function()
      ctx.state.params.width = 999
    end)
  end)
end)
assert(not success)
assert(ctx.state.params.width == before + 1)
ctx:begin_edit('cancel')
ctx.state.params.width = 20
ctx:cancel_edit()
assert(ctx.state.params.width == before + 1)
