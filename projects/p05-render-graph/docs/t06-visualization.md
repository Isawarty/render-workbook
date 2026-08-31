# P5-t06 — Graphviz 与 ImGui 图面板

## 目标与实现

`CompiledGraph::toDot()` 导出 pass/resource 节点和 R/W 边；`p05_render_graph --dot FILE` 写盘。
窗口内按 `F1` 打开 ImGui 面板，查看 pass 流、依赖、barrier 数、lifetime 与 alias slot。面板绘制在
P4 tonemap render pass 末尾，使用 load 后的最终画面，不另起清屏 pass。

## 验收与调试入口

```bash
python3 rwb.py test p05-t06
python3 rwb.py run p05
```

自动测试要求 DOT 包含五个 pass、关键资源和 R/W 边；人工验收按 `F1` 后面板出现、鼠标释放、相机
停止移动，`F1`/`Esc` 关闭后重新捕获并恢复漫游。resize 后 ImGui backend 必须随新的 post render pass
重建。离屏测试不初始化 UI，因此最终图对照不被字体或面板污染。

常见失败：DOT 缺边是声明层问题；面板空白看 `ImGui::Render` 是否位于 tonemap render pass 内；
validation 红看 ImGui pipeline 的 render-pass compatibility；键鼠冲突看 `uiInteractionEnabled`，不要
同时让 GLFW camera polling 和 ImGui 消费同一帧输入。
