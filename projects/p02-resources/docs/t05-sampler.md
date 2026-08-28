# P2-t05 — Image View / Sampler / Mipmap 生成

## 目标

把 t04 传上去的那张纹理真正用起来：给它一个 view，配一个 sampler，
再把 mip 链在 GPU 上生成出来。做完这一题，画面第一次出现纹理 ——
一块铺了棋盘格、uv 重复 8 次的地面。

OpenGL 里这三件事挤在一个纹理对象上：`glTexParameteri` 设过滤和 wrap，
`glGenerateMipmap` 一行生成 mip 链。Vulkan 把它拆成三个独立对象，因为**复用粒度不同**：
一张 image 可以有多个 view（不同 mip 范围、不同 swizzle、不同 aspect），
一个 sampler 则能给几百张 image 共用 —— 它压根不引用任何 image，只描述「怎么取样」。

`generateMipmaps` 是本题真正有难度的地方。它逼你面对一件事：
**同一张 image 的不同 mip 级可以同时处于不同的 layout**，而你要用 barrier 手工编排它们。
t04 那条「一次把整张图转过去」的 barrier 在这里不够用了。

## 要实现的函数

`src/steps/05_sampler.cpp`：`createImageView` / `generateMipmaps` /
`createTextureImageView` / `createTextureSampler`

（`createTextureImage` 已经在 t04 写好了，它会在检测到本阶段要生成 mip 链时，
把拷完 staging、还停在 `TRANSFER_DST` 的图直接交给你的 `generateMipmaps`，
后续 layout 全归它管。descriptor set 那边框架已经接好：
`createDescriptorSets` 会把 `texture().view` + `sampler()` 以
`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` 写进 binding 1。）

## 验收标准

```bash
python rwb.py test p02-t05
```

1. `texture().view != VK_NULL_HANDLE`
2. `sampler() != VK_NULL_HANDLE`
3. golden 比对 `p02-t05`：一块铺了棋盘格的地面，远处不该是一片闪烁的噪点
4. validation 零报错、零警告 —— mip 链生成里最常见的错误（最后一级忘了转 layout）
   就是在这一条上被抓住的

**关于第 3 条的判分，先说清楚，免得你以为测试放水**：这张图标了
`filteringSensitive = true`。各向异性过滤的实现是厂商私有的，NVIDIA 的 16x
和 lavapipe 的「压根没有」画出来的地面差得非常远，跨 GPU 逐像素永远对不齐 ——
但那不代表实现写错了。所以在真实 GPU 上它退化成**结构比对**：
先把两张图切成 16×16 的块取平均色，再比较块。构图错了照样抓得住，
过滤差异被平均掉。**严格的逐像素判分仍然在 CI 的 lavapipe 上做**（纯 CPU、确定性），
基准图也是那里生成入库的。本机基准图缺失时测试 SKIP，不是 FAIL。

## 阅读材料

1. **vulkan-tutorial.com** → Texture mapping → *Image view and sampler*。
   `VkSamplerCreateInfo` 每个字段都过一遍。
2. **vulkan-tutorial.com** → *Generating Mipmaps* 整节，重点是那个
   「blit 前后各插一条 barrier」的循环结构，以及末尾对最后一级的单独处理。
3. **Vulkan Spec** → Samplers 一章。当字典查 `mipmapMode`、`minLod`/`maxLod`、
   `unnormalizedCoordinates` 的确切语义。
4. **《Real-Time Rendering》第 4 版** → *Texturing* 章的 *Minification* 一节。
   讲清楚了 mipmap 为什么能消走摩尔纹，以及各向异性过滤解决的是哪一类残留问题。

## 关键决策，先自己想清楚

**mip 链为什么在 GPU 上生成，而不是 CPU 上算好一次传上去？**
离线管线通常就是后者，而且更好 —— 可以用更高质量的滤波核，也省下运行时开销。
GPU 生成的价值在**运行时才产生的图**：渲染目标、程序化纹理、动态 cubemap。
在这门课里它还有第二个价值：逼你把「同一张 image 的不同 mip 级处于不同 layout」
这件事亲手写一遍。做法是用 `vkCmdBlitImage` 从第 i-1 级缩一半到第 i 级，逐级递推。
同一张 image 既当源又当目标是**合法的** —— 因为源和目标是不同的 mip 级，
而且中间有 barrier 把它们的 layout 和访问隔开了。

