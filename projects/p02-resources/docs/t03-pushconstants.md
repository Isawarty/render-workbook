# P2-t03 — Push Constant，以及它和 UBO 的分界线

## 目标

学会 Vulkan 里第二条给 shader 送数据的路径，并且想清楚什么时候该走哪一条。

t02 的 UBO 走的是「显存 + 描述符」：数据躺在一块 buffer 里，descriptor set
告诉 shader 去哪找它。push constant 完全不同 —— 数据在录制命令缓冲的时候
就被**拷进命令流本身**，`vkCmdPushConstants` 和 `vkCmdDraw` 一样是一条命令。
没有 buffer、没有描述符、没有显存分配，也因此**没有任何同步问题**。

代价是容量。规范只保证 `maxPushConstantsSize >= 128` 字节，这是整个 Vulkan
里最吝啬的下限之一，而且那 128 字节是**所有着色器阶段加起来的总量**，
不是每个阶段各 128。一个 `mat4` 就 64 字节。

三者的分工，做完这题你应该能不看笔记说出来：

| | 顶点缓冲（t01） | UBO（t02） | push constant（t03） |
|---|---|---|---|
| 数据量 | MB 级 | KB 级 | **≤ 128 字节** |
| 更新频率 | 上传一次 | 每帧一次 | **每次 draw call** |
| 要自己分配显存 | 是 | 是 | **否** |
| 需要 descriptor | 否 | 是 | **否** |
| 每个在飞帧要一份吗 | 不要 | **要** | **不要** |

最后一行是这一题真正的分水岭，下面会展开。

本阶段的场景是同一份四边形几何画三次，每次一个不同的 model 矩阵 ——
这正是 push constant 的教科书用法：数据极小、每次 draw 都不同。

## 要实现的函数

`src/steps/03_pushconstants.cpp`：`pushConstantRanges` / `pushObjectData`

只有两个短函数，是 P2 里代码量最小的一题 —— 但值得花时间的是想明白上面那张表，
不是敲这几行。

`pushConstantRanges()` 的返回值被两处用：框架的 `createGraphicsPipeline`
拿它去填 `VkPipelineLayoutCreateInfo::pPushConstantRanges`，
测试通过 `pushConstantRangesForTest()` 读同一份声明。
`pushObjectData()` 由框架的 `recordFrame` 在每次 `vkCmdDrawIndexed` 之前调用。
shader 侧（`shaders/scene_push.vert` 里的 `layout(push_constant) uniform Push`）
已经写好了。

## 验收标准

```bash
python rwb.py test p02-t03
```

1. `pushConstantRangesForTest()` 非空
2. 每一个 range 的 `stageFlags` 都包含 `VK_SHADER_STAGE_VERTEX_BIT`
   —— model 矩阵是在顶点着色器里用的
3. 所有 range 里 `offset + size` 的最大值 `<= 128`，且 `>= sizeof(ObjectPush)`（64）
4. `draws().size() == 3`（场景由框架搭好，这条是在确认你驱动到了正确的阶段）
5. 画面与基准图一致：三个各自平移、旋转、缩放过的四边形（L3；基准图未入库时 SKIP）
6. validation 零 error 零 warning

## 阅读材料

1. **vkguide.dev** → *Push Constants* 一节。它是少数把「什么时候不该用
   push constant」也讲清楚的材料。
2. **Vulkan Spec** → *Pipeline Layouts* 里 push constant range 的部分，
   以及 *Push Constant Updates*。重点看两条硬规则：`offset` 和 `size`
   必须是 4 的倍数；`vkCmdPushConstants` 的 `stageFlags` 和 range 声明的关系。
3. **vulkan.gpuinfo.org** 上查 `maxPushConstantsSize` 的真实分布。
   看完你会明白为什么 128 这个数字必须当真 —— 桌面 NVIDIA 给得很宽，
   移动端和一部分驱动就是踩着下限走的。
4. **Sascha Willems 的 Vulkan-Examples** 里的 `pushconstants` 示例。当模板查。

