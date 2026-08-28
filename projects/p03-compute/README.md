# P3 — Compute Shader 专项

P1/P2 的输出最终都进了光栅化管线。P3 暂时关掉窗口，把 GPU 当并行处理器：输入和输出
都是 buffer，结果同步回读后与 CPU 参考实现逐元素比对。因此这一段没有 golden image，
也没有「看起来差不多」——算错一个元素就是错。

当前已交付前五题：

| task | 内容 | 判分 |
|---|---|---|
| t01 | compute pipeline + storage buffer + dispatch + readback（saxpy） | L1 + L2 float |
| t02 | 多轮并行归约；subgroup 算术与 shared-memory 降级路径 | L1 + L2 float |
| t03 | 两级 exclusive prefix sum | L1 + L2 exact u32 |
| t04 | bitonic sort；非二次幂 padding | L1 + L2 exact u32 |
| t05 | Gaussian blur + tonemap；compute → graphics barrier | L1 + L2 float |

## 代码组织

`ComputeApp.cpp` 是框架：headless Context、VMA buffer、staging 上传和 noexcept 销毁路径。
这些分别是 P1 与 P2-t01 已完成的能力。新实现都在：

```text
src/steps/01_saxpy.cpp
src/steps/02_reduce.cpp
src/steps/03_scan.cpp
shaders/*.comp
```

每题入口互不依赖；后面的题没写完不会拖红前面的题。测试命令统一为：

```bash
python rwb.py test p03-t01
```

做完后按仓库工作流保存自己的实现：`mine/p03-tNN`。参考实现仍在 `done/p03-tNN`，
需要时主动 `git diff done/p03-tNN -- projects/p03-compute`，不会自动显示答案。

## 判分为什么可信

- L1：validation layer + synchronization validation；能抓到的状态与同步违规必须为零。
- L2：`readbackBufferAs<T>` 回读，`BufferAssert` 与 CPU 参考实现逐元素比对。
- 测试长度刻意跨越 workgroup 边界，最后一组不满，真实触发越界保护路径。
- subgroup 能力不足不是 SKIP：shared 路径仍必跑，并断言降级决策与设备能力一致。

注意：sync validation 对 descriptor 背后的 shader 读写 hazard 并不完备，不能单独证明
barrier 正确；L2 会兜住结果错误，任务书同时要求用 RenderDoc/Nsight 检查依赖链。

P3-t01–t05 都用严格 L2 验收，因此没有 L3，也不需要 golden baseline。
