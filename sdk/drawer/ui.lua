-- Adapts controls and groups parameter edits through the transaction API.
local M = {}

function M.attach(ctx, host)
  ctx.ui = {}
  for _, operation in ipairs({
    'button',
    'text',
    'separator',
    'same_line',
    'slider',
    'number',
    'checkbox',
    'input',
    'combo',
  }) do
    ctx.ui[operation] = function(...)
      return host.ui(operation, ...)
    end
  end
  --- Groups a complete widget edit into one transaction, including slider drags.
  ---@param label string
  ---@param key string Existing key in state.params.
  ---@param kind string Widget operation such as slider, input or checkbox.
  function ctx:parameter(label, key, kind, ...)
    local value, changed, finished = self.ui[kind](label, self.state.params[key], ...)
    if changed then
      if not self.temp.parameter then
        self:begin_edit('修改参数：' .. label)
        self.temp.parameter = key
      end
      self.state.params[key] = value
    end
    if finished and self.temp.parameter == key then
      self:commit_edit()
      self.temp.parameter = nil
    end
    return self.state.params[key]
  end
end

return M
