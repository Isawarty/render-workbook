# P2-t08 — 多重采样（MSAA）

## 目标

看一眼 t07 的截图：立方体和棱柱的边缘是一级一级的台阶。
锯齿的根因很朴素 —— **一个像素只在中心采一次样**，三角形边缘要么全覆盖要么完全不覆盖，
没有中间值。像素被判定为「在里面」就上满色，「在外面」就一点色都没有。

超采样（SSAA）的解法是整张图按 N 倍分辨率画完再缩小，效果最好，代价是片元着色器
跑了 N 倍。MSAA 的做法聪明得多：每像素放 N 个采样点，但**只用它们做覆盖测试和深度测试**，
片元着色器**仍然只跑一次**，算出来的颜色写进所有被覆盖到的采样点。
边缘像素于是有了「覆盖了 3/4」这种中间状态。这就是它比超采样便宜得多的原因，
代价只在显存和带宽 —— 一个 4x MSAA 的颜色附件占 4 倍空间。

Vulkan 里开 MSAA 不是一个开关。你要额外建一张多采样的离屏颜色附件，
在 render pass 里声明 resolve 目标，framebuffer 里按顺序挂三张图，
pipeline 的 `rasterizationSamples` 也要跟着改。
后三件框架已经写好了（`createRenderPass` / `createFramebuffers` /
`createGraphicsPipeline` 里都有 `useMsaa` 分支），你的任务是把采样数定下来、
把那张离屏图建出来。

## 要实现的函数

`src/steps/08_msaa.cpp`：`maxUsableSampleCount` / `createColorResources`

这两个函数在 `initUpTo` 里被调用的时机很关键：**在 `createDepthResources`
和 `createRenderPass` 之前**。因为 `m_sampleCount` 一旦定下来，深度图要按它建、
render pass 的附件要按它声明，顺序反了就全对不上。

`createColorResources` 也会被 `onSwapchainResized` 调用，所以它必须能被反复调用。

## 验收标准

```bash
python rwb.py test p02-t08
```

1. `sampleCount()` 必须是 `VK_SAMPLE_COUNT_1_BIT` / `VK_SAMPLE_COUNT_2_BIT` /
   `VK_SAMPLE_COUNT_4_BIT` 之一 —— 本课封顶 4x，8x 以上画质收益迅速递减，
   而显存占用是线性涨的
2. 选出来的采样数必须在
   `limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts`
   这个**交集**里。测试自己把两个 limit 与了一遍再和你的结果比，
   所以只查颜色能力的实现会在这里挂掉
3. 如果采样数 > 1x：`colorTarget().valid()` 且 `colorTarget().view != VK_NULL_HANDLE`
4. 如果采样数 == 1x：测试给一条 `WARN` 就放过 —— 这台设备（通常是 lavapipe 这类
   软件渲染器）跑不到多重采样那条路径，不算你的错
5. golden 比对分两半，**这一题刻意不建自己的基准图**：
   - **结构上**必须和 `p02-t07` 的基准图一致。用的是 structural 容差
     （16x16 分块比平均色），因为场景和 t07 完全相同，区别只该有多重采样。
     构图变了说明你把别的东西改坏了
   - **逐像素上**必须和单采样版本**不同**。测试会另外起一个 `Stage::Model` 的 app
     渲一张单采样的图，然后 strict 容差逐像素比，断言的是 `REQUIRE_FALSE(passed)` ——
     两张图完全一致就说明 resolve 根本没生效，画面其实还是单采样那张。
     这一半只在采样数 > 1x 时才跑
6. validation 零报错（error 和 warning 都要为 0）

第 5 条那个反向断言是这一题最值钱的检查。MSAA 写错的典型后果不是崩溃、不是报错，
而是「一切正常，只是没有效果」—— 这条断言专门抓这个。

## 阅读材料

1. **vulkan-tutorial.com** → *Multisampling*。整节。
2. **Vulkan Spec** → *Rasterization* 一章的 *Multisampling* 小节：
   看清楚采样点位置是由实现定义的（所以跨 GPU 逐像素比对 MSAA 的图注定失败），
   以及 `sampleShadingEnable` / `minSampleShading` 的确切语义。
3. **Vulkan Spec** → *Render Pass* 一章里 `VkSubpassDescription::pResolveAttachments`
   的说明：它和 `pColorAttachments` 是**按下标一一对应**的，
   数组长度必须是 `colorAttachmentCount`。
4. **Khronos Vulkan-Samples** → performance 系列的 *MSAA* 示例。
   它专门讲了 tile-based GPU 上 `TRANSIENT_ATTACHMENT` 和 `storeOp = DONT_CARE`
   能省下多少带宽，有实测数字。

## 关键决策，先自己想清楚

