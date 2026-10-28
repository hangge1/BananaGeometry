-- Shared Bezier controls for built-in demonstrations and custom cases.
local M = {}

function M.attach(ctx)
  --- Uses state.params.degree/closed; closure changes form one undoable setting edit.
  function ctx:bezier_creation_controls()
    local params = self.state.params
    local choice, changed =
      self.ui.combo('曲线形式', params.closed and 2 or 1, { '开口', '闭合' })
    if changed and (choice == 2) ~= params.closed then
      self:finish_parameter()
      local pending = self:drawing_count() > 0
      local raised = choice == 2 and self.state.params.degree < 3
      self:edit('修改新曲线形式', function()
        self.state.params.closed = choice == 2
        if raised then
          self.state.params.degree = 3
        end
      end)
      self:reset_drawing()
      local message = raised and '闭合至少需要 3 阶，已调整新曲线阶数'
        or '新曲线形式已更新'
      self:notify(message .. (pending and '；未完成绘制已取消' or ''))
    end
    params = self.state.params
    self:creation_parameter('阶数', 'degree', 'integer', params.closed and 3 or 1, 32)
    self.ui.text(
      string.format(
        '%d 阶%s曲线：点击 %d 个控制点%s',
        params.degree,
        params.closed and '闭合' or '开口',
        params.closed and params.degree or params.degree + 1,
        params.closed and '，自动回到起点。' or '。'
      )
    )
  end

  --- Operates on the selected object, independently of creation preferences.
  function ctx:bezier_selection_controls()
    local object = self:object(self.temp.selected)
    if self.mode ~= 'edit' or not object or object.kind ~= 'bezier' then
      return
    end
    self.ui.separator()
    self.ui.text(
      string.format(
        '选中曲线：%d 阶 · %s',
        #object.points - 1,
        object.closed and '已闭合' or '开口'
      )
    )
    if not object.closed and #object.points < 4 then
      self.ui.text('闭合至少需要 3 阶；此曲线保持原阶数。')
      return
    end
    if object.closed then
      self.ui.text('拖“起点/终点”一起移动首尾；断开后可拉开末端。')
    elseif self:handle_label(object, 1) == nil then
      self.ui.text('端点重合，拖动接点可拉开末端。')
    else
      self.ui.text('闭合会将终点合并到起点，可撤销。')
    end
    local closed = object.closed == true
    if self.ui.button(closed and '断开接点' or '闭合曲线') then
      self:finish_parameter()
      local id = object.id
      if self.temp.drag then
        self:cancel_edit()
      end
      self:edit(closed and '断开贝塞尔接点' or '闭合贝塞尔曲线', function()
        self:set_bezier_closed(self:object(id), not closed)
      end)
      self.temp.selected, self.temp.pointer_hint = id, nil
      self:notify(
        closed and '接点已断开；拖动接点可拉开末端'
          or '曲线已闭合；首尾接点联动'
      )
    end
  end
end

return M
