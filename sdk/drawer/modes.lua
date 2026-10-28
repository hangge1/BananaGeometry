-- Owns mode metadata and transitions; feedback is derived from the actual mode.
local M = {}
local kind_names = {
  point = '点',
  line = '线段',
  polyline = '折线',
  triangle = '三角形',
  polygon = '多边形',
  rect = '矩形',
  circle = '圆',
  ellipse = '椭圆',
  bezier = '贝塞尔曲线',
  text = '文本',
}
local cursors = { arrow = true, crosshair = true, move = true, hand = true }

function M.attach(ctx, host)
  local definitions = {
    draw = { label = '绘制模式', cursor = 'crosshair' },
    edit = { label = '调整模式', cursor = 'arrow' },
  }
  --- Registers presentation only; the case retains its own event behavior.
  function ctx:register_mode(id, definition)
    assert(type(id) == 'string' and #id > 0 and #id <= 200, 'Invalid mode ID')
    assert(type(definition.label) == 'string' and #definition.label > 0, 'Missing mode label')
    assert(#definition.label <= 200, 'Mode label is too long')
    assert(
      definition.hint == nil or (type(definition.hint) == 'string' and #definition.hint <= 800),
      'Invalid mode hint'
    )
    assert(cursors[definition.cursor or 'arrow'], 'Invalid mode cursor')
    definitions[id] =
      { label = definition.label, hint = definition.hint, cursor = definition.cursor or 'arrow' }
  end
  function ctx:set_mode(mode)
    assert(type(mode) == 'string' and #mode > 0 and #mode <= 200, 'Invalid mode ID')
    -- Explicit calls retain the v1 reset contract; active UI buttons never call this.
    local canceled = self.temp.drag and '拖动预览已取消'
      or (self:drawing_count() > 0 and '未完成绘制已取消')
    self:cancel_edit()
    self.mode = mode
    if canceled then
      self:notify(canceled)
    end
    return true
  end
  --- Brief non-modal feedback, excluded from document data and history.
  function ctx:notify(message)
    self.temp.mode_notice = { text = message, expires = host.clock() + 2.5 }
  end
  function ctx:mode_feedback()
    local definition = definitions[self.mode] or { label = tostring(self.mode), cursor = 'arrow' }
    local title, hint = definition.label, definition.hint
    if self.mode == 'draw' then
      local spec = self.temp.drawing
      if spec then
        title = title .. ' · ' .. (kind_names[spec.kind] or spec.kind)
        if spec.kind == 'bezier' then
          title = title
            .. string.format(
              ' · %d 阶%s',
              spec.control_count - 1,
              spec.closed and (spec.smooth and '平滑闭合' or '闭合') or '开口'
            )
        end
      end
      if not hint then
        if spec and spec.count > 0 then
          hint = string.format(
            '已落点 %d/%d；继续单击落点，Esc 取消',
            self:drawing_count(),
            spec.count
          )
        elseif spec then
          hint =
            string.format('已落点 %d；Enter / 右键完成，Esc 取消', self:drawing_count())
        else
          hint = '单击画布开始绘制；Esc 取消当前操作'
        end
      end
    elseif self.mode == 'edit' then
      local selected = self.temp.selected and self:object(self.temp.selected)
      title = title
        .. (
          selected and (' · 已选中' .. (kind_names[selected.kind] or selected.kind))
          or ' · 未选择对象'
        )
      if selected and selected.kind == 'bezier' and selected.closed then
        title = title .. (selected.smooth and ' · 平滑闭合' or ' · 已闭合')
        hint = hint
          or (
            selected.smooth and '拖切向点镜像联动，拖接点平移首尾与切向'
            or '拖接点移动首尾，拖其他控制点调整；拖主体平移'
          )
      end
      hint = hint
        or (
          selected and '拖主体平移，拖控制点调整；Esc 取消'
          or '单击选择图形，拖动主体平移'
        )
    end
    local notice = self.temp.mode_notice
    return {
      id = self.mode,
      title = title,
      hint = hint or '按用例说明进行交互',
      cursor = definition.cursor,
      notice = notice and host.clock() < notice.expires and notice.text or '',
    }
  end
end

return M
