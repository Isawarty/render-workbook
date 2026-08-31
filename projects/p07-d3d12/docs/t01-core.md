# P07-t01 — Device、Queue、Command List 与 WARP Swapchain

## 目标

从 Windows SDK 原生 API 建立最小 D3D12 提交链：开启 debug layer，创建 DXGI factory，显式枚举
WARP adapter，创建 device、direct command queue、每帧 allocator、可复用 command list 与双缓冲
swapchain。完成后应能把这些对象逐项映射回 Vulkan，而不是只会运行微软 sample。

## 验收与调试入口

```powershell
python rwb.py test p07-t01
```

通过信号是 `p07-t01 Passed`，状态摘要确认使用 WARP、两份 frame resources、完整 queue/list/swapchain，
且 `ID3D12InfoQueue` 没有 error/corruption。推荐断点在 adapter 枚举、`D3D12CreateDevice`、
`CreateCommandList` 和 `CreateSwapChainForHwnd`；设备创建失败先查 Windows SDK/feature level，窗口失败
查 HWND，debug layer 报错再查对象参数和生命周期。不要回退到硬件 adapter 来让测试偶然通过。

## 分层提示

<details><summary>Hint 1</summary>用 `EnumWarpAdapter`，请求 `D3D_FEATURE_LEVEL_12_0`；allocator 数量与
swapchain buffer 数一致。</details>
<details><summary>Hint 2</summary>新建 command list 默认处于 recording 状态，初始化结束前要关闭；每帧先等对应
fence，再 reset allocator 和 list。</details>
