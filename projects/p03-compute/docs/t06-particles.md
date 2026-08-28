# P3-t06 — GPU 粒子与跨队列同步

## 最终能力

让 compute queue 更新一组粒子位置，再由 graphics vertex stage 消费同一 buffer；设备有专用
compute queue 时完成 queue-family ownership transfer，没有时走同队列 memory dependency。

非目标：粒子外观、混合和屏幕空间效果。本题只判数据流与同步。

## 要实现

完成 `src/steps/06_particles.cpp` 与两个 particle shader。你负责 command pool/buffer、两次
queue submit、binary semaphore、release/acquire barrier，以及同队列回退。

## 验收标准

运行 `python rwb.py test p03-t06`。通过信号是 L1 validation 零错误、513 个 vec4 的 L2
逐元素一致，且状态摘要证明 graphics submit 等待了 semaphore；是否 ownership transfer
必须与实际 queue-family 索引一致。必做路径不 SKIP。

## 调试入口

- final：`python rwb.py test p03-t06`；看到 `100% tests passed`。
- 开启 validation 与 synchronization validation，关注 queue-family ownership VUID。
- 在两次 `vkQueueSubmit`、release/acquire barrier 处断点，记录 family index 与 wait stage。
- 数值整体未更新通常是 CPU/descriptor 问题；专用队列才报错通常是 Vulkan ownership；只有尾部错误通常是 dispatch 组数。
- 出现 hang、device lost 或 semaphore 未 signal 时抓帧；同队列机器则重点看 submission timeline。

## 分层提示

Hint 1：release 的 `srcQueueFamilyIndex` 是 compute，acquire 两个 family 必须完全相同；同族时都用 `VK_QUEUE_FAMILY_IGNORED`。

Hint 2：compute dispatch → release → signal；wait at vertex stage → acquire → draw → vertex-write 到 transfer-read barrier。

Hint 3：专用队列的 release 使用 shader write，acquire 使用 shader read；binary semaphore 的 wait stage 是 vertex shader。
