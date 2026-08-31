# P07-t02 — Root Signature、PSO 与 Descriptor Heap

## 目标

建立一份含 CBV 与 SRV descriptor table 的 root signature，创建 shader-visible CBV/SRV/UAV heap，
再把 shader bytecode、input layout、raster/depth/blend 状态打包成 graphics PSO。对照 Vulkan 时要分清：
root signature 更接近 pipeline layout，descriptor table 才承担 descriptor set 的绑定角色。
本题的最小三角形由 `SV_VertexID` 生成，因此 input layout 合法地为空；真实 vertex/index layout 留到 t04。

## 验收与调试入口

```powershell
python rwb.py test p07-t02
```

通过信号是 root signature、PSO、shader-visible heap 均有效且 info queue 干净。序列化失败先打印
`ID3DBlob` error；PSO 创建失败核对 input semantic、RTV/DSV format 与 shader signature；绘制为空再看
descriptor handle 和 root parameter index。用 PIX 时检查 Draw 前实际绑定的 root signature、PSO 和 heap。

<details><summary>Hint 1</summary>descriptor heap 与 descriptor table 是两层对象：先把 SRV 写入 CPU handle，
录制时绑定 heap，再把对应 GPU handle 设进 root table。</details>
