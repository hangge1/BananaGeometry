-- Object registration, primitives and hit testing; no interaction state access.
local geometry = require('drawer.geometry')
local distance = geometry.distance
local segment_distance = geometry.segment_distance

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
  function ctx:can_translate(object)
    local custom = self.types[object.kind]
    return (custom and custom.translate ~= nil) or #(object.points or {}) > 0
  end
  --- Translates all geometry from an optional immutable baseline, preserving style.
  --- Call inside an edit. Custom non-point data can implement translate(ctx,object,delta,source).
  ---@param object DrawerObject
  ---@param delta DrawerPoint World-coordinate displacement.
  ---@param source? DrawerObject Original object; repeated previews do not accumulate error.
  function ctx:translate_object(object, delta, source)
    assert(self:can_translate(object), 'Object does not support translation')
    source = source or self.copy(object)
    local custom = self.types[object.kind]
    if custom and custom.translate then
      custom.translate(self, object, delta, source)
      return
    end
    for i, point in ipairs(source.points) do
      object.points[i] = { x = point.x + delta.x, y = point.y + delta.y }
    end
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
  --- Returns world bounds using the renderer's actual screen-space font size.
  --- Text grows rightward and downward from its anchor; nil for empty/unmeasured text.
  ---@param object DrawerObject
  ---@return DrawerTextBounds? bounds World coordinates.
  function ctx:text_bounds(object)
    local anchor = object.points and object.points[1]
    if not anchor then
      return nil
    end
    local width, height = host.measure_text(object.text or '')
    if width <= 0 or height <= 0 then
      return nil
    end
    return {
      left = anchor.x,
      right = anchor.x + width / self.zoom,
      top = anchor.y,
      bottom = anchor.y - height / self.zoom,
    }
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
      if shape.kind == 'text' then
        local bounds = ctx:text_bounds(shape)
        if bounds then
          local padding = 3 / ctx.zoom
          host.emit({
            kind = 'rect',
            points = {
              { x = bounds.left - padding, y = bounds.top + padding },
              { x = bounds.right + padding, y = bounds.bottom - padding },
            },
            color = overlay.color,
            width = 1.5,
          })
        end
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
    if kind == 'text' then
      local bounds = self:text_bounds(object)
      if bounds then
        return p.x >= bounds.left - tolerance
          and p.x <= bounds.right + tolerance
          and p.y <= bounds.top + tolerance
          and p.y >= bounds.bottom - tolerance
      end
      return distance(p, points[1]) <= tolerance
    end
    if kind == 'point' then
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
    if kind == 'bezier' and #points >= 2 then
      local curve = host.bezier_polyline(points, 0.75 / self.zoom)
      for i = 2, #curve do
        if segment_distance(p, curve[i - 1], curve[i]) <= tolerance then
          return true
        end
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
  --- Editing uses closed interiors as grab areas even when the object is unfilled.
  --- Custom hit callbacks retain their own selection policy.
  function ctx:hit_body(object, p, tolerance)
    local custom = self.types[object.kind]
    local closed = object.kind == 'triangle'
      or object.kind == 'polygon'
      or object.kind == 'rect'
      or object.kind == 'circle'
      or object.kind == 'ellipse'
    if closed and not (custom and custom.hit) then
      local area = setmetatable({ fill = true }, { __index = object })
      return self:hit(area, p, tolerance)
    end
    return self:hit(object, p, tolerance)
  end
  function ctx:draw_object(object)
    local custom = self.types[object.kind]
    if custom and custom.draw then
      custom.draw(self, object)
    else
      self:emit(object)
    end
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
