-- Tree selection and property editing use the same transactions as canvas gestures.
local M = {}
function M.attach(ctx)
  function ctx:finish_ui_edit()
    self:finish_parameter()
    local selected = self.temp.selected
    self:cancel_edit()
    self.temp = { selected = selected and self:object(selected) and selected or nil }
  end
  function ctx:select_object(id, editor_mode)
    self:finish_parameter()
    if id ~= 0 and editor_mode ~= '' then
      self:set_mode(editor_mode)
    else
      self:cancel_edit()
    end
    if id ~= 0 then
      assert(self:object(id), 'Object no longer exists')
    end
    self.temp.selected = id ~= 0 and id or nil
    if id ~= 0 and editor_mode == '' then
      self:notify('此用例未声明调整模式，仅查看对象')
    end
  end
  function ctx:selected_object_id()
    return self.temp.selected or 0
  end
  function ctx:delete_selected_object()
    assert(self.editor_mode ~= '', 'This case does not declare an editing mode')
    local id = self.temp.selected
    self:finish_parameter()
    self:cancel_edit()
    if not id or not self:object(id) then
      return
    end
    self:edit('删除对象', function()
      for i, o in ipairs(self.state.objects) do
        if o.id == id then
          table.remove(self.state.objects, i)
          break
        end
      end
    end)
  end
  local function property(self, object, key, label, kind, value, apply, ...)
    local next_value, changed, finished = self.ui[kind](label, value, ...)
    local tag = 'object:' .. object.id .. ':' .. key
    if changed then
      if self.temp.parameter and self.temp.parameter ~= tag then
        self:finish_parameter()
      end
      if not self.temp.parameter then
        self:begin_edit('修改对象：' .. label)
        self.temp.parameter = tag
      end
      apply(next_value)
    end
    if finished and self.temp.parameter == tag then
      self:finish_parameter()
    end
  end
  function ctx:inspect_object(id)
    local object = self:object(id)
    if not object then
      return
    end
    if self.editor_mode == '' then
      self.ui.text('名称：' .. (object.display_name or '未命名'))
      self.ui.text('类型：' .. object.kind .. '    ID：' .. object.id)
      self.ui.text('此用例未声明 edit_mode，仅查看对象')
      return
    end
    property(self, object, 'name', '名称', 'input', object.display_name or '', function(v)
      object.display_name = v
    end)
    self.ui.text('类型：' .. object.kind .. '    ID：' .. object.id)
    if object.kind == 'text' then
      property(
        self,
        object,
        'font_size',
        '字号(px)',
        'integer',
        self:text_font_size(object),
        function(v)
          object.font_size = v
        end,
        6,
        256
      )
    else
      property(self, object, 'width', '线宽', 'slider', object.width or 2, function(v)
        object.width = v
      end, 0.5, 32)
    end
    property(
      self,
      object,
      'color',
      '颜色',
      'color',
      object.color or { 0.8, 0.85, 0.95, 1 },
      function(v)
        object.color = self.copy(v)
      end
    )
    if object.kind == 'text' then
      property(self, object, 'text', '文字', 'input', object.text or '', function(v)
        object.text = v
      end)
    end
    if
      object.kind == 'polygon'
      or object.kind == 'triangle'
      or object.kind == 'rect'
      or object.kind == 'circle'
      or object.kind == 'ellipse'
    then
      property(self, object, 'fill', '填充', 'checkbox', object.fill == true, function(v)
        object.fill = v
      end)
    end
    if self.editor_mode ~= '' and self.ui.collapsing_header('控制点坐标', false) then
      for i, p in ipairs(self:handles(object)) do
        local label = self:handle_label(object, i)
        if label then
          self.ui.text('控制点 ' .. label)
          for _, axis in ipairs({ 'x', 'y' }) do
            property(
              self,
              object,
              'point' .. i .. axis,
              axis .. '##point' .. i,
              'number',
              p[axis],
              function(v)
                local position = self.copy(self:handles(object)[i])
                position[axis] = v
                self:move_handle(object, i, position)
              end,
              -1e12,
              1e12
            )
          end
        end
      end
    end
    local custom = self.types[object.kind]
    if custom and custom.inspect then
      custom.inspect(self, object)
    end
  end
  function ctx:object_bounds(id)
    local object = self:object(id)
    if not object then
      return self.array()
    end
    local custom = self.types[object.kind]
    if custom and custom.bounds then
      return custom.bounds(self, object)
    end
    if object.kind == 'text' then
      local b = self:text_bounds(object)
      if b then
        return { { x = b.left, y = b.top }, { x = b.right, y = b.bottom } }
      end
    end
    return self:handles(object)
  end
end
return M
