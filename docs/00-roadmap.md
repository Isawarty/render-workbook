# render-workbook 路线图

以「项目驱动 + 代码挖空」的方式学 Vulkan / Compute Shader / Lua / DirectX。
终点是一个属于自己的跨平台渲染框架。

P3 起的新任务，其挖空、测试资产与发布门槛遵循
[课程任务编写与反馈检查点规范](04-course-authoring.md)。核心取舍是：**不按代码行数拆题**，
优先把 validation、日志、debug name 和状态摘要做厚，让故障自己可观察；
只有在普通调试手段真的跨不过去的语义边界上，才加检查点。
P0–P2 在这份规范之前就已完成并逐 tag 验证过，不回头改造。

当初为什么这么选、哪些原始计划被推翻了，见
[设计决策与理由](05-design-decisions.md)。

**总量 ≈ 300–390 小时。按每周 12 小时算，主线（P0–P5）约 5–6 个月，全部约 12–15 个月。**
这是真实估计，不是保守估计。

## 项目序列

| 项目 | 内容 | 工时 | 状态 |
|---|---|---|---|
| **P0** | [环境与骨架](../projects/p00-setup/README.md) | 8–12h | ✅ 已交付 |
| **P1** | [三角形（全挖空）](../projects/p01-triangle/README.md) | 35–45h | ✅ 已交付 |
| **P2** | [资源与场景](../projects/p02-resources/README.md) | 45–60h | ✅ 已交付 |
| **P3** | [Compute Shader 专项](../projects/p03-compute/README.md) | 35–45h | ✅ 已交付 |
| **P4** | [延迟渲染 + PBR](../projects/p04-deferred/README.md) | 45–60h | ✅ 已交付 |
| P5 | Render Graph | 50–70h | 大纲 |
| P6 | Lua（语言速通 + 引擎嵌入） | 40–55h | 大纲 |
| P7 | D3D12 概念打通（Windows-only） | 35–45h | 大纲 |

P3、P4 已完整交付 t01–t07。

---

## P2 — 资源与场景 · 45–60h

P1 的三角形顶点是硬编码在 shader 里的。P2 把真实的数据管线建起来。

| task | 内容 |
|---|---|
| t01 | VMA 接入 + vertex/index buffer + staging 上传 |
| t02 | UBO + descriptor set layout / pool / set |
| t03 | push constant，并与 t02 的 UBO 做对比 |
| t04 | 纹理：image 创建、staging 上传、layout transition |
| t05 | image view + sampler + mipmap 生成 |
| t06 | 深度缓冲 + depth test |
| t07 | glTF 加载（tinygltf）+ 多 mesh 绘制 |
| t08 | MSAA（macOS 上注意 MoltenVK 差异） |

**产出**：P1 里你写的 instance/device/swapchain/同步代码提炼进 `engine/rhi/`。

> 原计划这里有一题 Dear ImGui 集成，已挪到 P5-t06 —— 那里才真的需要一个可视化面板。
> ImGui 的 Vulkan backend 是体力活，教不了 Vulkan 概念，
> 而把 t04 拆成「image + transition」和「view + sampler + mipmap」两题，粒度更匀。

---

## P3 — Compute Shader 专项 · 35–45h

> **当前进度：t01–t07 已交付。**

**全套里判分最硬的一段。** 每一题都是 GPU 结果回读后与 CPU 参考实现逐元素比对，
100% 确定性，不受光栅化和驱动差异影响。

| task | 内容 |
|---|---|
| t01 | compute pipeline + storage buffer + dispatch + readback（saxpy） |
| t02 | 并行归约，含 subgroup 操作（**macOS 需降级路径**） |
| t03 | 前缀和 scan |
| t04 | bitonic sort |
| t05 | 图像后处理：高斯模糊 / tonemap，compute → graphics 的 barrier |
| t06 | GPU 粒子系统（compute 更新 + graphics 渲染，跨队列同步） |
| t07 | indirect dispatch |

---

## P4 — 延迟渲染 + PBR · 45–60h

| task | 内容 |
|---|---|
| t01 | 多附件 G-Buffer（✅ 本地实现） |
| t02 | 几何 pass（✅ 本地实现） |
| t03 | 光照 pass（Cook-Torrance，✅ 本地实现） |
| t04 | IBL：irradiance / prefiltered / BRDF LUT（✅ compute 生成） |
| t05 | 固定光源 shadow map + 世界坐标重建 + PCF（✅ 本地实现） |
| t06 | 后处理链（✅ compute bloom + tonemap） |
| t07 | **全部 shader 从 GLSL 迁移到 Slang**（✅ 本地实现） |

t07 是 shader 语言的分水岭。Slang 一份源码同时产出 SPIR-V / DXIL / Metal，
正好覆盖 Vulkan + D3D12 两条线；而且它的语法基本是 HLSL 超集，工业界价值不丢。

