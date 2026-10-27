-- Owns persistent data transactions; cancel can replace ctx.state.
local function copy_value(value, array_factory)
  if type(value) ~= 'table' then
    return value
  end
  local result = getmetatable(value) and array_factory() or {}
  for key, item in pairs(value) do
    result[key] = copy_value(item, array_factory)
  end
  return result
end

local M = {}

function M.attach(ctx, host)
  ctx.array = host.array
  ctx.copy = function(value)
    return copy_value(value, host.array)
  end
  function ctx:begin_edit(label)
    host.begin_edit(label)
  end
  function ctx:commit_edit()
    host.commit_edit()
  end
  function ctx:cancel_edit()
    host.cancel_edit()
  end
  --- Runs one atomic edit. Do not retain state-table references after rollback.
  ---@param label string
  ---@param action fun()
  function ctx:edit(label, action)
    self:begin_edit(label)
    local ok, message = pcall(action)
    if ok then
      ok, message = pcall(self.commit_edit, self)
    end
    if not ok then
      self:cancel_edit()
      error(message, 0)
    end
  end
  function ctx:clear(custom)
    self:edit('清空画布', function()
      self.state.objects = self.array()
      if custom then
        custom(self)
      end
    end)
    self.temp = {}
  end
end

return M
