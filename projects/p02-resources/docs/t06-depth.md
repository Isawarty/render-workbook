# P2-t06 — 深度缓冲 / 深度测试

## 目标

到 t05 为止，画面里只有一块平铺的地面，谁盖住谁完全由绘制顺序决定 —— 后画的赢。
从这一题起场景里多了一个立方体，这条规则立刻崩掉：立方体的背面在索引缓冲里排在
正面之后，于是**背面把正面盖住了**。

这门课的 pipeline 刻意关掉了背面剔除（`raster.cullMode = VK_CULL_MODE_NONE`），
就是为了让这个现象无处可藏。剔除只是省掉一半三角形，它解决不了两个物体互相穿插、
也解决不了凹形物体自遮挡。真正把「谁在前面」从绘制顺序里解耦出来的，是深度缓冲。

OpenGL 里这件事是 `glEnable(GL_DEPTH_TEST)` 一行，深度缓冲由上下文自带。
Vulkan 里深度附件就是一张普通的 image：你自己挑格式、自己建、自己建 view、
自己保证它和颜色附件同尺寸同采样数，然后在 render pass、framebuffer、pipeline
三处把它接上。

后三处框架已经写好了（`createRenderPass` / `createFramebuffers` /
`createGraphicsPipeline`），但它们只是读你产出的 `m_depth`。开工前先去看一眼
这三处怎么用它 —— 你要建成什么样，是被它们决定的。

## 要实现的函数

`src/steps/06_depth.cpp`：`findDepthFormat` / `createDepthResources`

`createImage` / `createImageView` / `transitionImageLayout` 是你在 t04、t05
已经写过的，这里直接复用。

## 验收标准

```bash
python rwb.py test p02-t06
```

1. `depthImage().valid()`，且 `depthImage().view != VK_NULL_HANDLE`
2. 深度图的 `width` / `height` 和 `swapchain().extent()` 完全相等
3. 挑出来的格式必须是三个深度格式之一：`VK_FORMAT_D32_SFLOAT` /
   `VK_FORMAT_D32_SFLOAT_S8_UINT` / `VK_FORMAT_D24_UNORM_S8_UINT`
4. 这一阶段的场景是「地面 + 立方体」，`draws().size() == 2`（由框架的 `buildScene`
   产出，你不用管，但它失败说明你把 `initUpTo` 的阶段判断改坏了）
5. golden 比对 `p02-t06`：立方体的前后关系正确。没写深度测试的话这张图会明显不同 ——
   立方体看起来像被从里往外翻过来一样
6. validation 零报错（error 和 warning 都要为 0）

## 阅读材料

1. **vulkan-tutorial.com** → *Depth buffering*。整节都要读，尤其是
   `findSupportedFormat` 那段候选列表的写法。
2. **Vulkan Spec** → *Fragment Operations* 一章的 *Depth Test* 小节：
   看清楚深度测试发生在片元着色器的**哪一侧**，以及 early-z 的适用条件。
3. **Vulkan Spec** → *Fixed-Function Vertex Post-Processing* → *Controlling the Viewport*。
   这里定义了 Vulkan 的 NDC 深度范围是 0 到 1，是下面那条 GLM 坑的规范依据。
4. **Vulkan Spec** → *Formats* 一章里 `VkFormatProperties::optimalTilingFeatures` 与
   `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` 的说明。

## 关键决策，先自己想清楚

**为什么不能写死 `VK_FORMAT_D32_SFLOAT`？**
深度格式的支持情况按设备而异，规范只保证「D32_SFLOAT 和 X8_D24_UNORM_PACK32
至少有一个可用」这类下限，不保证某个具体格式一定在。
所以要用 `findSupportedFormat` 从候选列表里按**精度从高到低**挑第一个可用的：
D32_SFLOAT（32 位浮点，桌面卡标配）→ D32_SFLOAT_S8_UINT（多 8 位模板，
将来做描边、遮罩要用）→ D24_UNORM_S8_UINT（24 位定点，移动端和老卡上更常见）。
写死在你的机器上一路绿灯，直到某天在 MoltenVK 或安卓上拿到
`VK_ERROR_FORMAT_NOT_SUPPORTED` —— 而那时你已经忘了这里。

