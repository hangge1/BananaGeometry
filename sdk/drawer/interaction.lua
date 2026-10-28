-- Selection, handle editing and object translation share one pointer gesture.
local distance = require('drawer.geometry').distance
local M = {}
local drag_threshold = 3 -- Screen pixels; clicks and small jitters stay selection-only.
local arrows = {
  leftarrow = { x = -1, y = 0 },
  rightarrow = { x = 1, y = 0 },
  uparrow = { x = 0, y = 1 },
  downarrow = { x = 0, y = -1 },
}

local function pick(ctx, event)
  local tolerance = 9 / ctx.zoom
  local function handle(object)
    for index, point in ipairs(ctx:handles(object)) do
      if distance(event, point) <= tolerance then
        return index
      end
    end
  end
  local selected = ctx:object(ctx.temp.selected)
  if selected then
    local index = handle(selected)
    if index then
      if event.alt then
        return selected
      end
      return selected, index
    end
  end
  for i = #ctx.state.objects, 1, -1 do
    local object = ctx.state.objects[i]
    local index = handle(object)
    if index then
      if event.alt then
        return object
      end
      return object, index
    end
    if ctx:hit_body(object, event, tolerance) then
      return object
    end
  end
end

local function hint(ctx, object, handle)
  if handle then
    return 'handle'
  end
  if object and ctx:can_translate(object) then
    return 'move'
  end
end

local function update_drag(ctx, event)
  local drag = ctx.temp.drag
  local object = ctx:object(ctx.temp.selected)
  if not object or not event.x or not event.y then
    return
  end
  local delta = { x = event.x - drag.origin.x, y = event.y - drag.origin.y }
  if not drag.started and math.sqrt(delta.x ^ 2 + delta.y ^ 2) * ctx.zoom < drag_threshold then
    return
  end
  drag.started = true
  if drag.handle then
    local position = { x = drag.anchor.x + delta.x, y = drag.anchor.y + delta.y }
    local custom = ctx.types[object.kind]
    if custom and custom.move_handle then
      custom.move_handle(ctx, object, drag.handle, position)
    else
      object.points[drag.handle] = position
    end
  else
    if event.shift then
      if math.abs(delta.x) >= math.abs(delta.y) then
        delta.y = 0
      else
        delta.x = 0
      end
    end
    ctx:translate_object(object, delta, drag.source)
  end
end

function M.attach(ctx)
  --- Handle drags reshape; body drags translate. Alt forces translation, Shift locks an axis.
  --- Every gesture snapshots its origin, preserving shape, grab offset and undo boundaries.
  ---@param event DrawerEvent
  function ctx:edit_event(event)
    if (event.type == 'down' or event.type == 'up') and event.button and event.button ~= 1 then
      return
    end
    local temp = self.temp
    if event.type == 'key' and event.key == 'escape' then
      self:cancel_edit()
      return
    end
    if event.type == 'key' and event.key == 'delete' and temp.selected then
      local id = temp.selected
      self:cancel_edit()
      self:edit('删除对象', function()
        for i, object in ipairs(self.state.objects) do
          if object.id == id then
            table.remove(self.state.objects, i)
            break
          end
        end
      end)
      return
    end
    if
      event.type == 'key'
      and arrows[event.key]
      and not temp.drag
      and not event.ctrl
      and not event.alt
    then
      local object = self:object(temp.selected)
      if object and self:can_translate(object) then
        local direction = arrows[event.key]
        local step = (event.shift and 10 or 1) / self.zoom
        self:edit('键盘平移图形', function()
          self:translate_object(object, { x = direction.x * step, y = direction.y * step })
        end)
      end
      return
    end
    if event.type == 'down' then
      if temp.drag then
        self:cancel_edit()
        temp = self.temp
      end
      local object, handle = pick(self, event)
      temp.selected = object and object.id or nil
      temp.pointer_hint = hint(self, object, handle)
      if object and (handle or self:can_translate(object)) then
        self:begin_edit(handle and '移动控制点' or '平移图形')
        temp.drag = {
          origin = { x = event.x, y = event.y },
          source = self.copy(object),
          handle = handle,
          anchor = handle and self.copy(self:handles(object)[handle]),
          started = false,
        }
      end
    elseif event.type == 'move' and temp.drag then
      update_drag(self, event)
    elseif event.type == 'up' and temp.drag then
      update_drag(self, event)
      self:commit_edit()
      temp.drag = nil
      local object, handle = pick(self, event)
      temp.pointer_hint = hint(self, object, handle)
    elseif event.x and event.y then
      local object, handle = pick(self, event)
      temp.pointer_hint = hint(self, object, handle)
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
