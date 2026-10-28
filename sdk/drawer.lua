-- Drawer SDK v1. All editable data belongs to ctx.state; temp/mode are transient.
---@class DrawerPoint
---@field x number Mathematical world X coordinate.
---@field y number Mathematical world Y coordinate, increasing upward.

---@class DrawerObject
---@field id integer Stable within one document.
---@field kind string Built-in or registered object type.
---@field points DrawerPoint[]
---@field color? number[] RGBA components in the range 0 to 1.
---@field width? number Stroke width in screen pixels.
---@field fill? boolean
---@field closed? boolean Bezier endpoints are linked; requires degree 3..32.
---@field text? string UTF-8 content for text objects.

---@class DrawerTextBounds
---@field left number
---@field right number
---@field top number Mathematical Y, above bottom.
---@field bottom number

---@class DrawerState
---@field objects DrawerObject[]
---@field params table<string, any>
---@field next_id integer

---@class DrawerEvent : DrawerPoint
---@field type string down, move, up, key or key_up.
---@field button integer 1 for left, 2 for right, 0 for non-button events.
---@field key string Lowercase key name.
---@field ctrl boolean
---@field shift boolean
---@field alt boolean

---@class DrawerContext
---@field state DrawerState Replaced when an edit is canceled or history changes.
---@field temp table Transient state, excluded from history and documents.
---@field mode string
---@field zoom number Host-provided screen pixels per world unit.
---@field types table<string, table>
---@field ui table
local M = {}
local geometry = require('drawer.geometry')
local state = require('drawer.state')
local objects = require('drawer.objects')
local bezier = require('drawer.bezier')
local bezier_ui = require('drawer.bezier_ui')
local interaction = require('drawer.interaction')
local drawing = require('drawer.drawing')
local ui = require('drawer.ui')
local modes = require('drawer.modes')

-- The host injects this table once per isolated Lua VM.
---@diagnostic disable-next-line: undefined-global
local host = _host

---@return DrawerContext
function M.new()
  local ctx = {
    state = { objects = host.array(), params = {}, next_id = 1 },
    temp = {},
    mode = 'draw',
    zoom = 1,
    types = {},
    math = geometry,
  }
  state.attach(ctx, host)
  objects.attach(ctx, host)
  bezier.attach(ctx)
  interaction.attach(ctx)
  drawing.attach(ctx)
  modes.attach(ctx, host)
  ui.attach(ctx, host)
  bezier_ui.attach(ctx)
  function ctx:render()
    self:render_objects()
    self:render_interaction()
    self:render_drawing()
  end
  return ctx
end

return M
