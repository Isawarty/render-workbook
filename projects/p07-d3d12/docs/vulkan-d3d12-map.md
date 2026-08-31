# Vulkan ↔ Direct3D 12 概念对照表

> 这是 P07-t07 的待完成资产。骨架先列出必须覆盖的轴；学习者需要补全“责任差异”和“常见误区”。

| Vulkan | Direct3D 12 | 责任差异 | 常见误区 |
|---|---|---|---|
| `VkInstance` | `IDXGIFactory` + debug layer | TODO | TODO |
| `VkPhysicalDevice` | `IDXGIAdapter` | TODO | WARP 是软件 adapter，不是硬件 vendor driver |
| `VkDevice` | `ID3D12Device` | TODO | TODO |
| `VkQueue` | `ID3D12CommandQueue` | TODO | TODO |
| `VkCommandPool` | `ID3D12CommandAllocator` | TODO | TODO |
| `VkCommandBuffer` | `ID3D12GraphicsCommandList` | TODO | TODO |
| `VkPipelineLayout` | Root Signature | TODO | TODO |
| `VkDescriptorSet` | Descriptor Table + Descriptor Heap | TODO | TODO |
| `VkPipeline` | `ID3D12PipelineState` | TODO | TODO |
| `vkCmdPipelineBarrier2` | `ResourceBarrier` | TODO | TODO |
| `VkFence` / timeline semaphore | `ID3D12Fence` timeline value | TODO | TODO |
| `VkSwapchainKHR` | `IDXGISwapChain` | TODO | TODO |
| lavapipe | WARP | TODO | 两者都可作软件判分环境，但 API 与输出基线不可混用 |