**各向异性过滤到底解决什么问题，为什么必须先查？**
斜着看地面时，一个屏幕像素在纹理上覆盖的是一个**细长**的区域：沿视线方向拉得很长，
垂直方向很窄。各向同性的 mip 选择只能取两个方向里更粗的那一级，
于是远处的地面糊成一片。各向异性过滤沿长的那个方向多采几次，把细节留住。
但它是**可选特性**：用之前必须查 `Context::enabledFeatures().samplerAnisotropy`，
没开就设 `anisotropyEnable = VK_TRUE` 是未定义行为。开了的话
`maxAnisotropy` 从 `Context::properties().limits.maxSamplerAnisotropy` 取，别写死。

**`maxLod` 和 `mipmapMode` 怎么定？**
`maxLod` 要放得足够大，否则采样器根本用不到后面几级。`mipmapMode` 用
`VK_SAMPLER_MIPMAP_MODE_LINEAR` 表示在相邻两级之间再插值一次（也就是三线性过滤）；
用 `NEAREST` 的话，地面上会出现一条条**肉眼可见的 mip 分界线** —— 那道弧形的色带
就是 mip 级切换的位置。

## 常见坑

- **最后一级 mip 忘了单独收尾转 layout**
  这是本题最常见的错误。循环里每一轮转的是第 i-1 级（用完当源之后转成
  `SHADER_READ_ONLY_OPTIMAL`），而**最后一级从来没当过 blit 的源**，
  所以它还停在 `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`。循环外必须再补一条 barrier
  单独把它转过去，否则 validation 直接报最后一级 layout 不对。

- **image view 的 `levelCount` 写成 1**
  view 决定了采样器**看得到**哪几级。只给 1 级的话，mip 链白生成了，
  采样器永远只能取最清晰那一级。症状恶心在于它不报错、也不黑屏 ——
  只是「远处画面有点闪」，很难联想到是 view 的问题。要写 `image.mipLevels`。

- **barrier 的 `subresourceRange` 没收窄到一级**
  循环里每条 barrier 只该动一级：`levelCount = 1`，`baseMipLevel` 每轮推进。
  照抄 t04 那条「一次转全部」的 barrier 会把还没写入的级也一起转了，
  下一轮 blit 的源 layout 就对不上。

- **没查格式支不支持线性过滤的 blit**
  生成前必须用 `vkGetPhysicalDeviceFormatProperties` 查
  `optimalTilingFeatures` 里有没有 `VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT`。
  不是所有格式都支持，某些移动 GPU 上直接是错的。注意查的是
  `optimalTilingFeatures` 而不是 `linearTilingFeatures` —— 我们的 image 是 OPTIMAL 平铺的。
  真实项目里查出来不支持时的正解是回退到「离线生成好再上传」。

- **每级尺寸直接除以 2，除到了 0**
  `VkImageBlit::dstOffsets[1]` 的宽高必须至少是 1。正方形纹理碰不上这个坑，
  非正方形的会先有一边到 1，另一边还在往下减 —— 那一边就得钳住不动。

- **用各向异性没查特性开关**
  `samplerAnisotropy` 没在建 device 时启用就直接 `anisotropyEnable = VK_TRUE`，
  在开发机上可能一切正常，换台设备就是未定义行为。两条分支都要写全，
  关掉的那条记得把 `maxAnisotropy` 设成 1.0f。

- **回头发现 t04 建 image 时漏了 `TRANSFER_SRC`**
  blit 的源是这张纹理自己。usage 是创建时定死的，漏了这里就报 image 不能当传输源。
  修的地方在 `04_texture.cpp`，不在这一题的文件里。

## 做完之后

```bash
git diff done/p02-t05 -- projects/p02-resources/src/steps/05_sampler.cpp
```
