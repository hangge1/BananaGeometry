-- Single-segment closure and C1 constraints; independent of UI and interaction state.
local distance = require('drawer.geometry').distance
local M = {}

local function same_point(a, b)
  return a.x == b.x and a.y == b.y
end

local function reflect(anchor, point)
  return { x = 2 * anchor.x - point.x, y = 2 * anchor.y - point.y }
end

--- Completes an independent, caller-owned point array for construction/preview.
function M.close_controls(points, smooth)
  if smooth then
    assert(not same_point(points[1], points[2]), 'Smooth Bezier requires a nonzero seam tangent')
    points[#points + 1] = reflect(points[1], points[2])
  end
  points[#points + 1] = { x = points[1].x, y = points[1].y }
end

function M.sync_tangent(object)
  if object.smooth then
    object.points[#object.points - 1] = reflect(object.points[1], object.points[2])
  end
end

function M.can_smooth(object)
  local points = object.points
  return object.closed == true
    and #points >= 5
    and #points <= 33
    and (not same_point(points[1], points[2]) or not same_point(points[1], points[#points - 1]))
end

local function coincident_endpoints(object)
  local points = object.points
  return #points >= 2 and points[1].x == points[#points].x and points[1].y == points[#points].y
end

function M.handles(object)
  if not object.closed then
    return object.points
  end
  local handles = {}
  for i = 1, #object.points - 1 do
    handles[i] = object.points[i]
  end
  return handles
end

function M.preferred_handle(object, point, tolerance)
  -- After disconnecting, the shared location must let the user pull the end away.
  if not object.closed and coincident_endpoints(object) then
    local index = #object.points
    if distance(point, object.points[index]) <= tolerance then
      return index
    end
  end
end

function M.handle_label(object, index)
  if object.closed and index == 1 then
    return '起点/终点'
  end
  if object.smooth and (index == 2 or index == #object.points - 1) then
    return tostring(index) .. ' · 切向'
  end
  if not object.closed and coincident_endpoints(object) and index == 1 then
    return nil
  end
  if index == 1 then
    return '1 · 起点'
  end
  if index == #object.points then
    return tostring(index) .. ' · 终点'
  end
  return tostring(index)
end

function M.move_handle(object, index, position, source)
  local original = source.points
  local last = #object.points
  if object.smooth then
    if index == 1 or index == last then
      local delta = { x = position.x - original[1].x, y = position.y - original[1].y }
      object.points[2] = { x = original[2].x + delta.x, y = original[2].y + delta.y }
    elseif index == 2 or index == last - 1 then
      if same_point(position, original[1]) then
        return false
      end
      local opposite = index == 2 and last - 1 or 2
      object.points[opposite] = reflect(original[1], position)
    end
  end
  object.points[index] = { x = position.x, y = position.y }
  if object.closed and (index == 1 or index == #object.points) then
    object.points[1] = { x = position.x, y = position.y }
    object.points[#object.points] = { x = position.x, y = position.y }
  end
  if object.smooth and (index == 1 or index == last) then
    M.sync_tangent(object)
  end
  return true
end

function M.attach(ctx)
  --- Merges the end into the start, or releases their link without moving either.
  --- Call inside an edit. Closing preserves degree and requires 4..33 controls.
  ---@param object DrawerObject
  ---@param closed boolean
  ---@return boolean changed
  function ctx:set_bezier_closed(object, closed)
    assert(object.kind == 'bezier' and type(closed) == 'boolean', 'Expected Bezier and boolean')
    assert(#object.points >= 2 and #object.points <= 33, 'Bezier requires 2..33 control points')
    assert(not closed or #object.points >= 4, 'Closed Bezier requires degree 3..32')
    if (object.closed == true) == closed then
      return false
    end
    if closed then
      local start = object.points[1]
      object.points[#object.points] = { x = start.x, y = start.y }
    end
    object.closed = closed
    if not closed and object.smooth then
      object.smooth = false
    end
    return true
  end
  --- Enables C1 mirror linkage without changing degree; disabling preserves shape.
  --- Call inside an edit. Requires closure, degree 4..32 and a usable seam tangent.
  ---@param object DrawerObject
  ---@param enabled boolean
  ---@return boolean changed
  function ctx:set_bezier_smooth(object, enabled)
    assert(object.kind == 'bezier' and type(enabled) == 'boolean', 'Expected Bezier and boolean')
    if (object.smooth == true) == enabled then
      return false
    end
    if enabled then
      assert(
        M.can_smooth(object),
        'Move a seam tangent away from the seam; smoothing requires closed degree 4..32'
      )
      local points = object.points
      if same_point(points[1], points[2]) then
        points[2] = reflect(points[1], points[#points - 1])
      end
      points[#points - 1] = reflect(points[1], points[2])
    end
    object.smooth = enabled
    return true
  end
end

return M