## 关键决策，先自己想清楚

**model 矩阵为什么不放 UBO？**
放得进去，但会难看。三个物体三个矩阵，要么建三个 UBO 配三个 descriptor set
（物体一多，set 的分配和绑定就成了瓶颈），要么建一个大 UBO 装下所有物体、
用 `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` 加动态偏移逐个绑。
后者是真实引擎的做法，但它需要你自己处理 `minUniformBufferOffsetAlignment`
对齐、需要管理这块大 buffer 的分配。
push constant 是三条路里唯一不需要任何额外机制的 —— 代价就是 128 字节的天花板。
三个四边形看不出差别，三千个物体就看得出来了。

**为什么 push constant 不需要「每个在飞帧一份」，而 UBO 需要？**
这是全题最值得想清楚的一句话。UBO 是一块**共享的显存**：CPU 写它的时候
GPU 可能正在读上一帧的同一个地址，所以必须复制。
push constant 的数据在 `vkCmdPushConstants` 那一刻就被拷进了命令缓冲，
它**是命令流的一部分**，随命令缓冲一起走。而命令缓冲本身已经是每个在飞帧一份了 ——
复制在更外层已经做过一次，这里不用再做。
换句话说：不是 push constant 有什么魔法，是它把同步问题委托给了已经解决同步的那一层。

**为什么不干脆什么都用 push constant？**
除了 128 字节的上限之外，还有一条：每调一次 `vkCmdPushConstants`，数据就被
拷进命令缓冲一次。一千次 draw 就是一千份拷贝，命令缓冲本身会膨胀，
录制开销也是实打实的。规律是 —— **每帧一次的、稍大的数据走 UBO；
每次 draw 一次的、极小的数据走 push constant**。

## 常见坑

- **在自己的机器上超过 128 字节也能跑**
  桌面 NVIDIA 的 `maxPushConstantsSize` 常见是 256，AMD 有的给 128。
  你塞两个 `mat4`（128 字节）再加一个 `vec4`，本机毫无问题，
  换台机器 `vkCreatePipelineLayout` 直接失败 —— 而且报的是 pipeline layout
  创建失败，跟你几天前加的那个字段隔了十万八千里。
  别忘了那 128 是**所有阶段共享**的：顶点用 64、片元再用 96，加起来就已经超了。

- **以为 push 一次就够了**
  框架的 `recordFrame` 里 `pushObjectData` 是写在 draw 循环**内部**的，
  这不是随手写的 —— `vkCmdPushConstants` 改的是「从这条命令往后」的状态，
  每次 draw 前不重新 push 的话，三个四边形会用同一个 model 矩阵完全重叠，
  画面上看起来像只画了一个。你写自己的渲染循环时会真的踩到这条。

- **值不跨命令缓冲保留**
  push constant 的内容不是持久状态，命令缓冲重录之后就没了。
  本课每帧都重录，所以每帧都要重新 push。
  另外换绑一条 pipeline layout 不兼容的管线之后，之前 push 的值也会失效。

- **`vkCmdPushConstants` 的 `stageFlags` 和 range 声明不一致**
  最省事也最安全的做法是：调用时原样传 range 里声明的那个 `stageFlags`。
  自作聪明只传一半，或者传了 range 里没声明的阶段，validation 都会报错。

- **`offset` / `size` 不是 4 的倍数**
  规范硬要求。用 `sizeof(ObjectPush)` 这种 `mat4` 结构体不会撞上，
  但你自己加个 `bool` 或者三个 `float` 凑数的时候就会。

- **shader 里的 push constant 块和 C++ 结构体布局不一致**
  和 t02 的 std140 是同一类问题，而且这里更隐蔽：push constant 没有 descriptor
  可以给 validation 做交叉检查，写错了就是安静地读到垃圾数据。
  症状是物体飞到屏幕外或者整个消失。

## 做完之后

```bash
git diff done/p02-t03 -- projects/p02-resources/src/steps/03_pushconstants.cpp
```
