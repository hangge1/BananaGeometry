return {
    id="drawer.bezier", name="贝塞尔曲线 / Bezier", data_version=1, default_mode="draw",
    on_init=function(ctx) ctx.state.params={width=3,samples=24} end,
    on_ui=function(ctx)
        ctx.ui.text("三次贝塞尔曲线")
        if ctx.ui.button("绘制曲线") then ctx:set_mode("draw") end
        ctx.ui.same_line()
        if ctx.ui.button("调整控制点") then ctx:set_mode("edit") end
        ctx.ui.separator()
        ctx.ui.text("点击四个控制点创建曲线；可连续创建。")
        ctx:parameter("线宽","width","slider",1,8)
        ctx:parameter("采样点数","samples","number",2,100)
        ctx.ui.text("调整模式显示控制多边形和采样点。")
    end,
    on_event=function(ctx,event)
        if ctx.mode=="edit" then ctx:edit_event(event)
        else ctx:draw_event("bezier",4,event,{color={0.65,0.5,1,1},width=ctx.state.params.width}) end
    end,
    on_draw=function(ctx)
        ctx:render()
        local selected=ctx:object(ctx.temp.selected)
        if selected and selected.kind=="bezier" then
            local count=math.floor(ctx.state.params.samples)
            for i=0,count do ctx:point(ctx.math.bezier(selected.points,i/count),{0.3,0.9,0.8,1},2) end
        end
    end,
}
