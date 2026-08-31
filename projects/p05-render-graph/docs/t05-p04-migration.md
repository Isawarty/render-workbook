# P5-t05 — 用 Render Graph 驱动 P4

## 目标与实现

把 P4 的一帧拆成 `Shadow → Geometry → Lighting → Bloom → Tonemap`。P4 仍拥有资源与交互；
P5 每帧用当前 extent、实际 HDR/Bloom image 和相机矩阵构图，编译后依拓扑执行。Geometry/Lighting
保持原生 subpass，图自动提交 HDR→Bloom、Bloom→Tonemap 两处 Vulkan barrier。

## 验收与调试入口

```bash
python3 rwb.py test p05-t05
```

通过信号：五个 pass 名称和 barrier 快照成立；沿用 P4-t07 的隐藏窗口首帧 warm-up 后，软件渲染器上图路径与 P4 直接路径严格比对，真机按
仓库既有的 16×16 结构容差比对；validation error/warning 为 0。macOS 测试会创建隐藏窗口；受限终端若卡在 Cocoa/LaunchServices，
应在正常桌面终端复验，不能把宿主 GUI 问题误修成同步改动。

输出不同先比较 pass 顺序和 camera state；L1 红先看 graph barrier；只在资源回读正确、最终色彩异常时
再看 PBR/tonemap。最终窗口必须仍包含头盔、地面、天空、阴影和 bloom，且 P4 全套镜头操作可用。
