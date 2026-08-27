# P1-t05 — Shader Module / Graphics Pipeline

## 目标

把 shader 和全部渲染状态固化成一个不可变的 `VkPipeline`。

OpenGL 里绝大多数状态是全局的，随时 `glEnable` 就能改。Vulkan 把混合、深度、
光栅化、视口、shader 全部塞进一个创建时一次性校验完的对象。

- **代价**：状态组合爆炸。PSO 数量管理是真实引擎的核心问题之一。
- **回报**：draw call 期间驱动无事可做，也就不存在「驱动在你背后重编 shader」的卡顿。

## 要实现的函数

`src/steps/05_pipeline.cpp`：`createShaderModule` / `createGraphicsPipeline`

shader 源码已经给好了（`shaders/triangle.vert` / `.frag`），构建时由 glslang
自动编成 SPIR-V，路径在 `RWB_SHADER_DIR` 宏里。

## 验收标准

```bash
python rwb.py test p01-t05
```

`m_pipeline` 非空 + validation 零报错。

## 阅读材料

1. **vulkan-tutorial.com** → Graphics pipeline basics 整章（*Introduction*、
   *Shader modules*、*Fixed functions*、*Conclusion*）。
2. **Vulkan Spec** §10 *Pipelines*，重点是 §10.2 *Graphics Pipelines* 的
   状态结构体总览。不用背，当字典查。
3. 关于动态状态该开多少，读 **"Vulkan Dynamic State"**（Sascha Willems 的
   `VK_EXT_extended_dynamic_state` 示例说明）。
4. Vulkan 的 NDC 和 OpenGL 不同（Y 向下、Z 是 [0,1]）。读
   **"Vulkan Coordinate System"**（Johannes Unterguggenberger 的图解最清楚）。

## 关键决策

**为什么把 viewport/scissor 设成动态状态？**
不设的话，窗口一 resize 就要重建整条 pipeline。设成动态之后，t08 的重建路径
只需要重建 swapchain / image view / framebuffer 三样。这是本题给两题之后省的力气。

**为什么 vertex input 是空的？**
P1 的顶点硬编码在 vertex shader 的常量数组里，用 `gl_VertexIndex` 索引。
这样能把「画出第一个三角形」需要的概念数压到最少 —— vertex buffer、显存分配、
staging 上传全部留到 P2-t01。

## 常见坑

- **`codeSize` 传了 word 数**
  `VkShaderModuleCreateInfo::codeSize` 的单位是**字节**，而 `pCode` 的类型是
  `const uint32_t*`。这是 Vulkan API 里最经典的踩坑点之一。

- **shader module 建完就销毁 vs 留着**
  SPIR-V 在建 pipeline 时已经被编译进去了，module 建完 pipeline 就能扔。
  但注意异常路径上也要销毁，否则测试反复创建 app 时会泄漏。

- **动态状态开了却没在录制时设置**
  `viewportCount` 给了 1 但录制时没调 `vkCmdSetViewport`，validation 会直接报错。
  这个会在 t06 里体现出来。

- **`lineWidth` 没设**
  `polygonMode = FILL` 时也必须给合法值（1.0f）。非 1.0 需要 `wideLines` 特性。

## 做完之后

```bash
git diff done/p01-t05 -- projects/p01-triangle/src/steps/05_pipeline.cpp
```
