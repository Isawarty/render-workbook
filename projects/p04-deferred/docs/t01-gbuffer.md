# P4-t01 — 多附件 G-buffer

## 目标

建立延迟渲染的资源地基：创建三张语义明确的颜色附件和一张深度附件，把它们接入
framebuffer 与两子通道 render pass，并让这些资源随 swapchain resize 正确重建。

完成后你应能解释：为什么 G-buffer 需要多个格式、attachment 顺序怎样跨 render pass 与
framebuffer 对齐、`loadOp/storeOp` 与 initial/final layout 分别解决什么问题。非目标：这一题
不创建 graphics pipeline，不绘制几何，也不实现 PBR 光照。

## 要实现

在 `src/steps/01_gbuffer.cpp` 中完成：

- `createImage`：用 VMA 创建 2D image，并创建覆盖 color 或 depth aspect 的 image view；
- `createGBufferResources`：按项目 README 中的附件契约创建三张颜色图和深度图；
- `createRenderPass`：geometry subpass 写 G-buffer/depth，lighting subpass 写 swapchain；
- `createFramebuffers`：每张 swapchain image 对应一个 framebuffer，附件顺序必须与 pass 一致。

框架已提供录制、清理、resize 和 raw readback。t01 的 pass 只有 clear，没有 draw；清屏值是
可观测测试信号，不是最终渲染效果。

## 验收标准

```bash
python rwb.py test p04-t01
```

通过信号：CTest 报 `p04-t01 Passed`，且同时满足：

1. 三张颜色图、深度图、render pass 和全部 framebuffer 均创建成功；
2. 图片尺寸跟随 swapchain，颜色图含 color/input/transfer-src usage；
3. GPU raw readback 得到三组预定 clear 值，包括 `RGBA16F` 法线附件；
4. validation error/warning 均为 0，测试没有 SKIP。

## 调试入口

- final 命令就是上面的 `python rwb.py test p04-t01`。macOS 上测试会短暂创建隐藏窗口；
  若在受限终端看到 Cocoa/LaunchServices 错误，应在正常本机终端运行，而不是修改 Vulkan 同步。
- 推荐断点：`createGBufferResources` 末尾、`vkCreateRenderPass` 前、
  `vkCreateFramebuffer` 前；观察 format、usage、extent 与 views 的顺序。
- 对象创建失败通常是 format/usage 或 attachment 数量问题；L1 红通常是 layout、subpass dependency
  或生命周期问题；L2 像素错误通常是 clear 顺序、storeOp 或读回前 layout 问题。
- 在 RenderDoc/Xcode GPU Capture 中，第一子通道应有三张 color attachment 和 depth；第二子通道
  应只有 swapchain color attachment。t01 没有 draw call 是正常现象。

## 关键不变量

1. render pass 的 attachment 索引、framebuffer view 顺序、clear value 顺序必须完全一致。
2. 三张 G-buffer 在 pass 结束后保留内容，并进入后续片元阶段可读的 layout。
3. depth format 必须由设备能力选择，不写死为某一个桌面格式。
4. resize 时先销毁 framebuffer/render pass，再销毁旧 image/view，然后按新 extent 重建。

## 分层提示

<details><summary>Hint 1</summary>
先分别写出五个 attachment description，再画出两个 subpass 各引用哪些索引。framebuffer 和
clear value 按这同一张索引表排列。
</details>

<details><summary>Hint 2</summary>
geometry subpass 的 `colorAttachmentCount` 是 3，并带 depth reference；lighting subpass 在 t01
只引用 swapchain。外部依赖要覆盖颜色输出和 early depth test。
</details>

<details><summary>Hint 3</summary>
三张颜色图都需要 `COLOR_ATTACHMENT | INPUT_ATTACHMENT | SAMPLED | TRANSFER_SRC`；
前两张 8-bit attachment 每像素 4 字节，`RGBA16F` 每像素 8 字节。
</details>

完成后可看：

```bash
git diff done/p04-t01 -- projects/p04-deferred/src/steps/01_gbuffer.cpp
```
