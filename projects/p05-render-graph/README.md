# P5 — Render Graph

P5 把 P4 手写在 `recordFrame` 中的固定执行序列提升为可编译的有向图。Pass 只声明读写什么、
需要什么 Vulkan 状态和自己的录制回调；编译器负责 hazard 边、稳定拓扑序、pass 间 image barrier、
资源生命周期与 transient 物理槽复用计划。

| task | 能力 | 判分 |
|---|---|---|
| t01 | 声明式 pass/resource API 与回调执行 | 纯 CPU 契约 |
| t02 | RAW/WAR/WAW 依赖、稳定拓扑排序、环检测 | 纯 CPU 图算法 |
| t03 | stage/access/layout 状态机与 Vulkan image barrier | CPU 精确断言 + t05 L1 |
| t04 | first/last use、兼容 alias slot 与真实 VMA allocation 复用 | CPU 精确断言 + L1 |
| t05 | Shadow → Geometry → Lighting → Bloom → Tonemap 驱动 P4 | L1 + 与 P4 同帧精确对照 |
| t06 | DOT 导出与 ImGui 实时面板 | 结构断言 + 可交互窗口 |

## 运行

```bash
python3 rwb.py test p05
python3 rwb.py run p05
python3 rwb.py run p05 --frames 120
build/mac-arm64/projects/p05-render-graph/p05_render_graph --dot /tmp/p05.dot --frames 1
dot -Tpng /tmp/p05.dot -o /tmp/p05.png
```

P05 复用 P4 已验证的 Sci-Fi Helmet、PBR 地面、程序化天空、固定光源阴影、Bloom/ACES、Slang
shader 与 swapchain resize。它也保留 P4 的完整镜头手感：鼠标观察，`W/S` 前后、`A/D` 平移、
`Q/E` 升降、`Shift` 三倍加速、`R` 复位；`Esc` 先释放鼠标，再按关闭窗口，左键重新捕获。

按 `F1` 打开 Render Graph 面板。打开时自动释放鼠标并暂停相机输入；`F1` 或 `Esc` 关闭面板后
恢复鼠标捕获。面板展示五个 pass、依赖、推导 barrier 数和 transient lifetime/alias slot，覆盖在
最终 tonemap render pass 中，不另清屏，也不替换 P4 的丰富场景。离屏判分不初始化 ImGui，避免
UI 状态污染确定性输出。

P05 依赖 P4 的最终 Slang 管线，因此需要 `slangc`。Dear ImGui 由 CMake 按固定 commit 和 SHA256
下载；Graphviz 只用于把 `--dot` 文件转成图片，不是编译或判分依赖。

## 关键边界

- Render Graph 只拥有调度元数据；P4 继续拥有 Vulkan image、pipeline、descriptor、场景和 resize。
- G-buffer 的 Geometry/Lighting 是同一个原生 render pass 的两个 subpass，其内部同步仍由 subpass
  dependency 管理；HDR→Bloom 与 Bloom→Tonemap 是图真正发出的 Vulkan image barrier。
- `compatibilityKey` 表示可共享物理槽的创建兼容类；生命周期不重叠只是必要条件，不绕过格式、
  usage、alignment 等兼容约束。
- P4 的直接录制路径和 P5 图路径调用同一组 pass 函数；端到端在软件渲染器严格比较、真机按
  16×16 块结构比较，避免把纹理过滤/首帧驱动差异误判成调度错误。
