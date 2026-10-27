-- Owns transient selection, drag and preview state; uses public object methods.
local distance = require('drawer.geometry').distance

local M = {}

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
      if not handle then
        for i = #self.state.objects, 1, -1 do
          local object = self.state.objects[i]
          handle = pick(object)
          if handle then
            selected = object
            break
          end
        end
      end
      if handle then
        self:begin_edit('移动控制点')
        temp.selected, temp.drag = selected.id, handle
        return
      end
      temp.selected = nil
      for i = #self.state.objects, 1, -1 do
        if self:hit(self.state.objects[i], event, tolerance) then
          temp.selected = self.state.objects[i].id
          break
        end
      end
    elseif event.type == 'move' and temp.drag then
      local object = self:object(temp.selected)
      if object then
        local custom = self.types[object.kind]
        local position = { x = event.x, y = event.y }
        if custom and custom.move_handle then
          custom.move_handle(self, object, temp.drag, position)
        else
          object.points[temp.drag] = position
        end
      end
    elseif event.type == 'up' and temp.drag then
      self:commit_edit()
      temp.drag = nil
    end
  end
  ---@param kind string
  ---@param count integer Number of clicks per completed object.
  ---@param event DrawerEvent
  ---@param style? table
  function ctx:draw_event(kind, count, event, style)
    self.temp.pointer = { x = event.x, y = event.y }
    if event.type == 'key' and event.key == 'escape' then
      self.temp = {}
      return
    end
    if event.type ~= 'down' or (event.button and event.button ~= 1) then
      return
    end
    self.temp.pending = self.temp.pending or self.array()
    table.insert(self.temp.pending, { x = event.x, y = event.y })
    if #self.temp.pending == count then
      local points = self.temp.pending
      self:edit('创建对象', function()
        self:add(kind, points, style)
      end)
      self.temp.pending = nil
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
    local pending = self.temp.pending
    if pending then
      for i, p in ipairs(pending) do
        self:point(p, { 0.3, 0.8, 0.8, 1 }, 4)
        if i > 1 then
          self:line(pending[i - 1], p)
        end
      end
      if self.temp.pointer then
        self:line(pending[#pending], self.temp.pointer, { 0.3, 0.8, 0.8, 1 })
      end
    end
  end
end

return M
