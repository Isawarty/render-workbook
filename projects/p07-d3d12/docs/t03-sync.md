# P07-t03 — Resource Barrier 与 Fence Timeline

## 目标

显式记录 back buffer 的 `PRESENT → RENDER_TARGET → PRESENT` transition，并以单调递增的
`ID3D12Fence` value 管理 allocator 重用。这里的 fence 是时间线数值，不是“每帧创建一个事件”。

## 验收与调试入口

```powershell
python rwb.py test p07-t03
```

测试连续提交三帧，检查 transition 摘要、completed value 与 info queue。hang 时记录每次 queue signal、
等待值和 completed value；`COMMAND_ALLOCATOR_SYNC` 报错表示 GPU 未完成就 reset allocator；画面正常但
state mismatch 仍算失败。PIX 的 Resource History 应显示完整的双向 transition。

<details><summary>Hint 1</summary>每个 back buffer 保存它最后一次提交的 fence value；开始复用该 frame slot 时
只等待这个值。</details>
