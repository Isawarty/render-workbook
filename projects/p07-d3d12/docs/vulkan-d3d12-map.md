# Vulkan ↔ Direct3D 12 概念对照表

这张表用于迁移心智模型，不表示两套 API 的对象可以逐个机械替换。重点是：谁拥有资源、谁声明绑定、
谁负责状态转换，以及 CPU 何时可以安全复用提交对象。

| Vulkan | Direct3D 12 | 责任差异 | 常见误区 |
|---|---|---|---|
| `VkInstance` | `IDXGIFactory` + debug layer | Vulkan instance 汇集扩展、layer 和全局入口；D3D12 的 adapter/swapchain 枚举由 DXGI factory 负责，debug layer 则在创建设备前单独启用。 | `IDXGIFactory` 不是 `ID3D12Device`，开启 DXGI debug 也不等于已经开启 D3D12 debug layer。 |
| `VkPhysicalDevice` | `IDXGIAdapter` | 两者都代表创建设备前选中的实现；Vulkan 查询 queue family 与 feature，DXGI 查询 adapter 属性，再由 `D3D12CreateDevice` 验证 feature level。 | WARP 是软件 adapter，不是硬件 vendor driver；adapter 名称也不能替代 capability 查询。 |
| `VkDevice` | `ID3D12Device` | 都是资源、pipeline 和同步对象的创建入口。Vulkan 在创建时显式启用 feature/extension；D3D12 以 feature level 建立设备，再用 `CheckFeatureSupport` 查询细项。 | device 不负责自动追踪资源状态，也不会替应用等待 GPU。 |
| `VkQueue` | `ID3D12CommandQueue` | Vulkan 从声明过的 queue family 取得 queue；D3D12 创建 DIRECT/COMPUTE/COPY 类型的 queue。两边都由应用决定提交顺序和跨队列同步。 | queue 类型相似不代表能力集合完全相同；向 queue 提交后也不能立刻复用其 allocator 或资源。 |
| `VkCommandPool` | `ID3D12CommandAllocator` | 都保存命令记录所需的后端内存，通常按 frame-in-flight 分配。D3D12 allocator 只能在引用它的 GPU 工作完成后 `Reset`。 | `ID3D12CommandAllocator` 不等于 command list；只重置 list 而提前重置 allocator 会产生 GPU/CPU 生命周期错误。 |
| `VkCommandBuffer` | `ID3D12GraphicsCommandList` | 都记录 barrier、draw、dispatch 与 copy。D3D12 list 在提交前必须 `Close`，完成后以安全的 allocator 和可选初始 PSO `Reset` 再录制。 | `ExecuteCommandLists` 不代表执行已完成；关闭 list 也不会等待 GPU。 |
| `VkPipelineLayout` | Root Signature | 两者都定义 shader 可见的资源接口。Vulkan layout 组合 descriptor set layout 与 push constant range；Root Signature 可组合 Descriptor Table、root descriptor、root constants 和 static sampler。 | Root Signature 不是 descriptor 数据本身；过多 root 参数还会增加 root signature 成本。 |
| `VkDescriptorSet` | Descriptor Table + Descriptor Heap | Vulkan 从 pool 分配 set 并写 descriptor；D3D12 把 descriptor 写进 heap，Descriptor Table 只是指向一段 GPU-visible handles。提交期间 shader-visible heap 和其中的 descriptor 必须保持有效。 | CPU-only heap 不能直接供 shader table 使用；切换 descriptor heap 会使之前表绑定的含义失效，需要重新绑定。 |
| `VkPipeline` | `ID3D12PipelineState` | 两者都把 shader 和大部分固定功能状态编译成不可变对象。Vulkan pipeline 关联 pipeline layout；D3D12 PSO 与兼容 Root Signature 配合，并在 command list 上分别绑定。 | PSO 不包含 vertex/index buffer、descriptor 内容或当前 render target 资源。 |
| `vkCmdPipelineBarrier2` | `ResourceBarrier` | Vulkan 显式给出 stage/access mask 与 image layout；传统 D3D12 barrier 主要声明资源的 before/after state，并用 UAV barrier 表达未改变 state 的 UAV 访问顺序。两者都要选对 subresource。 | `PRESENT → RENDER_TARGET → PRESENT` 不能省；UAV 写后 copy 也不能只靠提交顺序猜测可见性。 |
| `VkFence` / timeline semaphore | `ID3D12Fence` timeline value | Vulkan binary fence 常表示一次提交的 host completion，timeline semaphore 使用单调值；`ID3D12Fence` 本身就是 64-bit timeline，由 queue `Signal`，CPU 或其他 queue 等待指定值。 | 等待“某个 fence 对象”还不够，必须等待正确的 timeline value；每帧复用 allocator 前要检查该帧记录的值。 |
| `VkSwapchainKHR` | `IDXGISwapChain` | 两者都管理可呈现图像并返回当前 image/back-buffer index。应用仍负责每张图像的 in-flight 状态，以及渲染和 `PRESENT` 状态之间的转换。 | flip-discard swapchain 在 `Present` 后不保证保留旧像素；要稳定回读，应在 Present 前复制到 readback resource。 |
| lavapipe | WARP | 都能提供不依赖独显的 CPU 软件实现，适合 CI 中验证真实 API 路径。lavapipe 实现 Vulkan，WARP 实现 Direct3D，因此各自仍需对应 loader、同步和 shader 产物。 | 两者都可作软件判分环境，但 API、支持特性与像素基线不可混用；WARP 通过不代表 Vulkan 路径通过，反之亦然。 |

## 从 Vulkan 迁移到 D3D12 时的最小检查单

1. 先确定 queue 类型与 frame-in-flight 数，再为每个在途 frame 保存 allocator 和 fence value。
2. 把 `VkPipelineLayout` 的 set/push-constant 契约重画为 Root Signature，明确哪些数据进入
   shader-visible Descriptor Heap，哪些适合 root descriptor 或 root constants。
3. 为每个资源写下当前 D3D12 state；特别检查上传、UAV 写、copy、render target 和 `PRESENT` 边界。
4. debug layer 与 `ID3D12InfoQueue` 必须在开发和 CI 中开启；“画面看起来对”不能替代无
   error/corruption 的诊断记录。
5. 在 WARP 上分别验证 draw 的 pre-Present readback 与 compute 的逐元素 readback，避免把硬件驱动、
   显示器或截图工具引入确定性判分。
