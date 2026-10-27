local math_api = ctx.math
assert(math_api.distance({ x = 0, y = 0 }, { x = 3, y = 4 }) == 5)
assert(math_api.segment_distance({ x = 5, y = 2 }, { x = 0, y = 0 }, { x = 10, y = 0 }) == 2)
assert(math_api.segment_distance({ x = 3, y = 4 }, { x = 0, y = 0 }, { x = 0, y = 0 }) == 5)
local points = { { x = 0, y = 0 }, { x = 10, y = 40 }, { x = 30, y = 40 }, { x = 40, y = 0 } }
assert(math_api.bezier(points, 0).x == points[1].x)
assert(math_api.bezier(points, 1).x == points[4].x)
local midpoint = math_api.bezier(points, 0.5)
assert(midpoint.x == 20 and midpoint.y == 30)
local triangle = {
  kind = 'triangle',
  points = { { x = 0, y = 0 }, { x = 10, y = 0 }, { x = 0, y = 10 } },
  fill = true,
}
assert(ctx:hit(triangle, { x = 2, y = 2 }, 0.1))
assert(not ctx:hit(triangle, { x = 20, y = 20 }, 0.1))
