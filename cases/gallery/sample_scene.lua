-- Optional reference scene; callers choose the transaction and insertion policy.
local M = {}

function M.populate(ctx)
  local function add(kind, points, extra)
    ctx:add(kind, points, extra or { color = { 0.35, 0.75, 0.9, 1 }, width = 2 })
  end
  add('circle', { { x = -220, y = 100 }, { x = -170, y = 100 } })
  add('ellipse', { { x = -60, y = 100 }, { x = 10, y = 135 } })
  add(
    'rect',
    { { x = 100, y = 60 }, { x = 220, y = 140 } },
    { fill = true, color = { 0.5, 0.7, 1, 1 } }
  )
  add('polygon', {
    { x = -220, y = -50 },
    { x = -140, y = -50 },
    { x = -180, y = -80 },
    { x = -140, y = -120 },
    {
      x = -220,
      y = -120,
    },
  }, { fill = true, color = { 0.6, 0.5, 1, 1 } })
  add('polyline', { { x = -60, y = -50 }, { x = -20, y = -110 }, { x = 40, y = -40 } })
  add('text', { { x = 100, y = -65 } }, { text = '二维图形 SDK', color = { 0.8, 0.85, 1, 1 } })
  add('point', { { x = 120, y = -110 } }, { radius = 6, color = { 1, 0.65, 0.3, 1 } })
  add('line', { { x = -260, y = 200 }, { x = 240, y = 200 } })
  add('cross', { { x = 0, y = 0 } }, { size = 25 })
end

return M
