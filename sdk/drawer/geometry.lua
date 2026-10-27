-- Pure world-coordinate geometry; no host or mutable context dependencies.
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
  assert(#points >= 2 and #points <= 33, 'Bezier requires 2..33 control points')
  assert(type(t) == 'number' and t >= 0 and t <= 1, 'Bezier parameter must be in [0, 1]')
  if #points ~= 4 then
    -- De Casteljau avoids factorial coefficients and leaves control data intact.
    local work = {}
    for i, point in ipairs(points) do
      work[i] = { x = point.x, y = point.y }
    end
    for remaining = #work - 1, 1, -1 do
      for i = 1, remaining do
        work[i].x = (1 - t) * work[i].x + t * work[i + 1].x
        work[i].y = (1 - t) * work[i].y + t * work[i + 1].y
      end
    end
    return work[1]
  end
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

return {
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
