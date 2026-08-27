# P1-t04 — Render Pass / Framebuffer

## 目标

声明「这一趟渲染会碰哪些附件、怎么读写、和外界的依赖关系如何」。

render pass 是 Vulkan 里最没有 OpenGL 对应物的概念。它是一份提前声明，
驱动据此决定 tile 划分、layout 转换的时机、能不能省掉一次显存写回。
移动端 tile-based GPU 从中获益最大；桌面端主要换来显式可控的 layout 管理。

## 要实现的函数

`src/steps/04_renderpass.cpp`：`createRenderPass` / `createFramebuffers`

## 验收标准

```bash
python rwb.py test p01-t04
```

1. `m_renderPass` 非空
2. `m_framebuffers.size() == m_swapchainImageViews.size()`，每个都非空
3. validation 零报错（**包括 synchronization validation**）

第 3 条是本题真正的难点。见下面「subpass dependency」。

## 阅读材料

1. **vulkan-tutorial.com** → Graphics pipeline basics → *Render passes*，
   以及 Drawing → *Framebuffers*。
2. **Vulkan Spec** §8 *Render Pass*。重点读 §8.2 *Render Pass Creation* 里
   `VkSubpassDependency` 的语义，这是 P1 最难啃但最值钱的一段规范。
3. **"Yet another blog explaining Vulkan synchronization"**（Maister / Hans-Kristian
   Arntzen）。整篇都值得读，本题重点是 external subpass dependency 那节。
4. 想知道 render pass 在移动端为什么重要：ARM 的
   **"Vulkan Render Passes"** 开发者文档。

## subpass dependency：本题的核心

`vkAcquireNextImageKHR` 用 semaphore 通知「图像可用了」。但 semaphore 只保证
「等到了」，不保证 render pass 隐式的 `UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL`
布局转换发生在正确的时刻 —— 它可能早于图像真正可用。

解法是声明一条 `VK_SUBPASS_EXTERNAL -> subpass 0` 的依赖，把转换钉在
`COLOR_ATTACHMENT_OUTPUT` 阶段之后。

**先自己想清楚**：这个 stage 和你 t07 里提交时用的 `waitDstStageMask` 是什么关系？
两者必须一致，否则依赖链是断的。

漏掉这条依赖时，画面通常看起来是对的 —— 所以只靠肉眼永远发现不了。
但 sync validation 会报出来，而本仓库的 L1 判分把 warning 也算失败。

## 常见坑

- **`initialLayout` 用了 `PRESENT_SRC_KHR`**
  第一帧时图像还从没被 present 过，它的布局是 `UNDEFINED`。
  配合 `loadOp = CLEAR` 用 `UNDEFINED` 才是对的 —— 反正要全部覆盖。

- **`loadOp` 用 `LOAD`**
  那等于告诉驱动「旧内容我要」，tile-based GPU 会因此多做一次从显存读回。
  你要清屏，就用 `CLEAR`。

- **clear value 数量和 attachment 对不上**
  `pClearValues` 的顺序必须和 `pAttachments` 一一对应（t06 里会用到）。

## 做完之后

```bash
git diff done/p01-t04 -- projects/p01-triangle/src/steps/04_renderpass.cpp
```
