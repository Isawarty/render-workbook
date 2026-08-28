# P2-t01 — VMA / Vertex Buffer / Index Buffer / Staging 上传

## 目标

建立「CPU 内存里的数据怎么变成 GPU 能读的显存」这条通路。

P1 的三角形顶点是硬编码在 vertex shader 的常量数组里的，用 `gl_VertexIndex` 查表。
那是为了把「画出第一个三角形」需要的概念数压到最少。现在补上被跳过的那一半：
显存从哪来、谁能写它、写完怎么让 GPU 看见。

OpenGL 里这件事是一行 `glBufferData`。驱动替你决定了显存放哪、要不要中转、
什么时候真正拷过去。Vulkan 把这三个决定全还给你，于是你会第一次遇到
**「能被 CPU 写的内存」和「GPU 读得最快的内存」通常不是同一块**这个事实 ——
这正是 staging 存在的全部理由。

这一题还会引入 VMA（Vulkan Memory Allocator）。它不是「更简单的 API」，
而是替你做 sub-allocation：一次 `vkAllocateMemory` 切给很多个 buffer。
一些驱动的 `maxMemoryAllocationCount` 只有 4096，真实场景里手写分配器很快就撞墙。

## 要实现的函数

`src/steps/01_buffers.cpp`：`createBuffer` / `destroyBuffer` / `uploadViaStaging` /
`createVertexBuffer` / `createIndexBuffer`

前三个是**基础设施，后面的题一直在用**：`createBuffer` 会被 t02 的 UBO
和 t04 的纹理 staging 复用，`destroyBuffer` 被 `cleanup()` 用来收所有 buffer。
所以别把它写成只服务顶点缓冲的特化版本 —— `hostVisible` / `persistentlyMapped`
这两个参数就是留给后面那些题的。

顶点格式（`Vertex::bindingDescription` / `attributeDescriptions`）和绑定、
draw 的录制都在框架里写好了，不用你管。

## 验收标准

```bash
python rwb.py test p02-t01
```

1. `vertexBuffer().valid()` 和 `indexBuffer().valid()` 都为真
2. `vertexBuffer().size == sizeof(Vertex) * vertices().size()`，
   `indexBuffer().size == sizeof(std::uint32_t) * indices().size()`
3. `vertexBuffer().mapped == nullptr` —— 顶点缓冲必须在 device-local 显存里。
   它非空就说明你跳过了 staging，直接在 host-visible 内存里建了它
4. 画面与基准图一致：一个彩色四边形（L3；基准图未入库时该条 SKIP，不算失败）
5. validation 零 error 零 warning

## 阅读材料

1. **vulkan-tutorial.com** → Vertex buffers → *Vertex buffer creation*、
   *Staging buffer*、*Index buffer* 三节。它用的是手写 `findMemoryType`，
   你用 VMA —— 对着读，正好看清 VMA 替你做掉了什么。
2. **VMA 官方文档** → *Choosing memory type* 和 *Memory mapping* 两页。
   重点是 `VMA_MEMORY_USAGE_AUTO` 和 `VMA_ALLOCATION_CREATE_HOST_ACCESS_*` 标志的关系。
3. **"Vulkan Memory Types on PC and How to Use Them"**（Adam Sawicki）。
   讲清了 PC 上到底有哪几种堆、ReBAR 前后有什么区别。这一题的「为什么要 staging」
   的完整答案在这篇里。
4. **Vulkan Spec** → *Memory Allocation* 与 *Resource Creation* 两章。当字典查，不用通读。

## 关键决策，先自己想清楚

**为什么非要 staging？直接往顶点缓冲里 memcpy 不行吗？**
device-local 显存通常根本不映射到 CPU 地址空间 —— 没有虚拟地址可写。
确实存在同时带 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` 和
`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` 的堆，但在 ReBAR 普及之前它只有 256MB，
是留给「每帧都要改的小块数据」（比如 t02 的 UBO）的，不是拿来放几十 MB 顶点的。
所以标准做法是：写进能映射的中转 buffer，再让 GPU 用 `vkCmdCopyBuffer` 拷过去。

**顶点缓冲和 UBO 该用同一个 `createBuffer` 吗？**
该。它们的差别可以完全表达成两个 bool：要不要 CPU 能写、要不要常驻映射。
写成三个近似函数会在 t04 变成四个。但反过来也要警惕：如果发现自己往
`createBuffer` 里加第五个 bool，那就是该拆的信号了。

**staging buffer 用完立刻销毁，还是留着复用？**
本课用 `Context::immediateSubmit`，它提交后同步等 GPU 做完才返回，
所以回来时拷贝一定结束了，随手销毁是安全的。真实引擎会攒一批上传共用一个
常驻 staging ring、只等一次 fence —— 那时候就绝不能在提交后立刻销毁中转站，
因为 GPU 可能还没开始读。想清楚这两种写法的区别，比背下其中一种有用。

## 常见坑

- **`usage` 少写一个标志**
  顶点缓冲要 `VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`，
  少 TRANSFER_DST 则拷不进去，少 VERTEX_BUFFER 则绑不上。
  好消息是 validation layer 会当场点名，不用猜。

- **要了 `pMappedData` 却没给 `VMA_ALLOCATION_CREATE_MAPPED_BIT`**
  `VmaAllocationInfo::pMappedData` 会是 `nullptr`，接着 `std::memcpy` 直接崩。
  崩的位置在 memcpy，根因在几行之前的 flags 少了一位，中间隔着一层 VMA，
  第一次遇到很难联想。

- **在 host-visible 内存里回读**
  `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` 允许 VMA 选
  write-combined 内存：顺序写很快，**随机读慢到以为程序死了**。
  症状是「功能全对，就是某个函数莫名其妙跑了几百毫秒」。
  真要回读就换 `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT`。

- **用 `vkDestroyBuffer` 销毁 VMA 建的 buffer**
  能编过，也不报错，但 VMA 那边的 allocation 永远不回收。
  必须成对：`vmaCreateBuffer` ↔ `vmaDestroyBuffer`。

- **`destroyBuffer` 之后没把结构体清零**
  销毁完要把整个 `Buffer` 复位（`buffer = Buffer{}`），别只销毁不清 handle。
  框架里的 `destroySizeDependent()` 在 resize 和 cleanup 时都会跑，
  留着旧 handle 就是拿已释放的句柄再销毁一遍 —— validation 报「invalid handle」，
  而现场早已不在出问题的那一行。

- **`size` 算成了元素个数**
  `sizeof(Vertex) * m_vertices.size()`，不是 `m_vertices.size()`。
  验收第 2 条就是专门抓这个的；不抓的话你会得到一个能跑但只画出前几个顶点的程序。

## 做完之后

```bash
git diff done/p02-t01 -- projects/p02-resources/src/steps/01_buffers.cpp
```
