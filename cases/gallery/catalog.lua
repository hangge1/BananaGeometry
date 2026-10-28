-- Stable kind identifiers keep saved tool settings independent of list order.
local M = {}
M.items = {
  { kind = 'point', name = '点', count = 1, hint = '点击一个位置创建点。' },
  { kind = 'line', name = '线段', count = 2, hint = '依次点击线段的两个端点。' },
  { kind = 'polyline', name = '折线', hint = '依次点击折线的各个顶点。' },
  { kind = 'triangle', name = '三角形', count = 3, hint = '依次点击三个顶点。' },
  { kind = 'rect', name = '矩形', count = 2, hint = '点击两个对角点。' },
  {
    kind = 'circle',
    name = '圆',
    count = 2,
    hint = '点击圆心，再点击圆周上的一点。',
  },
  {
    kind = 'ellipse',
    name = '椭圆',
    count = 2,
    hint = '点击中心，再点击定义两轴半径的角点。',
  },
  { kind = 'polygon', name = '多边形', hint = '依次点击多边形的各个顶点。' },
  { kind = 'bezier', name = '贝塞尔曲线', hint = '依次点击曲线的控制点。' },
  { kind = 'text', name = '文本', count = 1, hint = '先设置文字，点击画布放置。' },
  {
    kind = 'cross',
    name = '自定义十字',
    count = 1,
    hint = '设置十字大小，点击画布放置中心。',
  },
}
M.labels = {}
for i, item in ipairs(M.items) do
  M.labels[i] = item.name
end
local colors =
  { { 0.35, 0.75, 0.9, 1 }, { 0.65, 0.5, 1, 1 }, { 0.3, 0.85, 0.65, 1 }, { 1, 0.65, 0.3, 1 } }

function M.current(kind)
  for i, item in ipairs(M.items) do
    if item.kind == kind then
      return item, i
    end
  end
  return M.items[2], 2
end
function M.count(item, params)
  if item.count then
    return item.count
  end
  if item.kind == 'bezier' then
    return math.max(1, math.min(32, math.tointeger(params.degree) or 3)) + 1
  end
  if params.free then
    return 0
  end
  return math.max(
    item.kind == 'polygon' and 3 or 2,
    math.min(256, math.tointeger(params.vertices) or 5)
  )
end
function M.style(item, params)
  local style = { color = colors[params.palette] or colors[1], width = params.width }
  if
    item.kind == 'polygon'
    or item.kind == 'triangle'
    or item.kind == 'rect'
    or item.kind == 'circle'
    or item.kind == 'ellipse'
  then
    style.fill = params.fill
  elseif item.kind == 'text' then
    style.text = params.text
  elseif item.kind == 'point' then
    style.radius = params.radius
  elseif item.kind == 'cross' then
    style.size = params.size
  elseif item.kind == 'bezier' then
    style.closed = params.closed
    style.smooth = params.closed and params.smooth
  end
  return style
end
function M.click_count(item, params)
  local count = M.count(item, params)
  if item.kind == 'bezier' and params.closed then
    count = count - 1 - (params.smooth and 1 or 0)
  end
  return count
end
function M.register(ctx)
  ctx:register_type('cross', {
    draw = function(c, object)
      local p, size = object.points[1], object.size
      local color, width = object.color or colors[4], object.width or 2
      c:line({ x = p.x - size, y = p.y }, { x = p.x + size, y = p.y }, color, width)
      c:line({ x = p.x, y = p.y - size }, { x = p.x, y = p.y + size }, color, width)
    end,
    hit = function(c, object, p, tolerance)
      local center, size = object.points[1], object.size
      return c.math.segment_distance(
        p,
        { x = center.x - size, y = center.y },
        { x = center.x + size, y = center.y }
      ) <= tolerance or c.math.segment_distance(
        p,
        { x = center.x, y = center.y - size },
        { x = center.x, y = center.y + size }
      ) <= tolerance
    end,
    handles = function(_, object)
      return object.points
    end,
    move_handle = function(_, object, index, position)
      object.points[index] = position
    end,
  })
end
return M
