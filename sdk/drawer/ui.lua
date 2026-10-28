-- Adapts controls and groups parameter edits through the transaction API.
local M = {}

function M.attach(ctx, host)
  function ctx:finish_parameter()
    if self.temp.parameter then
      self:commit_edit()
      self.temp.parameter = nil
    end
  end
  ctx.ui = {}
  for _, operation in ipairs({
    'button',
    'mode_button',
    'text',
    'separator',
    'same_line',
    'slider',
    'number',
    'integer',
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
      if self.temp.parameter and self.temp.parameter ~= key then
        self:finish_parameter()
      end
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
  function ctx:creation_parameter(label, key, kind, ...)
    local before = self.state.params[key]
    local value = self:parameter(label, key, kind, ...)
    if value ~= before then
      self:reset_drawing()
    end
    return value
  end
  function ctx:mode_buttons()
    if self.ui.mode_button('绘制', self.mode == 'draw') and self.mode ~= 'draw' then
      self:finish_parameter()
      self:set_mode('draw')
    end
    self.ui.same_line()
    if self.ui.mode_button('调整', self.mode == 'edit') and self.mode ~= 'edit' then
      self:finish_parameter()
      self:set_mode('edit')
    end
  end
end

return M