---

## P5 — Render Graph · 50–70h

**面试最能聊的一节，也是自由发挥度最高的一节**（不给强基线，只给接口契约和测试）。

| task | 内容 |
|---|---|
| t01 | 声明式的 pass / resource API |
| t02 | 有向图构建 + 拓扑排序 |
| t03 | **自动 barrier 推导**（读写状态机） |
| t04 | 资源生命周期分析 + transient 资源别名（内存复用） |
| t05 | 用 render graph 重写 P4 的延迟管线 |
| t06 | 图可视化（graphviz 导出 / **ImGui 面板** —— ImGui 的集成从 P2 挪到了这里） |

图算法部分是纯 CPU 逻辑，可以写高覆盖单测；端到端验收是
「渲染结果与 P4 的 golden image 一致」。

---

## P6 — Lua · 40–55h

分两段，因为你是从零开始。

**t00 语言速通（10–15h，无挖空）**：20 道纯 Lua 小题，覆盖 table / metatable /
闭包 / 协程 / 模块，自带 runner 判分。约 2 周。

| task | 内容 |
|---|---|
| t01 | `lua_State` 嵌入 + 栈操作 + 错误处理 |
| t02 | C 函数注册 + userdata + metatable（向 Lua 暴露 vec3 / mat4） |
| t03 | 用 Lua 描述材质与场景 |
| t04 | **用 Lua 声明 render graph**（接 P5） |
| t05 | 热重载 + 沙箱 + 协程调度 |

t04 是「一举多得」真正兑现的地方。判分除了 Lua 侧断言，还有 C 侧的
**栈平衡检查**（每次调用前后 `lua_gettop` 必须一致）—— 这是嵌入 Lua 最容易错的地方。

---

## P7 — D3D12 概念打通 · 35–45h（Windows-only）

不追求做出漂亮效果，只求把概念打通，并产出一份 Vulkan ↔ D3D12 对照表。

| task | 内容 |
|---|---|
| t01 | device / command queue / allocator / list / swapchain |
| t02 | root signature + PSO + descriptor heap |
| t03 | resource barrier + fence |
| t04 | 三角形 → 纹理立方体 |
| t05 | compute pipeline，复用 P3 的算法 |
| t06 | Slang 一份源码同出 SPIR-V 与 DXIL |
| t07 | 写出 Vulkan ↔ D3D12 概念对照表 |

判分用 **WARP**（微软自带的软件光栅器，作用等价于 lavapipe）。

**这个项目没有前置依赖。** 如果做到 P4 时你想把它提前来趁热做对比，随时可以调整。

---

## 判分体系

| 层 | 机制 | 确定性 |
|---|---|---|
| **L1 validation** | validation layer（含 sync validation）零 error 零 warning。只统计 `VALIDATION` 类型的消息 —— loader 的 GENERAL 絮叨和驱动相关的 `PERFORMANCE` 建议都不判分，否则判分结果会取决于跑在谁家 GPU 上 | 高 |
| **L2 readback** | GPU 结果回读，与 CPU 参考实现逐元素比对 | **100%** |
| **L3 golden** | headless 渲染结果与基准图比对 | 依赖基准来源 |

L3 的基准图**只由 CI 的 lavapipe（纯 CPU 软件渲染）生成**。这是唯一能让
NVIDIA、Apple Silicon、CI 三方对齐的方案。本机 GPU 上自动切换到宽松容差，
只作冒烟检查。

```bash
ctest --preset win-msvc -L l2 # 高级用法：直接筛判分层（macOS 换 mac-arm64）
python rwb.py test p01        # 日常：自动识别平台并跑项目 1
python rwb.py test p01-t03    # 日常：只构建并验收某一题
```

## 已知的取舍

1. **官方基线削弱了「完全自己写」的叙事。** 每题对完答案后切回参考实现，
   代价是最终代码不是逐行自己的。`mine/*` tag 保留了你每一题的实现；
   P5/P6 不给强基线作为补偿。详见 [02-git-workflow.md](02-git-workflow.md)。

2. **MoltenVK 不是完整 Vulkan。** P3-t02（subgroup）和 P2-t08（MSAA）在 Mac 上
   需要降级路径。详见 [01-setup-macos.md](01-setup-macos.md)。

3. **P1 的 35–45 小时很难熬。** 前 3–4 周可能只有一个三角形。
   这是「先细后粗」的代价，也是它值钱的原因。

4. **GPU-driven 渲染不在计划内。** 原本 P5 之后还有一档（compute 剔除、
   indirect draw、bindless），砍掉了。Compute 因此从「GPU-driven 的一部分」
   变回独立的 P3。
