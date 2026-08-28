# P3-t01 — Compute 最小闭环：saxpy

## 目标

独立建起一条 compute 数据通路：storage buffer → descriptor → compute pipeline → dispatch →
barrier → GPU 回读。完成后你应能解释「发了多少 workgroup」「shader 能访问哪些资源」以及
「为什么算完不等于后续立刻可读」。非目标：重新练 VMA/staging；它们已由框架提供。

## 要实现

- `src/steps/01_saxpy.cpp`：`descriptorPool`、`createComputePipeline`、
  `allocateStorageSet`、`runSaxpy`。
- `shaders/saxpy.comp` 已给出；读懂 binding、push constant 和尾组边界判断。
- `destroyComputePipeline` 与所有 buffer 销毁都是 noexcept 路径，直接给出，不挖空。

## 验收标准

```bash
python rwb.py test p03-t01
```

通过信号：CTest 报 `p03-t01 Passed`，并同时满足：headless Context 无窗口但有 compute
能力；65537 个元素全部符合 `y = 1.75*x + y`；validation error/warning 均为 0。

## 调试入口

- final 命令就是上面的 `python rwb.py test p03-t01`；不要直接跑 exe 绕过 CTest 环境。
- P3 自动开启 validation 与 synchronization validation；先读失败输出中的
  `ValidationLog` 摘要，再看 L2 的首 8 个错误元素。
- 推荐断点：`runSaxpy` 的 `vkCmdDispatch` 前；观察 `n`、`groups`、descriptor bindings。
- 大片元素不对多半是 descriptor/pipeline 状态；只错末尾多半是向上取整或 shader 边界；
  L1 红时再按消息检查 barrier 的 stage/access。
- validation 对 shader descriptor hazard 并不完备；数值虽绿也应抓一次 RenderDoc/Nsight，
  确认 dispatch 后确实记录了预期 barrier。

## 关键不变量

1. descriptor binding 必须与 shader 的 `set=0,binding=0/1` 对齐。
2. workgroup 数必须向上取整；测试长度故意不是 256 的倍数。
3. shader write 到 transfer read 之间需要 execution + memory dependency。

## 分层提示

<details><summary>Hint 1</summary>
查 Vulkan 规范的 compute pipeline、storage buffer descriptor 和 `vkCmdPipelineBarrier`；
关注 `VK_ACCESS_SHADER_WRITE_BIT` 与 `VK_ACCESS_TRANSFER_READ_BIT`。
</details>

<details><summary>Hint 2</summary>
数据流是 CPU → staging copy → X/Y storage buffers → dispatch 改写 Y → barrier → readback copy。
</details>

<details><summary>Hint 3</summary>
pipeline 只有一个 compute stage；pipeline layout 包含一个 set layout 和一个 push range。
dispatch 组数是 `(n + 255) / 256`。
</details>

完成后可看：

```bash
git diff done/p03-t01 -- projects/p03-compute/src/steps/01_saxpy.cpp
```
