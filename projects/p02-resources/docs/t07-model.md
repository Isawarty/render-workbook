# P2-t07 — glTF 加载 / 多 mesh 绘制

## 目标

到这里为止，所有几何都是 `buildScene` 里硬写出来的：一块四边形、一个立方体，
顶点坐标直接躺在 C++ 代码里。真实项目的模型来自 DCC 工具，
而 glTF 2.0 是当下事实上的交换格式 —— Khronos 出品，和 Vulkan 同门，
数据布局本来就是照着「直接喂给 GPU」设计的。

这道题的核心**不是**解析 JSON。tinygltf 已经把文件读成了内存里的
`tinygltf::Model`，JSON 那一层你一行都不用碰。核心是三件事：

1. **accessor / bufferView / buffer 三层间接**，以及 `byteStride`。
   glTF 允许把 POSITION 和 NORMAL 交错放在同一个 bufferView 里，那时相邻元素之间
   隔的不是 `sizeof(元素)` 而是 stride。
2. **一个文件里有多个 mesh，各带自己的节点变换**，要合并进同一对
   vertex / index buffer —— 而不是一个模型开一对 buffer。
3. **索引要配 `vertexOffset`**。第二个 mesh 的顶点追加在第一个之后，
   但它自带的索引值还是从 0 开始数的。

本题要加载的 `assets/shapes.gltf` 由 `assets/make_shapes_gltf.py` 生成，
里面是三个 node、各带一个 mesh。**它的每一处「不规整」都是故意的** ——
如果资产全用最规整的形式（紧凑排列、u16 索引、只有 translation 的 node），
下面「常见坑」里的大半条都触发不到，你写错了测试也是绿的。

| mesh | 顶点布局 | 索引类型 | 节点变换 |
|---|---|---|---|
| Pyramid 四棱锥 | **交错**，`byteStride = 32` | **u8** (5121) | **`matrix` 形式**（4x4 列主序） |
| Prism 六棱柱 | 每属性一个 bufferView | **u32** (5125) | TRS，**带 rotation** |
| Octahedron 八面体 | 每属性一个 bufferView | u16 (5123) | TRS，只有 translation + scale |

顶点数据用 base64 data URI 内嵌，所以不需要处理外部 `.bin` 文件。

它们会被追加到框架已经建好的「地面 + 立方体」之后，
所以做完这题画面里一共是五次 draw。

## 要实现的函数

`src/steps/07_model.cpp`：`loadModel`

参考实现在匿名 namespace 里另切了三个辅助函数（读 float 属性、读索引、算节点变换），
你也可以照这个思路切 —— 但只有 `loadModel` 是头文件里声明的成员函数。

`loadModel` 在 `initUpTo` 里被调用的时机很关键：**在 `createVertexBuffer` /
`createIndexBuffer` 之前**。也就是说你的任务只是把数据填进 `m_vertices`、
`m_indices`、`m_draws`，GPU 上传是 t01 写好的代码替你做的。

## 验收标准

```bash
python rwb.py test p02-t07
```

1. `draws().size() == 5` —— 框架的「地面 + 立方体」两次，加上 glTF 来的三次
2. 三次 glTF draw 的 `vertexOffset` 严格递增且第一个 > 0，`firstIndex` 同样严格递增。
   前者说明你没把 glTF 顶点当成从 0 开始的，后者说明三个 mesh 真的各自追加了
3. 三次 draw 的 `indexCount` 都 > 0
4. **索引解码完整性**：对每个 mesh，它那一段的索引值必须全部落在自己的顶点范围内，
   **而且每个顶点都至少被引用过一次**。
   后半句是关键：三个 mesh 用了三种 componentType，只按一种去读的话，读出来的索引
   仍然「看起来合法」（多半偏小、大量重复 0），单纯的落界检查抓不到；
   但「有顶点没人引用」一定说明那段索引没解对
