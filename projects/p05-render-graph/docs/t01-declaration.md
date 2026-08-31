# P5-t01 — 声明式 pass / resource API

## 目标与实现

在 `engine/include/rwb/rendergraph/RenderGraph.h` 和 `engine/src/rendergraph/RenderGraph.cpp` 中建立
`ResourceHandle`、`PassHandle`、`ResourceDesc`、`ResourceState`、`PassBuilder` 与执行回调。资源分
transient/imported；pass 用 `read/write/readWrite` 声明访问，不允许同一 pass 重复声明同一资源。

## 验收与调试入口

```bash
python3 rwb.py test p05-t01
```

通过信号：两个 pass 按契约执行，回调顺序为 `Lighting, Tonemap`。失败先检查 handle 越界、资源名、
重复 use 与 callback 是否被保存；这是纯 CPU 题，不应启动 Vulkan 设备或出现 SKIP。

关键不变量：声明不立即执行；handle 是稳定索引；imported resource 不参与 transient 复用；空名称和
无效 handle 必须明确报错。
