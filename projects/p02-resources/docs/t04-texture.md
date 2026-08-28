# P2-t04 — Image 创建 / Staging 上传 / Layout Transition

## 目标

把一张 CPU 侧生成的棋盘格像素，搬进显存，并让它停在「可以被 shader 采样」的状态。

OpenGL 里这是 `glTexImage2D` 一行。那一行背后驱动替你做了五件事：分配显存、
挑一种平铺方式、开一块中转内存把数据拷进去、插好同步、把资源转成可采样的排布。
Vulkan 把这五件事全摊开来让你写 —— 于是它变成十几步。

真正的新概念其实只有两个：**tiling**（像素在显存里怎么排）和 **layout**
（同一块显存针对不同用途的最优排布）。staging 上传、`immediateSubmit` 录一次性命令，
t01 已经做过一遍了，这题只是把目标从 buffer 换成 image。

注意这一题做完**画面不会变**。t05 才会有 view 和 sampler，shader 现在还没开始采样纹理。
所以这题的主要判据是 validation 干不干净，不是截图。

## 要实现的函数

`src/steps/04_texture.cpp`：`createImage` / `transitionImageLayout` /
`copyBufferToImage` / `createTextureImage`

（`destroyImage` 直接给你了 —— 理由同 t01 的 `destroyBuffer`：它是 `noexcept`，
而 `destroySizeDependent()` 在任何阶段都会调它，挖空的话你收到的是 abort 而不是报错。
像素数据也由框架的 `checkerboardPixels(kTextureSize)` 给出，RGBA8、256×256、
带一层随 x 变化的色调 —— 那层色调是为了让 t05 的 mip 链每一级都长得不一样。）

## 验收标准

```bash
python rwb.py test p02-t04
```

1. `texture().valid()` 为真
2. `texture().width == kTextureSize && texture().height == kTextureSize`（都是 256）
3. `texture().mipLevels == 9` —— 256×256 的完整 mip 链是 256,128,64,32,16,8,4,2,1。
   只建 1 级的话 t05 没东西可生成
4. golden 比对用的是 **`p02-t03` 那张基准图**：这一步只是把纹理传上去，
   画面变了就说明你顺手改了不该改的东西
5. validation 零报错、零警告 —— layout 转换写错会在这里被抓住，这是本题主要的判据

## 阅读材料

1. **vulkan-tutorial.com** → Texture mapping → *Images*。image 创建、staging 上传、
   layout transition 三件事都在这一节里。
2. **Vulkan Spec** → Resource Creation 章的 *Image Layouts* 一节。把那张
   「哪些 layout 允许哪些访问」的表当字典查，别背。
3. **Khronos wiki** → *Vulkan Synchronization Examples*。里面是一份 barrier 配方表，
   常见的 stage/access 组合都能对着抄思路。
4. **vkguide.dev** → *Vulkan Images* 一章，讲 barrier 时的口径和本题最接近。

## 关键决策，先自己想清楚

**为什么不能像 OpenGL 那样直接往 image 里写像素？**
因为标准路径用的是 `VK_IMAGE_TILING_OPTIMAL` —— 驱动为了采样时的缓存局部性
会把像素按自己的私有规则重排（Z 序、tile 分块之类），具体怎么排它不告诉你，
CPU 也就没法直接读写。另一种是 `VK_IMAGE_TILING_LINEAR`，行优先、CPU 能映射着写，
但采样慢，而且很多格式压根不支持它。所以标准路径永远是 **OPTIMAL + staging buffer 中转**。

**layout 到底是什么？**
同一块显存，在「被采样」「被当渲染目标写入」「被当拷贝源读」时，最优的物理排布是不同的。
layout 就是告诉驱动「接下来我要拿它干什么」，让它有机会重排。
`VK_IMAGE_LAYOUT_UNDEFINED` 特殊：从它转出去时，驱动**被允许直接丢弃旧内容**，
所以它是最快的起点。凡是「内容马上要被整个覆盖」的场合，oldLayout 都该写 UNDEFINED。

**usage flags 现在要带哪些？**
`TRANSFER_DST`（staging 拷进来）+ `SAMPLED`（shader 要采样）是明显的。
容易漏的是 `TRANSFER_SRC` —— t05 生成 mip 链时要从第 i-1 级 blit 到第 i 级，
这张纹理**自己就是 blit 的源**。usage 是创建时定死的，到 t05 才发现漏了就得回来改。

## 常见坑

- **barrier 只写对了 layout，access mask 填 0**
  一条 image memory barrier 同时表达三件事：排布转换（old→new）、执行依赖
  （`srcStage`/`dstStage`）、内存可见性（`srcAccessMask`/`dstAccessMask`）。
  只写对第一件，多数机器上画面看着是对的，直到换一块 GPU 才崩。
  这正是本仓库默认开 synchronization validation 的原因 —— 让它当场报出来。

- **`srcQueueFamilyIndex` / `dstQueueFamilyIndex` 填了实际索引**
  不做队列族所有权转移时，两边都必须是 `VK_QUEUE_FAMILY_IGNORED`。
  填成真实索引不是「更精确」，而是会**触发一次真正的 ownership transfer**，
  于是 validation 报你少了配对的另一半 barrier。

- **`subresourceRange.levelCount` 只写了 1**
  image 有 9 级 mip，转换时只覆盖第 0 级的话，剩下 8 级还停在 UNDEFINED。
  t04 阶段还看不出来，t05 一采样就报「level N 的 layout 不对」。
  想一次转全部就用 `image.mipLevels`。

- **`copyBufferToImage` 的 `dstImageLayout` 传成了目标 layout**
  这个参数问的是「拷贝**发生时**这张图处在什么 layout」，不是「拷完要变成什么」。
  这里只能是 `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`，而且你得真的在此之前转过去。

- **`bufferRowLength` / `bufferImageHeight` 当成了字节数**
  它俩的单位是**像素**（不是字节），而且填 0 表示「按 `imageExtent` 紧密排列」——
  绝大多数情况填 0 就对。填成 `width * 4` 的话，驱动会认为源数据每行有 1024 个像素，
  于是要求的 staging 大小是实际的四倍，validation 直接报「buffer 太小」。

- **staging buffer 销毁得太早**
  拷贝是录进 command buffer 的，不是调用 `vkCmdCopyBufferToImage` 当场就完成的。
  必须等提交的那批命令真的执行完（`immediateSubmit` 内部会等）才能释放 staging。
  提前释放在有的驱动上照样能跑出正确画面，属于最难查的一类 bug。

- **mip 级数写死成 1**
  `mipLevels` 是 `VkImageCreateInfo` 的字段，创建时定死。这题就要算出 9 来
  （`floor(log2(max(w,h))) + 1`），否则 t04 的断言直接挂，t05 也无从做起。

## 做完之后

```bash
git diff done/p02-t04 -- projects/p02-resources/src/steps/04_texture.cpp
```