5. **`byteStride`**：Pyramid 的局部 y 范围必须是 `[-0.20, +0.60]`。
   它的三个属性交错在同一个 bufferView 里，按 `sizeof(vec3)` 连续读会把 uv 和颜色
   的字节当成坐标读出来 —— 而 uv 和颜色都在 `[0,1]`，顶点不会飞到天上去，只会挤成
   一团，y 范围立刻就不对了
6. **node 的 `matrix` 形式**：Pyramid 的 `model` 矩阵平移列必须是 `(-1.12, -0.50, 0.05)`
   （node 自带的平移，再叠上框架统一往下挪的 0.55）。只认 TRS 的加载器会把它当成单位
   矩阵让模型缩回原点；顺手转置一下的话 x/z 会跑到别处
7. **node 的 `rotation`**：Prism 的 `model` 矩阵左上 3x3 非对角元素必须显著非零
   （否则 rotation 被整个忽略了），**且旋转角必须是 52 度**。
   后半条专门抓四元数分量顺序 —— 顺序写反得到的仍是合法的单位四元数，
   非对角元素照样非零，只是转到了 167 度
8. golden 比对 `p02-t07`：三个模型出现在地面上，位置、姿态和大小都对
9. validation 零报错（error 和 warning 都要为 0）

第 4 条只做**上界**和**引用完整性**检查，不查你有没有把顶点读串；
第 5 到 7 条各盯一个具体的解析分支；真正抓「整体读成一团乱麻」的仍是第 8 条那张 golden 图。

第 7 条为什么不交给 golden：这个场景在真实 GPU 上走的是结构比对（16x16 块比平均色，
见 `tests/framework/ImageCompare.h`）。实测四元数顺序写反只让 **2.00%** 的块超阈，
而容忍上限正好也是 2.00% —— 卡在线上判为通过。严格逐像素只在 CI 的 lavapipe 上跑，
你在本机做题时抓不到。所以它必须是一条与设备无关的 CPU 断言。

下面「常见坑」里的每一条，除「层级递归」外都有资产真正触发、有断言真正判分。
换句话说：这道题没有「测试抓不到但你照样得写对」的东西 —— 抓不到的，就不算必做。

## 阅读材料

1. **vulkan-tutorial.com** → *Loading models*。它用的是 OBJ + tinyobjloader，
   格式不同，但「把加载器吐出来的数据摊平成一对 buffer」的思路是一样的。
2. **glTF 2.0 规范** → *Buffers and Buffer Views* 与 *Accessors* 两节。
   重点是 accessor 的 `byteOffset` 和 bufferView 的 `byteOffset` 是**叠加**的，
   以及 `byteStride` 什么时候可以省略、省略时默认值是什么。
3. **glTF 2.0 规范** → *Nodes and Hierarchy* 和 *Meshes*。
   看清楚 node 的变换可以写成 `matrix`，也可以写成 `translation` / `rotation` /
   `scale` 三件套，以及规范规定的 TRS 合成顺序。
4. **vkguide.dev** → GLTF loading 一章。它讲的是同一件事在一个更完整的引擎里
   长什么样，可以对照着看「哪些是这门课刻意省掉的」。

## 关键决策，先自己想清楚

**为什么用 `vkCmdDrawIndexed` 的 `vertexOffset` 参数，而不是在 CPU 上把索引值加好？**
两种都能画对。用 `vertexOffset` 的好处是：同一份索引数据可以被多个位置的实例复用，
而且加法由 GPU 在取顶点时顺手做掉，不占 CPU、不改数据。
更实际的理由是索引类型：如果一个 mesh 的索引是 u16，CPU 上加完偏移可能就溢出了，
你会被迫把整份索引提成 u32。用 `vertexOffset` 就没有这个问题 ——
它本身是 `int32_t`，和索引类型无关。

**为什么两种节点变换都要处理？**
glTF 允许 node 写一个 4x4 `matrix`，也允许写 TRS 三件套，两者互斥。
不同导出器的习惯不一样：Blender 默认吐 TRS，很多程序化生成的资产直接吐矩阵。
**只处理其中一种是加载器最常见的漏洞** —— 它在你手上的那个资产上完美工作，
换一个模型就整体错位，而错位的样子看起来像「模型本身导出坏了」。
`shapes.gltf` 两支都用了：Pyramid 走 `matrix`，另外两个走 TRS。

