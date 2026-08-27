# P1-t09 — 多帧并行 (Frames in Flight)

## 目标

t07 结束时你的程序是**串行**的：CPU 提交一帧 → 等 fence → 再提交下一帧。
GPU 在 CPU 录制期间闲着，CPU 在 GPU 渲染期间也闲着。

frames in flight 的做法是准备 N 套 per-frame 资源，让 CPU 录制第 n+1 帧的同时，
GPU 还在画第 n 帧。

**为什么 N 通常取 2？**
N 越大吞吐越好，但输入延迟也越大（鼠标操作要等 N 帧才上屏），
而且每多一帧就多一整套资源。2 是绝大多数引擎的默认值。

## 要实现的函数

`src/steps/09_frames_in_flight.cpp`：`setFramesInFlight(uint32_t n)`

这一题不改 `drawFrame` —— 如果你 t07 写对了，`drawFrame` 里的
`m_currentFrame = (m_currentFrame + 1) % m_framesInFlight` 已经天然支持任意 N。
`m_framesInFlight == 1` 时它退化成串行。

## 验收标准

```bash
python rwb.py test p01-t09
```

1. `framesInFlight() == 2`
2. `commandBuffers().size() == 2`
3. 连续渲染 10 帧（远多于 2，逼出帧槽位复用的同步错误）后截图仍与基准一致
4. validation 零报错

**测试的诚实边界**：它验不了「两帧是否真的在并行」——那需要 GPU timeline 分析。
它验的是资源数量正确、复用多轮后画面不变、同步无报错。想看真实的并行度，
用 RenderDoc 或 Nsight Graphics 抓帧。

## 阅读材料

1. **vulkan-tutorial.com** → Drawing → *Frames in flight*。
2. **"Vulkan: frames in flight"** 相关章节，vkguide.dev Chapter 1 的
   *Rendering Loop* 部分讲得更贴近引擎实践。
3. 想理解延迟与吞吐的权衡：**"Latency and Frame Pacing"**
   （NVIDIA Reflex 的白皮书对这个权衡讲得最系统）。

## 本题唯一真正的考点

**`m_renderFinishedSemaphores` 要不要跟着改数量？**

先自己给出答案和理由，再往下看。

<br><br><br><br><br>

答案是**不要**。它是 per-image 的，数量由 swapchain 图像数决定，
和 frames in flight 完全无关。

- `imageAvailable` / `inFlightFence` → 属于**帧槽位** → 数量 = N
- `renderFinished` → 属于**swapchain 图像** → 数量 = 图像数

把这两组混为一谈是 Vulkan 同步里最常见的错误之一，也正是 t07 里
「为什么 renderFinished 按图像数分配」那个问题的延续。

## 常见坑

- **没先 `vkDeviceWaitIdle` 就销毁旧资源**
  你要动的正是 GPU 可能正在用的东西。

- **`vkFreeCommandBuffers` 忘了调**
  直接 `m_commandBuffers.clear()` 只是丢了句柄，pool 里的内存没释放。
  反复调用 `setFramesInFlight` 会泄漏。

- **`m_currentFrame` 没重置为 0**
  改完数量后旧的 index 可能已经越界。

- **新 fence 忘了 SIGNALED**
  和 t07 同一个坑，第一帧直接卡死。

## 做完之后

```bash
git diff done/p01-t09 -- projects/p01-triangle/src/steps/09_frames_in_flight.cpp
```

**P1 到此结束。** 你现在有一份完全自己写过的 Vulkan 初始化代码。
P2 会把它提炼进 `engine/rhi/`，之后所有项目都复用它。
