-- Owns transient selection, drag and preview state; uses public object methods.
local distance = require('drawer.geometry').distance

local M = {}

local function begin_drag(ctx, object, handle, event)
  local anchor = ctx:handles(object)[handle]
  ctx:begin_edit('移动控制点')
  ctx.temp.selected, ctx.temp.drag = object.id, handle
  -- Preserve the grab offset so clicking a label or near a grip never snaps it.
  ctx.temp.drag_offset = { x = event.x - anchor.x, y = event.y - anchor.y }
end

function M.attach(ctx)
  function ctx:set_mode(mode)
    self:cancel_edit()
    self.temp = {}
    self.mode = mode
  end
  ---@param event DrawerEvent
  function ctx:edit_event(event)
    if event.type == 'down' and event.button and event.button ~= 1 then
      return
    end
    local temp = self.temp
    if event.type == 'key' and event.key == 'escape' then
      self:cancel_edit()
      self.temp = {}
      return
    end
    if event.type == 'key' and event.key == 'delete' and temp.selected then
      local id = temp.selected
      self:edit('删除对象', function()
        for i, object in ipairs(self.state.objects) do
          if object.id == id then
            table.remove(self.state.objects, i)
            break
          end
        end
      end)
      self.temp = {}
      return
    end
    if event.type == 'down' then
      local tolerance = 9 / self.zoom
      local selected = self:object(temp.selected)
      local function pick(object)
        for i, p in ipairs(self:handles(object)) do
          if distance(event, p) <= tolerance then
            return i
          end
        end
      end
      local handle = selected and pick(selected)
      if handle then
        begin_drag(self, selected, handle, event)
        return
      end
      temp.selected = nil
      -- Respect drawing order: a rear grip must not steal a foreground body hit.
      for i = #self.state.objects, 1, -1 do
        local object = self.state.objects[i]
        handle = pick(object)
        if handle then
          begin_drag(self, object, handle, event)
          return
        end
        if self:hit(object, event, tolerance) then
          temp.selected = object.id
          if object.kind == 'text' then
            begin_drag(self, object, 1, event)
          end
          return
        end
      end
    elseif event.type == 'move' and temp.drag then
      local object = self:object(temp.selected)
      if object then
        local custom = self.types[object.kind]
        local offset = temp.drag_offset or { x = 0, y = 0 }
        local position = { x = event.x - offset.x, y = event.y - offset.y }
        if custom and custom.move_handle then
          custom.move_handle(self, object, temp.drag, position)
        else
          object.points[temp.drag] = position
        end
      end
    elseif event.type == 'up' and temp.drag then
      self:commit_edit()
      temp.drag = nil
      temp.drag_offset = nil
    end
  end
  function ctx:render_interaction()
    local object = self:object(self.temp.selected)
    if object then
      self:highlight(object)
      local handles = self:handles(object)
      if object.kind == 'bezier' then
        for i = 2, #handles do
          self:line(handles[i - 1], handles[i])
        end
      end
      for i, p in ipairs(handles) do
        self:point(p, { 0.05, 0.09, 0.15, 1 }, 7)
        self:point(p, { 1, 0.82, 0.25, 1 }, 5)
        self:text({ x = p.x + 10 / self.zoom, y = p.y + 12 / self.zoom }, tostring(i))
      end
    end
  end
end

return M
