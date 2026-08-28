# P2-t02 — UBO / Descriptor Set Layout / Pool / Set

## 目标

学会 Vulkan 里绑定资源的唯一机制：descriptor。

OpenGL 的 `glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo)` 是一句话的事，
因为绑定点是全局状态，驱动在 draw call 时替你把它翻译成硬件寄存器。
Vulkan 把这个翻译过程摊成三个显式对象，而且这三个的生命周期完全不同：

| | 是什么 | 什么时候定 |
|---|---|---|
| `VkDescriptorSetLayout` | **模板** —— 第几号槽位放什么类型、哪些阶段能看到 | 建 pipeline 之前，和 shader 一起定死 |
| `VkDescriptorPool` | **容量规划** —— 这一池子最多能发几个 set、每类描述符总共几个 | 启动时按预算算好 |
| `VkDescriptorSet` | **实例** —— 具体是哪个 buffer、哪张图 | 分配出来，再 `vkUpdateDescriptorSets` 填内容 |

layout 里不含任何具体资源，pool 里不含任何具体资源，只有 set 才指向真东西。
这套结构后面一直在用：t05 的纹理、P3 的 storage buffer、P4 的 G-Buffer 输入，
全部走同一条路。这一题把它嚼透，后面就是重复劳动。

另外这题起画面开始走相机矩阵。t01 的顶点坐标直接就是 NDC，从这里开始
`gl_Position = proj * view * pos`。

## 要实现的函数

`src/steps/02_uniforms.cpp`：`createDescriptorSetLayout` / `createUniformBuffers` /
`createDescriptorPool` / `createDescriptorSets` / `updateUniformBuffer`

`createBuffer` 是 t01 写的，这里直接复用 —— UBO 和顶点缓冲的区别只在于
它要 host-visible、要常驻映射。

注意 `initUpTo` 的顺序：layout / uniform buffers / pool 建在前面，
但 `createDescriptorSets` 被刻意挪到了纹理和采样器之后 —— 因为到 t05 时
它要把纹理也写进同一个 set。你现在写的 `createDescriptorSets` 要能容忍
「纹理还不存在」这种情况。`createDescriptorSetLayout` 和 `createDescriptorPool`
里已经有按 `m_reached` 分档的写法，照着理解就行。

## 验收标准

```bash
python rwb.py test p02-t02
```

1. `descriptorSetLayout()` 和 `descriptorPool()` 都非 `VK_NULL_HANDLE`
2. `uniformBuffers().size() == kFramesInFlight`，
   `descriptorSets().size() == kFramesInFlight`（本课是 2）
3. 每个 UBO 都 `valid()`、`mapped != nullptr`、`size >= sizeof(CameraUniform)`
4. 每个 descriptor set 都非 `VK_NULL_HANDLE`
5. 画面与基准图一致：四边形经过相机变换后的样子（L3；基准图未入库时 SKIP）
6. validation 零 error 零 warning

## 阅读材料

1. **vulkan-tutorial.com** → Uniform buffers → *Descriptor layout and buffer*、
   *Descriptor pool and sets* 两节。这是最直白的一遍。
2. **vkguide.dev** → *Descriptor Sets* 一章。它讲的是「怎么把这套东西
   封装成引擎里能用的抽象」，和上一条互补 —— 教程教你调 API，它教你为什么要抽象。
3. **Vulkan Spec** → *Resource Descriptors* 章。特别是 descriptor set 的
   兼容性规则（什么情况下换了 pipeline 之后已绑定的 set 还有效）。
4. **OpenGL Wiki** → *Interface Block (GLSL)* → Memory layout 里的 std140 部分。
   C++ 结构体和 GLSL uniform block 的内存布局对不上，是这一题最难查的一类 bug。

## 关键决策，先自己想清楚

**为什么每个在飞帧要一份 UBO，而不是一份加个 barrier？**
本课有 2 个在飞帧：CPU 在为第 N+1 帧写 UBO 时，GPU 很可能还在读第 N 帧的同一块内存。
要解决它只有两条路 —— 要么加同步等 GPU 读完（那就把并行度打回 1，
多帧在飞的意义全没了），要么复制一份。UBO 才 128 字节，复制便宜到不用犹豫。
这是「多帧并行」的真实代价：**不只是命令缓冲要复制，所有 CPU 每帧会改的资源都要**。
反过来说，顶点缓冲、纹理这种「传上去就不改」的资源只要一份就够。

