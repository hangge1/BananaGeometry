-- Owns unfinished construction, completion and previews; uses public object APIs.
local bezier = require('drawer.bezier')
local M = {}
local max_points = 256

function M.attach(ctx)
  function ctx:reset_drawing()
    self.temp.pending, self.temp.pointer, self.temp.drawing = nil, nil, nil
  end

  function ctx:drawing_count()
    return self.temp.pending and #self.temp.pending or 0
  end

  --- Commits a completed construction; false when fewer than the required points exist.
  function ctx:finish_drawing()
    local spec, points = self.temp.drawing, self.temp.pending
    if not spec or not points then
      return false
    end
    local minimum = spec.count
    if minimum == 0 then
      minimum = spec.kind == 'polygon' and 3 or 2
    end
    if #points < minimum then
      return false
    end
    local controls = self.copy(points)
    if spec.closed then
      bezier.close_controls(controls, spec.smooth)
    end
    self:edit('创建对象', function()
      self:add(spec.kind, controls, spec.style)
    end)
    self:reset_drawing()
    return true
  end

  --- Fixed count: completes automatically; count=0: polygon/polyline, Enter/right click.
  --- Closed Bezier collects count-1 clicks and appends the start as its last control.
  --- Smooth closure collects count-2 clicks, also generating the mirrored tangent.
  --- Changing construction constraints cancels unfinished geometry without history.
  ---@param kind string
  ---@param count integer 0..256; Bezier requires 2..33.
  ---@param event DrawerEvent
  ---@param style? table
  function ctx:draw_event(kind, count, event, style)
    assert(
      math.tointeger(count) and count >= 0 and count <= max_points,
      'Invalid drawing point count'
    )
    assert(
      count ~= 0 or kind == 'polygon' or kind == 'polyline',
      'Free drawing requires polygon/polyline'
    )
    assert(kind ~= 'bezier' or (count >= 2 and count <= 33), 'Bezier requires 2..33 control points')
    local closed = kind == 'bezier' and style and style.closed == true or false
    local smooth = kind == 'bezier' and style and style.smooth == true or false
    assert(not closed or count >= 4, 'Closed Bezier requires degree 3..32')
    assert(not smooth or (closed and count >= 5), 'Smooth Bezier requires closed degree 4..32')
    local clicks = count - (closed and 1 or 0) - (smooth and 1 or 0)
    local previous = self.temp.drawing
    if
      previous
      and (
        previous.kind ~= kind
        or previous.count ~= clicks
        or previous.closed ~= closed
        or previous.smooth ~= smooth
      )
    then
      self:reset_drawing()
    end
    self.temp.drawing = {
      kind = kind,
      count = clicks,
      control_count = count,
      closed = closed,
      smooth = smooth,
      style = self.copy(style or {}),
    }
    if event.type == 'key' and event.key == 'escape' then
      self:reset_drawing()
      return
    end
    if
      count == 0
      and (
        (event.type == 'key' and event.key == 'enter')
        or (event.type == 'down' and event.button == 2)
      )
    then
      self:finish_drawing()
      return
    end
    if event.x and event.y then
      self.temp.pointer = { x = event.x, y = event.y }
    end
    if event.type ~= 'down' or (event.button and event.button ~= 1) then
      return
    end
    self.temp.pending = self.temp.pending or self.array()
    if
      smooth
      and #self.temp.pending == 1
      and event.x == self.temp.pending[1].x
      and event.y == self.temp.pending[1].y
    then
      self:notify('切向控制点需要离开起点，请重新落第二点')
      return
    end
    table.insert(self.temp.pending, { x = event.x, y = event.y })
    if (clicks > 0 and #self.temp.pending == clicks) or #self.temp.pending == max_points then
      self:finish_drawing()
    end
  end

  function ctx:render_drawing()
    local pending, spec = self.temp.pending, self.temp.drawing
    if not pending or not spec then
      return
    end
    local points = self.copy(pending)
    if self.temp.pointer then
      points[#points + 1] = self.temp.pointer
    end
    if #points >= 2 then
      local preview = self.copy(spec.style)
      if spec.closed and #points >= 3 then
        bezier.close_controls(points, spec.smooth)
      elseif spec.closed then
        preview.closed, preview.smooth = false, false
      end
      preview.kind, preview.points = spec.kind, points
      preview.color, preview.fill, preview.width = { 0.3, 0.8, 0.8, 0.8 }, false, 1.5
      if preview.kind == 'polygon' and #points < 3 then
        preview.kind = 'polyline'
      end
      self:draw_object(preview)
      if spec.kind == 'bezier' then
        self:emit({ kind = 'polyline', points = points, color = { 0.4, 0.5, 0.6, 1 } })
      end
    end
    for i, point in ipairs(pending) do
      self:point(point, { 0.3, 0.8, 0.8, 1 }, 4)
      self:text({ x = point.x + 8 / self.zoom, y = point.y + 10 / self.zoom }, tostring(i))
    end
  end
end

return M
