-- Shared Bezier controls for built-in demonstrations and custom cases.
local bezier = require('drawer.bezier')
local M = {}

local function edit_selected(ctx, object, label, action, notice)
  ctx:finish_parameter()
  local id = object.id
  if ctx.temp.drag then
    ctx:cancel_edit()
  end
  ctx:edit(label, function()
    action(ctx:object(id))
  end)
  ctx.temp.selected, ctx.temp.pointer_hint = id, nil
  ctx:notify(notice)
end

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
        if choice == 1 then
          self.state.params.smooth = false
        end
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
    if params.closed then
      local smooth, smooth_changed =
        self.ui.checkbox('平滑接点##new_bezier', params.smooth == true)
      if smooth_changed and smooth ~= (params.smooth == true) then
        self:finish_parameter()
        local pending = self:drawing_count() > 0
        local raised = smooth and params.degree < 4
        self:edit('修改新曲线平滑接点', function()
          params.smooth = smooth
          if raised then
            params.degree = 4
          end
        end)
        self:reset_drawing()
        local message = raised and '平滑闭合至少需要 4 阶，已调整新曲线阶数'
          or '新曲线平滑设置已更新'
        self:notify(message .. (pending and '；未完成绘制已取消' or ''))
      end
    end
    local minimum = params.closed and (params.smooth and 4 or 3) or 1
    self:creation_parameter('阶数', 'degree', 'integer', minimum, 32)
    local clicks = params.degree
      + 1
      - (params.closed and 1 or 0)
      - (params.closed and params.smooth and 1 or 0)
    self.ui.text(
      string.format(
        '%d 阶%s曲线：点击 %d 个控制点%s',
        params.degree,
        params.closed and (params.smooth and '平滑闭合' or '闭合') or '开口',
        clicks,
        params.closed
            and (params.smooth and '，自动补齐切向与终点。' or '，自动回到起点。')
          or '。'
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
        object.closed and (object.smooth and '平滑闭合' or '已闭合') or '开口'
      )
    )
    if not object.closed and #object.points < 4 then
      self.ui.text('闭合至少需要 3 阶；此曲线保持原阶数。')
      return
    end
    if object.closed then
      if #object.points < 5 then
        self.ui.text('平滑接点至少需要 4 阶；此曲线保持原阶数。')
      elseif not bezier.can_smooth(object) then
        self.ui.text('先将接点旁控制点拉离接点，再启用平滑。')
      else
        local smooth, changed =
          self.ui.checkbox('平滑接点##selected_bezier', object.smooth == true)
        if changed and smooth ~= (object.smooth == true) then
          edit_selected(
            self,
            object,
            smooth and '启用平滑接点' or '取消平滑接点',
            function(selected)
              self:set_bezier_smooth(selected, smooth)
            end,
            smooth and '接点已平滑；两侧切向镜像联动，可撤销'
              or '已解除平滑约束，保留当前形状'
          )
          object = self:object(self.temp.selected)
        end
      end
    end
    if object.closed then
      self.ui.text(
        object.smooth and '拖切向点镜像联动；拖接点移动首尾与切向。'
          or '拖“起点/终点”一起移动首尾；断开后可拉开末端。'
      )
    elseif self:handle_label(object, 1) == nil then
      self.ui.text('端点重合，拖动接点可拉开末端。')
    else
      self.ui.text('闭合会将终点合并到起点，可撤销。')
    end
    local closed = object.closed == true
    if self.ui.button(closed and '断开接点' or '闭合曲线') then
      edit_selected(
        self,
        object,
        closed and '断开贝塞尔接点' or '闭合贝塞尔曲线',
        function(selected)
          self:set_bezier_closed(selected, not closed)
        end,
        closed and '接点已断开；拖动接点可拉开末端'
          or '曲线已闭合；首尾接点联动'
      )
    end
  end
end

return M
