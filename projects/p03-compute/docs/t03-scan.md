# P3-t03 — 两级 exclusive prefix sum

## 目标

实现 exclusive scan：`out[0]=0`，`out[i]=sum(input[0..i-1])`。重点不是背某个最优算法，
而是把 workgroup 内共享内存算法扩展到多个 block，并正确连接三次 dispatch。
本题采用 Hillis–Steele，非目标是 work-efficient Blelloch 优化（扩展练习）。

## 要实现

- `src/steps/03_scan.cpp`：资源组织、三趟 dispatch 与两类 barrier。
- `shaders/scan_block.comp`：分块 scan，同时输出 block sum。
- `shaders/scan_add_offsets.comp`：把每块前缀偏移加回最终结果。

支持 0–65536 个 `uint32_t`；更大规模、多级递归 scan 是扩展练习。

## 验收标准

```bash
python rwb.py test p03-t03
```

通过信号：空输入、257（跨块且尾块不满）、65536（两级上限）都与 CPU 参考实现逐元素
完全相等，validation 零 error/warning。

## 调试入口

- final 命令同上；L2 是整数 exact compare，失败会列首 8 个错误索引。
- sync validation 默认开启；先区分 shader 算法错误与 dispatch 间依赖错误。
- 推荐断点/观测点：第一趟后的 `blockSums`、第二趟后的 `blockOffsets`、第三趟前的 blocks。
- 块内就错是 shared-memory barrier/索引；从索引 256 开始整体偏移错是二级 scan；只有尾块错
  是越界补零或 dispatch 向上取整。
- 这题通常不需要抓图形帧；必要时用 RenderDoc 看 storage buffer 的三次 dispatch 快照。

## 关键不变量

1. shared-memory 每轮必须先让所有线程读完旧值，再允许写新值。
2. 第一趟输出块内 exclusive 结果和块总和；第二趟 scan 块总和；第三趟加回偏移。
3. 最终 shader write 到回读 transfer read 之间仍需 barrier。

## 分层提示

<details><summary>Hint 1</summary>
先在纸上算 8 个元素的 Hillis–Steele inclusive scan，再思考怎么变 exclusive。
</details>

<details><summary>Hint 2</summary>
三趟数据流：`input → output+blockSums`，`blockSums → blockOffsets`，
`output+blockOffsets → output`。
</details>

<details><summary>Hint 3</summary>
`scan_block.comp` 被前两趟复用；第二趟只发一个 workgroup，因为 block 数最多 256。
最后一个 local invocation 的 inclusive 值就是该块总和。
</details>
