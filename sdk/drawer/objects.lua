-- Object registration, primitives and hit testing; no interaction state access.
local geometry = require('drawer.geometry')
local distance = geometry.distance
local segment_distance = geometry.segment_distance
local bezier_point = geometry.bezier

local M = {}

function M.attach(ctx, host)
  --- Adds persistent data; call inside an edit or during initialization.
  ---@param kind string
  ---@param points DrawerPoint[]
  ---@param style? table
  ---@return DrawerObject
  function ctx:add(kind, points, style)
    local object = self.copy(style or {})
    object.kind, object.points, object.id = kind, self.copy(points), self.state.next_id
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
  --- Draws temporary selection feedback without changing persistent styles.
  --- Custom draw callbacks keep using the public drawing helpers.
  ---@param object DrawerObject
  function ctx:highlight(object)
    local painter = setmetatable({}, { __index = self })
    function painter:emit(shape)
      local overlay = ctx.copy(shape)
      overlay.color = { 1, 0.82, 0.25, 1 }
      overlay.width = (shape.width or 2) + 2
      overlay.fill = false
      if shape.kind == 'point' then
        overlay.radius = (shape.radius or 5) + 2
      end
      host.emit(overlay)
    end
    function painter:draw_batch(shapes)
      for _, shape in ipairs(shapes) do
        self:emit(shape)
      end
    end
    local custom = self.types[object.kind]
    if custom and custom.draw then
      custom.draw(painter, object)
    else
      painter:emit(object)
    end
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
  function ctx:render_objects()
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
  end
end

return M
