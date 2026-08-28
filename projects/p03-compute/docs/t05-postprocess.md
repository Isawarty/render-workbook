# P3-t05 — 图像后处理与 compute → graphics barrier

## 目标

对线性 RGBA 图像执行 3×3 Gaussian blur 和 Reinhard tonemap，再让 fullscreen fragment pass
逐像素消费 compute 输出。图像以 storage buffer 表示，刻意避开 P2 已学过的 image/view/sampler
样板，把注意力放在跨 pipeline stage 的可见性上。

## 要实现

- `src/steps/05_postprocess.cpp`：compute/graphics 两条 pipeline、无颜色附件 render pass、
  descriptor、dispatch→draw barrier 与回读。
- `shaders/postprocess.comp`：边缘 clamp、Gaussian、tonemap。
- `postprocess.vert/frag`：fullscreen triangle 与逐像素 graphics 消费。

## 验收标准

```bash
python rwb.py test p03-t05
```

17×11 HDR 图（两个方向都不是 local size 8 的倍数）必须与 CPU 参考实现逐元素近似一致；
barrier 快照必须是 compute shader write → fragment shader read；validation 零 error/warning。

## 调试入口

- final 命令同上；L2 首 8 个差异能区分边缘、tonemap 和整帧未消费。
- 推荐断点：dispatch 后 barrier，以及 `vkCmdBeginRenderPass` 前；观察两个 descriptor set。
- 边缘像素错是 clamp；所有亮度偏差是 kernel/除数或 tonemap；输出全零通常是 graphics pass
  没画、viewport/scissor 错或 barrier/descriptor 错。
- 这题值得抓帧：确认 command stream 顺序为 dispatch → barrier → render pass → draw。

## 关键不变量

1. compute 输出的 shader write 对 fragment shader read 可见。
2. fragment pass 必须真的消费结果；最终回读的是 graphics 写出的第三块 buffer。
3. `fragmentStoresAndAtomics` 必须在创建设备时显式开启。

## 分层提示

<details><summary>Hint 1</summary>Gaussian 核是 `[1 2 1]ᵀ[1 2 1] / 16`，边缘坐标 clamp。</details>
<details><summary>Hint 2</summary>数据流：input → compute filtered → barrier → fragment consumed → readback。</details>
<details><summary>Hint 3</summary>barrier：COMPUTE_SHADER/SHADER_WRITE → FRAGMENT_SHADER/SHADER_READ。</details>
