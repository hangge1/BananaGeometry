-- Drawer SDK v1. All editable data belongs to ctx.state; temp/mode are transient.
---@class DrawerPoint
---@field x number Mathematical world X coordinate.
---@field y number Mathematical world Y coordinate, increasing upward.

---@class DrawerObject
---@field id integer Stable within one document.
---@field kind string Built-in or registered object type.
---@field points DrawerPoint[]
---@field color? number[] RGBA components in the range 0 to 1.
---@field width? number Stroke width in screen pixels.
---@field fill? boolean

---@class DrawerState
---@field objects DrawerObject[]
---@field params table<string, any>
---@field next_id integer

---@class DrawerEvent : DrawerPoint
---@field type string down, move, up, key or key_up.
---@field button integer 1 for left, 2 for right, 0 for non-button events.
---@field key string Lowercase key name.
---@field ctrl boolean
---@field shift boolean
---@field alt boolean

---@class DrawerContext
---@field state DrawerState Replaced when an edit is canceled or history changes.
---@field temp table Transient state, excluded from history and documents.
---@field mode string
---@field zoom number Host-provided screen pixels per world unit.
---@field types table<string, table>
---@field ui table
local M = {}

-- The host injects this table once per isolated Lua VM.
---@diagnostic disable-next-line: undefined-global
local host = _host
local function copy_value(value)
  if type(value) ~= 'table' then
    return value
  end
  local result = getmetatable(value) and host.array() or {}
  for key, item in pairs(value) do
    result[key] = copy_value(item)
  end
  return result
end
local function distance(a, b)
  return math.sqrt((a.x - b.x) ^ 2 + (a.y - b.y) ^ 2)
end
local function segment_distance(p, a, b)
  local dx, dy = b.x - a.x, b.y - a.y
  local length = dx * dx + dy * dy
  local t = length == 0 and 0
    or math.max(0, math.min(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / length))
  return distance(p, { x = a.x + t * dx, y = a.y + t * dy })
end
local function bezier_point(points, t)
  local u = 1 - t
  return {
    x = u ^ 3 * points[1].x
      + 3 * u ^ 2 * t * points[2].x
      + 3 * u * t ^ 2 * points[3].x
      + t ^ 3 * points[4].x,
    y = u ^ 3 * points[1].y
      + 3 * u ^ 2 * t * points[2].y
      + 3 * u * t ^ 2 * points[3].y
      + t ^ 3 * points[4].y,
  }
end

