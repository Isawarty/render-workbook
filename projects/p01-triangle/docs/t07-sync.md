# P1-t07 — Semaphore / Fence / 提交 / 呈现

**这是 P1 最难、也最值钱的一题。** 做完它你会真正理解为什么说
「Vulkan 把同步交给了你」。

## 目标

把一帧的因果链完整串起来，让三角形出现在屏幕上。

```
acquire 拿到图像索引 --(imageAvailable semaphore)--> GPU 开始画
GPU 画完             --(renderFinished semaphore)--> present 引擎开始显示
GPU 画完             --(inFlight fence)-----------> CPU 知道资源可以复用了
```

两种同步原语的分工：

| | 谁等谁 | CPU 能看到状态吗 |
|---|---|---|
| `VkSemaphore` | GPU 队列之间 | 看不到 |
| `VkFence` | GPU 通知 CPU | 能，`vkWaitForFences` |

## 要实现的函数

`src/steps/07_sync.cpp`：`createSyncObjects` / `drawFrame`

## 验收标准

```bash
python rwb.py test p01-t07
```

1. 连续渲染 5 帧不抛异常
2. 截图与基准图一致（L3；基准图未入库时该条会 SKIP，不算失败）
3. validation 零报错，**包括 synchronization validation**

## 阅读材料

按顺序，这一题的阅读量最大也最值得：

1. **vulkan-tutorial.com** → Drawing → *Rendering and presentation*、
   *Frames in flight*。先照着做通。
2. **"Yet another blog explaining Vulkan synchronization"**（Hans-Kristian Arntzen）。
   **整篇精读。** 这是中文互联网上找不到等价物的一篇，讲清了 stage mask、
   access mask、execution dependency 和 memory dependency 的区别。
3. **Khronos 官方 "Vulkan Synchronization Examples"**（GitHub: KhronosGroup/Vulkan-Docs
   wiki 的 Synchronization Examples 页）。当模板查。
4. **Vulkan Spec** §7 *Synchronization and Cache Control*。在你对某个具体
   validation 报错不理解时查。

## 三个必须自己想清楚的问题

**1. `renderFinished` semaphore 该按帧数分配还是按 swapchain 图像数分配？**

`vkAcquireNextImageKHR` 返回的 `imageIndex` 顺序由驱动决定，不保证轮转。
如果按帧数分配，可能出现「present 还在等图像 A 上的 semaphore，
而新一帧已经把同一个 semaphore 拿去 signal 图像 B」的别名冲突。

很多教程（包括 vulkan-tutorial 的旧版本）就是错的。开了 sync validation 会被抓出来。

**2. `vkResetFences` 该放在 acquire 之前还是之后？**

如果 acquire 返回 `VK_ERROR_OUT_OF_DATE_KHR`，你会提前 `return`。
此时若 fence 已经被重置，它就永远不会被 signal —— 下一帧 `vkWaitForFences` 死锁。

所以：**只有确认要提交工作了才重置**。

**3. `waitDstStageMask` 为什么用 `COLOR_ATTACHMENT_OUTPUT` 而不是 `TOP_OF_PIPE`？**

它说的是「GPU 可以先跑到哪一步，才必须停下来等 semaphore」。
顶点处理不碰颜色附件，可以先跑；到写颜色输出这一步才必须等图像就绪。
用 `TOP_OF_PIPE` 是对的但更保守，白白浪费了重叠执行的机会。

另外：这个 stage 必须和 t04 里 subpass dependency 的 stage 一致，否则依赖链是断的。

## 常见坑

- **fence 没有初始 SIGNALED**
  第一帧的 `vkWaitForFences` 会在等一个从没被提交过的 fence，永久阻塞。
  创建时要带 `VK_FENCE_CREATE_SIGNALED_BIT`。

- **`vkAcquireNextImageKHR` 的返回值没处理全**
  `VK_SUBOPTIMAL_KHR` 是「还能用但不理想」，不该当失败；
  `VK_ERROR_OUT_OF_DATE_KHR` 必须重建并放弃这一帧。

- **present 之后没检查 `m_framebufferResized`**
  有些平台上 resize 后驱动不返回 OUT_OF_DATE，得靠窗口回调的标志兜底。

## 做完之后

```bash
git diff done/p01-t07 -- projects/p01-triangle/src/steps/07_sync.cpp
```
