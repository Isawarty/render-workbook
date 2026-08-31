# P07-t07 — 写出 Vulkan ↔ D3D12 概念对照表

## 目标

完成 `docs/vulkan-d3d12-map.md`。每一行不只写对象名字，还要说明所有权、状态/同步责任和一个常见
误区。至少覆盖 instance/factory、adapter、device、queue、command pool/allocator、command buffer/list、
pipeline layout/root signature、descriptor、barrier、fence、swapchain 与软件实现。

## 验收与调试入口

```powershell
python rwb.py test p07-t07
```

通过信号是核心术语契约存在；它只是防止漏项，内容是否准确仍需人工复核。写每行时回到 P1–P5 的
真实代码，回答“谁创建、谁重用、何时必须等待、错误由哪个 validation/debug layer 暴露”。不要把
名称相似当成一对一等价，例如 Vulkan fence 是二值 host wait 对象，而 D3D12 fence 原生带 timeline value。