**clear 值为什么是 1.0 而不是 0.0？**
框架的 pipeline 用 `VK_COMPARE_OP_LESS`：深度值更小的通过。
配合 Vulkan 的 0 到 1 深度范围，近 = 0、远 = 1，所以每帧开始必须把深度缓冲
清成**最远**的 1.0，第一个片元才能写进去。清成 0.0 的话什么都通不过测试，
画面只剩背景色 —— 而这个症状看起来很像「根本没画」，会把你引到完全错误的方向去查。

**深度图为什么要传 `m_sampleCount` 而不是直接写 1？**
framebuffer 要求所有附件同尺寸、同采样数。t08 会把颜色附件变成多采样的，
那时深度附件必须跟着变。现在就把这个参数接出去，t08 就不用回头改这个文件。
在 t08 之前 `m_sampleCount` 恒为 `VK_SAMPLE_COUNT_1_BIT`。

## 常见坑

- **不定义 `GLM_FORCE_DEPTH_ZERO_TO_ONE`**
  OpenGL 的 NDC 深度是 -1 到 1，Vulkan 是 0 到 1。不定义这个宏，
  `glm::perspective` 吐出来的投影矩阵会把近一半的深度范围映射到负数，
  而负数在 Vulkan 里直接被裁掉。症状是「近处的东西莫名其妙消失了，远处却是对的」——
  几乎不可能联想到根因是一个 GLM 宏。
  本仓库在顶层 `CMakeLists.txt` 里已经全局定义了它（和 `GLM_FORCE_RADIANS` 一起），
  所以你不会踩到；但你必须知道它在那儿、以及为什么，否则下一个自建工程就中招。

- **深度 image view 的 aspectMask 写成了 `VK_IMAGE_ASPECT_COLOR_BIT`**
  复制粘贴 t05 的纹理代码最容易漏这一个参数。深度图必须用
  `VK_IMAGE_ASPECT_DEPTH_BIT`。validation 会报，但报的是「这个 aspect 对这个格式无效」，
  一眼看过去像是格式挑错了，于是你会跑去改 `findDepthFormat` —— 改错了地方。

- **深度图和颜色附件尺寸对不上**
  `vkCreateFramebuffer` 会直接失败。常见来源是拿窗口尺寸而不是
  `m_swapchain->extent()` —— 那两者在 Retina 屏和某些缩放设置下不是一回事，
  而且 swapchain 的 extent 是被驱动 clamp 过的。永远从 swapchain 拿。

- **resize 之后没重建深度图**
  框架的 `onSwapchainResized` 会调 `destroySizeDependent` 再调
  `createDepthResources`，所以这条路已经替你接好了。但要理解为什么必须重建：
  深度图尺寸绑死在 swapchain extent 上，尺寸一变旧的那张就再也进不了 framebuffer。
  漏了的话不是画面错，是 `vkCreateFramebuffer` 直接报错。

- **忘了 layout transition，或者以为它是必须的**
  两个方向都值得想清楚。参考实现显式做了一次
  `UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL`，但严格来说可以省：
  render pass 里深度附件的 `initialLayout` 是 `UNDEFINED`、`loadOp` 是 `CLEAR`，
  驱动会自己完成转换。显式做是为了养成「资源建好之后先转到它该在的 layout」的习惯 ——
  到 P4 做 G-Buffer 时就没有 render pass 替你兜底了。
  真要做的话别忘了 aspect 也得传 `VK_IMAGE_ASPECT_DEPTH_BIT`。

- **深度图的 usage 里加了 `SAMPLED_BIT` 或 `TRANSFER_SRC_BIT`**
  这一题只需要 `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`。
  多给的标志不会报错，但可能让驱动放弃某些压缩优化 —— 属于「不报错的性能损失」，
  最难被发现的那一类。

## 做完之后

```bash
git diff done/p02-t06 -- projects/p02-resources/src/steps/06_depth.cpp
```
