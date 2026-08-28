# P3-t07 — Indirect dispatch

## 最终能力

由第一条 compute shader 在 GPU 上写出 `VkDispatchIndirectCommand`，通过正确的 indirect-command
依赖后调用 `vkCmdDispatchIndirect`，让第二条 shader 处理任意长度 buffer。

非目标：multi-draw indirect、draw count 与 GPU culling，留到 GPU-driven 扩展方向。

## 要实现

完成 `src/steps/07_indirect.cpp`、command 生成 shader 和 scale shader。command buffer 必须同时有
storage、indirect 与 transfer-src usage；不能由 CPU 代算 group count 后冒充 indirect。

## 验收标准

运行 `python rwb.py test p03-t07`。通过信号是 65,537 个 float 的 L2 结果完全符合 CPU 参考，
回读 command 为 `{257,1,1}`，且 L1 validation 零错误。必做路径不 SKIP。

## 调试入口

- final：`python rwb.py test p03-t07`；看到 `100% tests passed`。
- 开启 validation 与 synchronization validation，观察 `DRAW_INDIRECT` stage 的 hazard。
- 在生成 command 后的 barrier 和 `vkCmdDispatchIndirect` 处断点；回读 command 是最窄状态摘要。
- command 数值错误属于 shader/CPU 契约；VUID 属于 Vulkan usage 或 barrier；尾部错误属于 bounds check。
- command 正确但无输出时抓帧检查 indirect dispatch；hang/device lost 时先核对 command 上限。

## 分层提示

Hint 1：indirect command 的生产者是 compute shader write，消费者是 indirect command read。

Hint 2：prepare dispatch → `COMPUTE_SHADER/SHADER_WRITE` 到 `DRAW_INDIRECT/INDIRECT_COMMAND_READ` → indirect dispatch → readback barrier。

Hint 3：`groupCountX = (n + 255) / 256`；scale shader 仍需 `i >= n` 的尾部保护。
