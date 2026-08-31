# P07-t06 — Slang 同源输出 DXIL 与 SPIR-V

## 目标

让 `shaders/shared.slang` 的同一个 `computeMain` 同时编译为 Shader Model 6 DXIL 与 SPIR-V 1.5。
这题验证的是共享入口、资源 binding 与产物格式，不要求两个后端二进制相同。

## 验收与调试入口

```powershell
slangc -version
cmake --build build/win-msvc --target p07_slang_shaders
python rwb.py test p07-t06
```

通过信号是 `shared.dxil` 具有 DXBC container magic，`shared.spv` 具有 SPIR-V magic。编译失败先看
target/profile/entry diagnostic；D3D12 运行失败再核对 register 与 root signature；Vulkan 侧消费留作
扩展练习，本题不以“文件存在”代替格式校验。

<details><summary>Hint 1</summary>同一资源在源码中保留显式 `b0/t0/t1/u0` register，并为 Vulkan 指定互不
重叠的 `vk::binding`；分别检查 DXIL reflection 和 SPIR-V decoration 是否保持契约。</details>
