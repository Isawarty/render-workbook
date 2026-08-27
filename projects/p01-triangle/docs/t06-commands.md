# P1-t06 — Command Pool / Command Buffer / 录制

## 目标

把一帧的绘制命令录进 command buffer。

OpenGL 的 draw call 看起来是「立即」的。Vulkan 里你先录制，之后再整体提交给队列。

- 录制可以多线程并行，提交是单线程的廉价操作
- 录制期间驱动不做任何状态校验（都在建 pipeline 时做完了）

## 要实现的函数

`src/steps/06_commands.cpp`：`createCommandPool` / `createCommandBuffers` /
`recordCommandBuffer`

## 验收标准

```bash
python rwb.py test p01-t06
```

1. `m_commandPool` 非空
2. `m_commandBuffers.size() == m_framesInFlight`（现在是 1，t09 会变成 2）
3. validation 零报错

注意这一题的测试只检查资源建出来了，**没有真的提交过任何命令** ——
录制对不对要到 t07 才会被验证。

## 阅读材料

1. **vulkan-tutorial.com** → Drawing → *Command buffers*。
2. **Vulkan Spec** §6 *Command Buffers*，重点看 §6.1 的生命周期状态机
   （Initial / Recording / Executable / Pending / Invalid）。这张状态图能解释
   后面几乎所有「command buffer 用法错误」的 validation 报错。
3. 多线程录制的实际做法，看 **Sascha Willems 的 multithreading 示例**
   （secondary command buffer 的用法）。P1 用不上，但知道它存在有好处。

## 关键决策

**为什么 pool 要带 `RESET_COMMAND_BUFFER_BIT`？**
不带的话只能整池 `vkResetCommandPool`。我们每帧都要单独重置并重录一个
command buffer，所以需要这个标志。

代价是驱动内部要为每个 buffer 单独管理内存池，略慢。真实引擎里更常见的做法是
**每帧一个 pool，整池重置** —— 那样更快。这里用 per-buffer reset 是因为它更直白。

**PRIMARY vs SECONDARY**
PRIMARY 能直接提交给队列；SECONDARY 只能被 PRIMARY 通过 `vkCmdExecuteCommands`
调用，是多线程录制的载体。P1 只需要 PRIMARY。

## 常见坑

- **录制时漏了 `vkCmdSetViewport` / `vkCmdSetScissor`**
  t05 把它们声明成了动态状态，不设就是错。validation 会报
  "dynamic state not set"。

- **`vkCmdDraw` 的参数搞混**
  签名是 `(cb, vertexCount, instanceCount, firstVertex, firstInstance)`。
  我们要 `(cb, 3, 1, 0, 0)`。`instanceCount` 传 0 会什么都不画。

- **在 render pass 外面 bind pipeline**
  graphics pipeline 必须在 `vkCmdBeginRenderPass` 之后绑定。

- **command pool 绑错队列族**
  从某个 pool 分配的 buffer 只能提交给对应队列族的队列。

## 做完之后

```bash
git diff done/p01-t06 -- projects/p01-triangle/src/steps/06_commands.cpp
```
