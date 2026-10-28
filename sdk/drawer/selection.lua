-- Selection surrounds painted geometry; never recolors or redraws its body.
local geometry = require('drawer.geometry')
local M = {}

function M.attach(ctx, host)
  --- Draws an external frame, including custom shapes emitted by public helpers.
  --- Bounds include screen-space stroke/radius and measured text at the current zoom.
  function ctx:highlight(object)
    local left, right, bottom, top = math.huge, -math.huge, math.huge, -math.huge
    local function include(p, padding)
      left, right = math.min(left, p.x - padding), math.max(right, p.x + padding)
      bottom, top = math.min(bottom, p.y - padding), math.max(top, p.y + padding)
    end
    local painter = setmetatable({}, { __index = self })
    function painter:emit(shape)
      local points = shape.points or {}
      local padding = math.max(0.5, math.min(32, shape.width or 2)) / (2 * ctx.zoom)
      if shape.kind == 'text' then
        local bounds = ctx:text_bounds(shape)
        if bounds then
          include({ x = bounds.left, y = bounds.bottom }, 0)
          include({ x = bounds.right, y = bounds.top }, 0)
          return
        end
      elseif shape.kind == 'point' then
        padding = math.max(1, math.min(64, shape.radius or 5)) / ctx.zoom
      elseif (shape.kind == 'circle' or shape.kind == 'ellipse') and #points >= 2 then
        local a, b = points[1], points[2]
        local rx = shape.kind == 'circle' and geometry.distance(a, b) or math.abs(a.x - b.x)
        local ry = shape.kind == 'circle' and rx or math.abs(a.y - b.y)
        include({ x = a.x - rx, y = a.y - ry }, padding)
        include({ x = a.x + rx, y = a.y + ry }, padding)
        return
      elseif shape.kind == 'bezier' and #points >= 2 then
        -- Match the renderer's bounded polyline, rather than the control hull.
        points = host.bezier_polyline(points, 0.75 / ctx.zoom)
      end
      for _, p in ipairs(points) do
        include(p, padding)
      end
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
    if custom and custom.bounds then
      for _, p in ipairs(custom.bounds(self, object)) do
        include(p, 0)
      end
    end
    if left > right then
      return
    end
    local gap = 6 / self.zoom
    host.emit({
      kind = 'rect',
      points = { { x = left - gap, y = bottom - gap }, { x = right + gap, y = top + gap } },
      color = { 1, 0.82, 0.25, 1 },
      width = 1.5,
      fill = false,
    })
  end
end

return M