**descriptor set 也要每帧一份吗？**
在本课的写法里要 —— 因为每帧那个 set 指向的是不同的 UBO，而 set 的内容是
`vkUpdateDescriptorSets` 时就写死的。
另一条路是建一个大 UBO 装下所有帧的数据，只要一个 set，绑定时用
`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` 传一个动态偏移进去。
真实引擎基本都这么干（省下大量 set 分配）。本课先用最直白的版本，
先把三层结构本身理解对，优化留到你看得懂 profiler 之后。

**pool 的容量到底怎么算？**
`maxSets` 和 `pPoolSizes[i].descriptorCount` 是两个独立的预算，很多人以为是一个。
前者是「这池子最多能分出几个 `VkDescriptorSet`」，后者是「这一类描述符总共能发几个」。
两个 set 每个含一个 uniform buffer + 一个 sampler，就是 `maxSets = 2`、
uniform 计数 2、sampler 计数 2（t05 之后是这个形状；t02 这一档还没有纹理，
只有 uniform 那一项）。算少了 `vkAllocateDescriptorSets` 返回
`VK_ERROR_OUT_OF_POOL_MEMORY` —— **池不会自动扩容**，这是 Vulkan 一贯的风格：
容量由你规划，运行期不做隐式分配。

## 常见坑

- **`VkDescriptorBufferInfo` 是栈上的临时对象**
  `VkWriteDescriptorSet::pBufferInfo` 存的是指针，`vkUpdateDescriptorSets`
  调用的那一刻它必须还活着。把所有帧的 write 攒进一个 vector、循环结束后
  统一提交一次，是这一题最经典的 bug —— 前面几个 write 指向的 `bufferInfo`
  早就出作用域了。通常不崩，只是画面莫名其妙，极难查。

- **layout 的 binding 号 / stageFlags 和 shader 对不上**
  shader 里写 `layout(set = 0, binding = 0)`，C++ 侧的
  `VkDescriptorSetLayoutBinding::binding` 就必须是 0，`stageFlags` 必须包含
  真正读它的那个阶段。shader 声明了而 layout 里没有 —— 一定报错；
  反过来 layout 多声明了一个 shader 不用的 binding，在多数驱动上能过，
  但那是运气，不是规范保证。

- **忘了 `ubo.proj[1][1] *= -1.0f`**
  glm 出来的投影矩阵按 OpenGL 约定，NDC 的 +Y 朝上；Vulkan 的 +Y 朝下。
  不翻这一下画面上下颠倒。本题的场景左右对称、上下也接近对称，
  肉眼很可能看不出来，要到 t05 铺了地面才暴露 —— 但 golden 图现在就会抓住它。

- **UBO 忘了 host-visible + 常驻映射**
  照抄 t01 顶点缓冲的参数（`hostVisible=false`）的话，`mapped` 是 `nullptr`，
  验收第 3 条直接挂。UBO 每帧都要改，它和顶点缓冲的取舍是相反的。

- **std140 对齐**
  现在的 `CameraUniform` 是两个 `mat4`，怎么摆都对齐。但你一旦往里加一个
  `glm::vec3`，GLSL 会按 16 字节对齐它，C++ 按 12 字节 —— 后面所有字段全错位。
  症状是「加了个颜色参数，结果矩阵也不对了」。加字段时用 `alignas(16)`，
  或者干脆只放 `vec4` 和 `mat4`。

- **假设内存一定是 HOST_COHERENT**
  本课在 `VMA_MEMORY_USAGE_AUTO` + SEQUENTIAL_WRITE 之下拿到的通常是
  coherent 内存，写完不用刷。但这是**观察到的结果，不是保证**。
  真实项目里要么查 `vmaGetAllocationMemoryProperties`，要么无脑调
  `vmaFlushAllocation` —— 它对 coherent 内存是空操作。

## 做完之后

```bash
git diff done/p02-t02 -- projects/p02-resources/src/steps/02_uniforms.cpp
```
