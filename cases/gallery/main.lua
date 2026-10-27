-- Demonstrates every built-in primitive and a custom persistent object.
local function register(ctx)
  ctx:register_type('cross', {
    draw = function(c, o)
      local p, s = o.points[1], o.size
      c:line({ x = p.x - s, y = p.y }, { x = p.x + s, y = p.y }, { 1, 0.6, 0.4, 1 }, 2)
      c:line({ x = p.x, y = p.y - s }, { x = p.x, y = p.y + s }, { 1, 0.6, 0.4, 1 }, 2)
    end,
    hit = function(c, o, p, t)
      return c.math.distance(o.points[1], p) <= o.size + t
    end,
    handles = function(_, o)
      return o.points
    end,
    move_handle = function(_, o, i, p)
      o.points[i] = p
    end,
  })
end
return {
  id = 'drawer.gallery',
  name = '图元与自定义对象 / Gallery',
  data_version = 1,
  default_mode = 'edit',
  on_init = function(ctx)
    register(ctx)
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
    add(
      'text',
      { { x = 100, y = -65 } },
      { text = '二维图形 SDK', color = { 0.8, 0.85, 1, 1 } }
    )
    add('point', { { x = 120, y = -110 } }, { radius = 6, color = { 1, 0.65, 0.3, 1 } })
    add('line', { { x = -260, y = 200 }, { x = 240, y = 200 } })
    add('cross', { { x = 0, y = 0 } }, { size = 25 })
  end,
  on_ui = function(ctx)
    ctx.ui.text('内置图元与 Lua 自定义十字对象')
    ctx.ui.text('单击图形高亮并显示控制点；文字区域可直接拖动。')
    ctx.ui.text('拖动控制点调整，Delete 删除，Esc 取消。')
    ctx.ui.text('保存后加载可继续编辑自定义对象。')
  end,
  on_event = function(ctx, event)
    ctx:edit_event(event)
  end,
  on_draw = function(ctx)
    ctx:render()
  end,
}