---@return DrawerContext
function M.new()
  local ctx = {
    state = { objects = host.array(), params = {}, next_id = 1 },
    temp = {},
    mode = 'draw',
    zoom = 1,
    types = {},
  }
  ctx.math = {
    distance = distance,
    segment_distance = segment_distance,
    bezier = bezier_point,
    add = function(a, b)
      return { x = a.x + b.x, y = a.y + b.y }
    end,
    sub = function(a, b)
      return { x = a.x - b.x, y = a.y - b.y }
    end,
    scale = function(a, s)
      return { x = a.x * s, y = a.y * s }
    end,
    dot = function(a, b)
      return a.x * b.x + a.y * b.y
    end,
  }
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
  ctx.array = host.array
  ctx.copy = copy_value
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
  function ctx:set_mode(mode)
    self:cancel_edit()
    self.temp = {}
    self.mode = mode
  end
  --- Adds persistent data; call inside an edit or during initialization.
  ---@param kind string
  ---@param points DrawerPoint[]
  ---@param style? table
  ---@return DrawerObject
  function ctx:add(kind, points, style)
    local object = copy_value(style or {})
    object.kind, object.points, object.id = kind, copy_value(points), self.state.next_id
    self.state.next_id = self.state.next_id + 1
    table.insert(self.state.objects, object)
    return object
  end
  function ctx:object(id)
    for _, object in ipairs(self.state.objects) do
      if object.id == id then
        return object
      end
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
  function ctx:register_type(name, definition)
    self.types[name] = definition
  end
  ---@param object DrawerObject
  ---@return DrawerPoint[]
  function ctx:handles(object)
    local custom = self.types[object.kind]
    if custom and custom.handles then
      return custom.handles(self, object)
    end
    return object.points or {}
  end
  function ctx:emit(shape)
    host.emit(shape)
  end
  function ctx:draw_batch(shapes)
    host.emit(shapes)
  end
  function ctx:line(a, b, color, width)
    self:emit({
      kind = 'line',
      points = { a, b },
      color = color or { 0.45, 0.55, 0.7, 1 },
      width = width or 1,
    })
  end
  function ctx:point(p, color, radius)
    self:emit({
      kind = 'point',
      points = { p },
      color = color or { 1, 0.75, 0.3, 1 },
      radius = radius or 5,
    })
  end
  function ctx:text(p, text, color)
    self:emit({
      kind = 'text',
      points = { p },
      text = text,
      color = color or { 0.8, 0.85, 0.95, 1 },
    })
  end
  ---@param object DrawerObject
  ---@param p DrawerPoint
  ---@param tolerance number World units, normally derived from pixel tolerance.
  ---@return boolean
  function ctx:hit(object, p, tolerance)
    local custom = self.types[object.kind]
    if custom and custom.hit then
      return custom.hit(self, object, p, tolerance)
    end
    local points, kind = object.points or {}, object.kind
    if #points == 0 then
      return false
    end
    if kind == 'point' or kind == 'text' then
      return distance(p, points[1]) <= tolerance
    end
    if kind == 'circle' or kind == 'ellipse' then
      if #points < 2 then
        return false
      end
      local rx = kind == 'circle' and distance(points[1], points[2])
        or math.abs(points[2].x - points[1].x)
      local ry = kind == 'circle' and rx or math.abs(points[2].y - points[1].y)
      if rx < 1e-12 or ry < 1e-12 then
        return segment_distance(p, points[1], points[2]) <= tolerance
      end
      local d = math.sqrt(((p.x - points[1].x) / rx) ^ 2 + ((p.y - points[1].y) / ry) ^ 2)
      return (object.fill and d <= 1) or math.abs(d - 1) * math.min(rx, ry) <= tolerance
    end
    if kind == 'bezier' and #points == 4 then
      local previous = points[1]
      for i = 1, 48 do
        local next_point = bezier_point(points, i / 48)
        if segment_distance(p, previous, next_point) <= tolerance then
          return true
        end
        previous = next_point
      end
      return false
    end
    if kind == 'rect' and #points >= 2 then
      points = {
        points[1],
        { x = points[2].x, y = points[1].y },
        points[2],
        {
          x = points[1].x,
          y = points[2].y,
        },
      }
    end
    for i = 2, #points do
      if segment_distance(p, points[i - 1], points[i]) <= tolerance then
        return true
      end
    end
    local closed = kind == 'triangle' or kind == 'polygon' or kind == 'rect'
    if closed and segment_distance(p, points[#points], points[1]) <= tolerance then
      return true
    end
    if closed and object.fill then
      local inside, j = false, #points
      for i = 1, #points do
        local a, b = points[i], points[j]
        if (a.y > p.y) ~= (b.y > p.y) and p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x then
          inside = not inside
        end
        j = i
      end
      return inside
    end
    return false
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
  function ctx:render()
    local standard = self.array()
    for _, object in ipairs(self.state.objects) do
      local custom = self.types[object.kind]
      if custom and custom.draw then
        self:draw_batch(standard)
        standard = self.array()
        custom.draw(self, object)
      else
        table.insert(standard, object)
      end
    end
    self:draw_batch(standard)
    local object = self:object(self.temp.selected)
    if object then
      local handles = self:handles(object)
      if object.kind == 'bezier' then
        for i = 2, #handles do
          self:line(handles[i - 1], handles[i])
        end
      end
      for i, p in ipairs(handles) do
        self:point(p)
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
  return ctx
end
return M
