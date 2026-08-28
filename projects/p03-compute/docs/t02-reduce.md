# P3-t02 — 并行归约与 subgroup 降级

## 目标

把 N 个 float 归约成一个和：每个 workgroup 先产出局部和，host 再多轮 dispatch，直到只剩
一个元素。同时理解 subgroup 是设备能力，不是跨平台保证。非目标：强迫所有设备执行 subgroup；
MoltenVK/lavapipe 能力不足时必须走 shared-memory 路径，但不能跳过整题。

## 要实现

- `src/steps/02_reduce.cpp`：能力决策 `choosePath`、真实设备选择与多轮 `runReduce`。
- `shaders/reduce_shared.comp`：共享内存树形归约。
- `shaders/reduce_subgroup.comp`：`subgroupAdd` + workgroup 内二级汇总。

## 验收标准

```bash
python rwb.py test p03-t02
```

通过信号：纯函数能力表四种组合正确；shared 路径始终与 CPU 求和一致；设备支持时 subgroup
路径也一致，不支持时明确断言降级条件；全程 validation 零 error/warning，无 SKIP。

## 调试入口

- final 命令同上；通过时两个 `t02` Catch2 case 都绿。
- 日志打印设备名、subgroupSize 与最终路径；sync validation 必开，但不能假设它覆盖所有
  descriptor-backed shader hazard。
- 推荐断点：每轮 dispatch 后，观察 `count → groups` 以及 ping/pong 交换。
- 第一轮就错通常是 shader 局部归约；只在 N>256 时错通常是多轮 barrier/交换；只有 subgroup
  路径错则检查 `gl_NumSubgroups` 与共享数组汇总。
- subgroup shader 不应在不支持的设备上创建；先核对能力位与路径选择。完成后抓一次
  RenderDoc/Nsight，确认每轮 dispatch 之间存在依赖。

## 关键不变量

1. 越界线程写加法单位元 0，但仍必须参与 workgroup barrier，不能提前 return。
2. 每轮输出元素数是 `ceil(count/256)`，下一轮读上一轮输出。
3. shader write → 下一轮 shader read 之间必须有 barrier；末轮则通向 transfer read。

## 分层提示

<details><summary>Hint 1</summary>
查 `VkPhysicalDeviceSubgroupProperties::supportedOperations/supportedStages/subgroupSize`。
</details>

<details><summary>Hint 2</summary>
shared 路径：256 个数 → 每组 1 个；重复同一 pipeline 并 ping-pong 两块 buffer。
</details>

<details><summary>Hint 3</summary>
subgroup 可用条件是 compute stage + arithmetic bit + 能容纳 `256/subgroupSize` 个局部和。
末轮 barrier 的 dst access/stage 应从 shader read 切换到 transfer read。
</details>
