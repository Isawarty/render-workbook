# P07-t05 — Compute Pipeline 与 SAXPY Readback

## 目标

把 P3-t01 的 `a*x+y` 搬到 D3D12：创建 SRV/UAV/constant buffer 描述符、compute root signature 与
PSO，dispatch 后通过 UAV/transition barrier 把结果复制到 readback heap。算法相同，显式资源模型不同。

## 验收与调试入口

```powershell
python rwb.py test p07-t05
```

通过信号是 7 个 float 与 CPU 参考逐元素一致、compute PSO 有效、info queue 干净。全零先查 dispatch
维度和 UAV 描述符；旧值或间歇错误先查 UAV barrier 与 copy 前 transition；map/readback 数值错再查
buffer byte size。用 PIX 看 Compute Dispatch 的 root bindings，而不是从最终数组反猜所有状态。

<details><summary>Hint 1</summary>线程组数用 `(count + 63) / 64`，shader 内仍需 `index < count`。</details>