**为什么必须查两个 limit 的交集？**
`framebufferColorSampleCounts` 和 `framebufferDepthSampleCounts` 是两个独立的位掩码，
**某些硬件颜色支持 8x 但深度只到 4x**。只查颜色的话你会挑一个深度附件建不出来的采样数，
而失败点在 `vkCreateFramebuffer` 或建深度图的时候 ——
错误信息只会说附件之间不兼容，**不会告诉你是深度的锅**。
这是 MSAA 最经典的一个坑，抄教程抄一半的人几乎都踩过。

**为什么不能直接往 swapchain 图像上画多采样？**
swapchain 的图像永远是单采样的 —— 显示引擎不认识多重采样，
它要的是一张能直接扫描输出的图。所以必须另开一张多采样的离屏图来画，
画完在 render pass 里 **resolve** 过去。
resolve 这一步由 subpass 的 `pResolveAttachments` 声明，驱动自己完成，
你不用写任何拷贝命令 —— 而且在 tile 架构上它是免费的，直接在片上做完。

**`VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` 到底省了什么？**
它是在告诉驱动「这块内容不会离开这趟 render pass」。
tile-based GPU（Apple Silicon、所有移动端）据此可以把这张图**完全留在片上内存**，
一个字节都不写回显存。桌面独显上这个标志基本不省什么，
但加上它几乎零成本，而且它逼你想清楚这张图的生命周期。
配套的是 render pass 里那张多采样颜色附件的 `storeOp` 用
`VK_ATTACHMENT_STORE_OP_DONT_CARE` —— resolve 出去就够了，
多采样图本身没人再读，省掉一次显存写回。两者要一起用才有意义。

## 常见坑

- **用 `vkCmdResolveImage` 手动 resolve**
  它是个真实存在的 API，也确实能把多采样图变成单采样图，所以这条路走得通、
  测试也能过。但它是一次独立的、跑在 render pass 之外的全屏拷贝：
  多采样图必须真的写回显存、再读回来。
  用 `pResolveAttachments` 的话 resolve 发生在 render pass 内部，
  tile 架构上直接在片上完成，一个字节都不进显存。差别不体现在正确性上，
  只体现在移动端的帧率上 —— 属于「跑得通但白扔一半带宽」的那类错误。

- **framebuffer 的附件顺序和 render pass 对不上**
  框架里的顺序是 color（多采样）→ depth → resolve（swapchain 图像），
  两处必须严格一致。顺序错了 validation 会报，但报的是格式或采样数不匹配，
  信息很不直观 —— 你得自己回去数下标。
  `VkAttachmentReference::attachment` 就是这个数组的下标，别把它和 shader 里的
  location 搞混。

- **采样数只有 1x 时还建了离屏图**
  设备只支持单采样时应该直接退回「往 swapchain 图像上画」那条路。
  这时候还建一张 1x 的离屏图并挂进 framebuffer，render pass 的 resolve 分支
  就和实际附件数量对不上了。`createColorResources` 开头要有这个提前返回。

- **忘了 `createColorResources` 会在 resize 时被再调一次**
  离屏图的尺寸绑死在 swapchain extent 上，窗口一变就得重建。
  框架的 `onSwapchainResized` 已经接好了（先 `destroySizeDependent` 再重建），
  但如果你在函数里做了「已经建过就跳过」这类缓存，重建路径会拿着旧尺寸的图继续用 ——
  症状是 resize 之后画面被拉伸或者裁掉一块，而不报任何错。

- **以为 MSAA 能解决纹理上的锯齿**
  默认的 MSAA 只对**几何边缘**生效，因为片元着色器每像素只跑一次，
  三角形内部的采样点共享同一个颜色。棋盘格纹理在远处的闪烁它管不了 ——
  那是 t05 的 mip 链在管。想连纹理锯齿一起处理要开 `sampleShadingEnable` +
  `minSampleShading`，代价是片元着色器真的跑 N 次。

- **`sampleRateShading` 是可选特性，用之前要先查**
  它不是核心保证的功能，得在建 device 时申请、用之前确认拿到了。
  框架里已经有 `m_ctx->enabledFeatures().sampleRateShading` 这个判断，
  没拿到就退回普通 MSAA。直接开而不查的话是 validation error。

- **拿 MSAA 的截图去和别的 GPU 逐像素比对**
  采样点的位置是实现定义的，不同 GPU 甚至不同驱动版本都可能不一样。
  这就是为什么第 5 条验收标准的前半用的是 structural 容差而不是 strict ——
  别指望 MSAA 的画面能跨设备逐像素对齐。

## 做完之后

```bash
git diff done/p02-t08 -- projects/p02-resources/src/steps/08_msaa.cpp
```

**扩展练习**（不判分）：把 `minSampleShading` 从 0.2 调到 1.0，
对比一下远处棋盘格的表现和帧率。再把 `TRANSIENT_ATTACHMENT_BIT` 去掉，
看看桌面卡上有没有可测量的差别 —— 大概率没有，这正是它值得理解的地方：
它是一个写给移动端看的承诺。
