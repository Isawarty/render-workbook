# P1-t08 — Swapchain 重建

## 目标

窗口一改变大小，swapchain 里的图像尺寸就对不上了，驱动开始返回
`VK_ERROR_OUT_OF_DATE_KHR`。OpenGL 里这件事驱动全包，Vulkan 里你要自己把
整条依赖链重建一遍。

**依赖 swapchain 的**：image views → framebuffers，以及按图像数分配的
`renderFinished` semaphore。

**不依赖的**：render pass（只依赖格式）、pipeline（视口是动态状态）、command pool。

后一组不用重建，正是 t05 把 viewport/scissor 设成动态状态换来的回报。

## 要实现的函数

`src/steps/08_resize.cpp`：`cleanupSwapchain` / `recreateSwapchain`

`drawFrame` 里调用它们的分支你在 t07 已经写好了。

## 验收标准

```bash
python rwb.py test p01-t08
```

1. 渲染若干帧 → `requestResize()` → 再渲染，全程不抛异常
2. `swapchainGeneration()` 必须增加 —— 说明真的走过了重建路径
3. 重建之后还能继续正常渲染
4. validation 零报错

第 2 条依赖你在 t03 的 `createSwapchain()` 里让 `m_swapchainGeneration` 自增。
如果那时漏了，这里会以一种看起来很莫名的方式失败。

## 阅读材料

1. **vulkan-tutorial.com** → Drawing → *Swap chain recreation*。
2. **Vulkan Spec** §34.5.4 关于 `VK_ERROR_OUT_OF_DATE_KHR` 与
   `VK_SUBOPTIMAL_KHR` 的语义差别。
3. `oldSwapchain` 字段的正确用法（本题的参考实现没用它，但生产代码通常会用）：
   Spec §34.5.1 `VkSwapchainCreateInfoKHR::oldSwapchain`。
   传入旧 swapchain 可以让驱动复用资源，减少重建时的卡顿。
   **这是一个值得你在做完之后自己加上去的扩展练习。**

## 关键决策

**为什么必须 `vkDeviceWaitIdle` 而不是等某个 fence？**
任何还在飞的命令都可能正在引用即将被销毁的 framebuffer 或 image view。
你手上的 fence 只覆盖了当前帧槽位，覆盖不了 present 引擎持有的引用。
这是少数几个「粗暴地整设备等待」才是正确做法的地方。

**最小化时为什么要死循环轮询？**
最小化时 framebuffer 是 0x0，这个尺寸建不出 swapchain。
阻塞等待用户恢复窗口是正确的做法 —— 反正也没什么可渲染的。

## 常见坑

- **忘了重建 `renderFinished` semaphore**
  swapchain 图像数量可能变化（换显示器、改刷新率、切 present 模式都可能触发）。
  semaphore 数量和图像数绑定，必须跟着重建。漏了之后是间歇性的越界。

- **`cleanupSwapchain` 在「什么都没建过」时崩溃**
  它也会被析构路径调用。必须能安全地处理空容器和 `VK_NULL_HANDLE`。

- **销毁了 `m_swapchainImages` 里的 `VkImage`**
  那些图像归 swapchain 所有，`vkDestroySwapchainKHR` 会一并回收。
  自己再销毁一次是 double free。

- **重建之后忘了清 `m_framebufferResized`**
  会导致每帧都重建，帧率暴跌但画面正常 —— 很难察觉。

## 做完之后

```bash
git diff done/p01-t08 -- projects/p01-triangle/src/steps/08_resize.cpp
```

**扩展练习**（不判分）：把 `oldSwapchain` 用起来，观察重建时的卡顿变化。
