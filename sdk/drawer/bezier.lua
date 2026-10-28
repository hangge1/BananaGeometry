-- Single-segment closure constraints; independent of UI and interaction state.
local distance = require('drawer.geometry').distance
local M = {}

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

function M.move_handle(object, index, position)
  object.points[index] = { x = position.x, y = position.y }
  if object.closed and (index == 1 or index == #object.points) then
    object.points[1] = { x = position.x, y = position.y }
    object.points[#object.points] = { x = position.x, y = position.y }
  end
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
    return true
  end
end

return M