**多个 mesh 为什么要塞进同一对 buffer，而不是一人一对？**
一人一对写起来更简单，但每次 draw 都要重新
`vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer`，绑定切换是有代价的。
合并成一对之后，整个场景只绑一次，剩下的全靠 `firstIndex` + `vertexOffset` 定位。
这是所有现代渲染器的做法，也是后面做 indirect draw 的前提 ——
`VkDrawIndexedIndirectCommand` 的字段和 `MeshDraw` 长得几乎一模一样，不是巧合。

## 常见坑

- **忽略 `byteStride`**
  只按 `sizeof(元素)` 连续读，在紧凑排列的资产上一切正常。
  但 glTF 允许把 POSITION 和 NORMAL 交错在同一个 bufferView 里，
  那时相邻 POSITION 之间隔的是 stride 而不是 12 字节。
  症状是模型变成一团彻底认不出的乱麻 —— 而你的加载器在别的模型上明明是对的，
  于是你会先去怀疑导出设置。

- **四元数分量顺序搞反**
  glTF 的 `rotation` 是 `(x, y, z, w)`，而 `glm::quat` 的构造函数是 `(w, x, y, z)`。
  直接按下标 0/1/2/3 喂进去不会报任何错，也不会崩，只会让模型转到一个诡异的角度。
  因为它「看起来在动、只是不对」，你会本能地去查投影矩阵和相机 —— 查错方向。

- **忘了 `vertexOffset`**
  第二个 mesh 的索引还是从 0 开始数的，直接用就会去取第一个 mesh 的顶点。
  症状极具迷惑性：第二个模型的**位置是对的**（model 矩阵没错），
  但它长着第一个模型的形状。看起来像「导出器把两个 mesh 搞混了」。

- **索引的 componentType 只处理了一种**
  glTF 允许 `UNSIGNED_BYTE` / `UNSIGNED_SHORT` / `UNSIGNED_INT` 三种，
  `shapes.gltf` 三种各用了一个 mesh。三种都要统一提成 `std::uint32_t`，因为框架的
  `vkCmdBindIndexBuffer` 全课统一用 `VK_INDEX_TYPE_UINT32`。
  按 u16 去读一段 u32 数据的症状很有迷惑性：读出来的索引值仍然「看起来合法」
  —— 偏小、大量重复的 0 —— 落界检查一条都不会响，画面只是缺了一半三角形。
  验收标准第 4 条的「每个顶点都至少被引用一次」就是为它准备的。

- **矩阵主序想多了**
  glTF 的 `matrix` 是**列主序**，glm 也是列主序，所以 16 个 float 可以直接喂给
  `glm::make_mat4`，不需要转置。有 D3D 背景的人会本能地转一下，
  转完模型就镜像+错位了。

- **属性缺失时没有兜底**
  glTF 的 primitive 只保证有 `POSITION`。`TEXCOORD_0` 和 `COLOR_0` 都是可选的，
  accessor 索引会是 -1。`shapes.gltf` 两个都有，但缺失时得给个合理默认
  （uv 给 0、颜色给白），否则换个模型就是越界读或者一片纯黑。

- **只看 `model.meshes`，不走 scene 的节点**
  直接遍历 `model.meshes` 能拿到所有几何，但会**丢掉节点变换** ——
  三个模型会重叠在原点上。必须从 `model.scenes[defaultScene].nodes` 出发，
  一个 node 一个 node 地走，才拿得到每个 mesh 该有的位置。

## 做完之后

```bash
git diff done/p02-t07 -- projects/p02-resources/src/steps/07_model.cpp
```

**扩展练习**（不判分）：本课的加载器只走了场景的第一层 node，没有递归子节点，
也没有把父节点的变换乘下去。给 `nodeTransform` 加上递归和父变换累乘，
再用 Blender 导出一个有层级的模型试试。
