# P7 — Direct3D 12 概念打通（Windows-only）

P7 不另造一套完整引擎，而是用一个小型 WARP 程序把 Vulkan 已学过的显式 API 概念逐项映射到
Direct3D 12。最终产物是可运行的纹理立方体、可精确回读的 SAXPY compute，以及一份
Vulkan ↔ D3D12 对照表。

> **平台边界：** D3D12 与 WARP 属于 Windows SDK。macOS/Linux 会正常配置仓库，但不会生成
> `p07_d3d12` / `p07_tests`。在 macOS 执行 `python3 rwb.py test p07` 会明确提示换到 Windows，
> 这不是可用 MoltenVK 绕过的限制。

| task | 能力 | 判分 |
|---|---|---|
| t01 | WARP device、queue、allocator/list、双缓冲 swapchain | L1 debug layer + 状态摘要 |
| t02 | root signature、graphics PSO、shader-visible descriptor heap | L1 + 结构断言 |
| t03 | resource barrier 与单调 fence timeline | L1 + fence 状态 |
| t04 | indexed triangle 扩展为上传纹理的旋转立方体 | L1 + WARP 离屏结果 |
| t05 | 复用 P3 SAXPY：dispatch、UAV barrier、readback | L1 + L2 逐元素 |
| t06 | 同一份 Slang compute entry 产出 DXIL 与 SPIR-V | 编译产物校验 |
| t07 | 写完 Vulkan ↔ D3D12 概念对照表 | 文档契约 |

## Windows 环境与统一入口

从安装了 MSVC、Windows 10/11 SDK、Ninja 和 `slangc` 的 x64 Native Tools terminal 运行：

```powershell
python rwb.py doctor
python rwb.py test p07-t01
python rwb.py test p07
python rwb.py run p07 --stage 4
```

所有 GPU 测试强制枚举 WARP adapter，不依赖机器上的 NVIDIA/AMD/Intel 驱动。D3D12 debug layer 与
`ID3D12InfoQueue` 是 L1 判分入口；测试结束时 error/corruption 消息必须为零。t04 的图像只负责
观察最终组合，资源状态与同步由独立断言负责。

## 代码组织

```text
src/D3D12App.*          生命周期、状态摘要与逐 task 初始化
src/steps/01_core.cpp   device / queue / allocator / list / swapchain
src/steps/02_pipeline.cpp root signature / PSO / descriptor heap
src/steps/03_sync.cpp   resource barrier / fence
src/steps/04_cube.cpp   upload / texture / indexed cube
src/steps/05_compute.cpp compute / readback
shaders/shared.slang    t06 的双后端 compute entry
tests/test_p07.cpp      WARP L1、L2 与文档判分
docs/                   t01–t07 任务书和最终对照表
```

## macOS 上能验证什么

在当前 macOS 机器上应做两件事：继续运行既有项目，确认 P07 没污染 Vulkan 构建；检查 launcher
能否在碰 CMake 前拒绝 P07。真正的 D3D12 编译、debug layer、WARP dispatch 和 DXIL 验收必须留到
Windows 或 Windows CI，不能把 macOS “跳过目标”写成 P07 已通过。
