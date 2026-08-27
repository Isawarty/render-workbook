# P1-t03 — Swapchain / Image Views

## 目标

建立「渲染结果如何交给窗口系统」的整条链路。

OpenGL 的双缓冲由驱动全包，你只会调 `SwapBuffers`。Vulkan 把这件事完全摊开：
几张图、什么格式、什么呈现模式、图像归哪个队列族、什么时候能往里画 —— 全归你管。

## 要实现的函数

`src/steps/03_swapchain.cpp`：`createSwapchain` / `createImageViews`

（`createSurface` 和 `querySwapchainSupport` 已经在 t02 做过了 —— 选设备时就需要它们。）

## 验收标准

```bash
python rwb.py test p01-t03
```

1. `m_swapchain` 非空，`m_swapchainImages` 非空
2. `m_swapchainImageViews.size() == m_swapchainImages.size()`，且每个都非空
3. `m_swapchainExtent` 宽高都 > 0，`m_swapchainFormat != VK_FORMAT_UNDEFINED`
4. validation 零报错

## 阅读材料

1. **vulkan-tutorial.com** → Presentation → *Swap chain*、*Image views* 两节。
2. **Vulkan Spec** §34.5 *WSI Swapchain*。特别是
   `VkSurfaceCapabilitiesKHR::currentExtent` 那段关于 `0xFFFFFFFF` 的说明。
3. 关于 sRGB 与线性空间的关系，读 **"Gamma error in picture scaling"**（John Novak）
   或 Khronos 的 *Data Format Specification* §13。P4 做 PBR 时会依赖这块理解。

## 关键决策，先自己想清楚

**为什么选 `B8G8R8A8_SRGB` 而不是 `_UNORM`？**
sRGB 格式让硬件在写入时自动做 gamma 编码，于是 shader 里可以一直在线性空间做计算。
用 UNORM 的话你要么自己在 shader 末尾做 pow(1/2.2)，要么整条光照链路都是错的。

**为什么 present mode 用 FIFO？**
它是唯一被规范保证一定支持的模式，而且不会空转 GPU。教学期用它最省心，
也最容易得到稳定可比对的截图。MAILBOX 延迟更低但不保证可用。

**为什么 `minImageCount + 1`？**
只要最小数量的话，驱动可能正好拿着全部图像，你 acquire 时只能干等。
多要一张换取真正的并行余量。

## 常见坑

- **用窗口尺寸而不是 framebuffer 尺寸**
  macOS Retina 上两者差 2 倍。用错了画面会只占屏幕四分之一。
  `Window::framebufferSize()` 给的是对的那个。

- **`imageUsage` 漏了 `TRANSFER_SRC_BIT`**
  本仓库的 L3 golden 测试要把 swapchain 图像拷出来存 PNG，缺了这个标志会直接报错。

- **忘了重新查询实际图像数量**
  `minImageCount` 只是你的请求，驱动给的可能更多。必须在
  `vkCreateSwapchainKHR` 之后再调一次 `vkGetSwapchainImagesKHR` 查真实数量。

- **`m_swapchainGeneration` 没自增**
  t08 的测试靠这个计数判断你有没有真的走过重建路径。现在漏了，两题之后才会发现。

- **忘了 clamp extent**
  自选尺寸时必须夹在 `minImageExtent` 和 `maxImageExtent` 之间。

## 做完之后

```bash
git diff done/p01-t03 -- projects/p01-triangle/src/steps/03_swapchain.cpp
```
