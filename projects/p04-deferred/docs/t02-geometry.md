# P4-t02 — Geometry pass 写入 MRT

## 目标与实现

加载 Khronos 的 CC0 Sci-Fi Helmet glTF：解析 70,074 个顶点/索引与 position、normal、tangent、UV，
通过 staging 上传真实 vertex/index buffer；解码并上传 base-color、metallic-roughness、normal、AO
四张贴图。geometry pass 用切线空间 TBN 还原法线，并同时写入三张 G-buffer。参考实现还把一块
代码生成的 PBR 地面追加进同一 vertex/index buffer：它写入固定材质和世界空间向上法线，为后续
shadow pass 提供明确的接影面。

这里已经不再用 fullscreen triangle 伪造几何；全屏三角形只用于后续 lighting/post pass。

## 验收

```bash
python3 rwb.py test p04-t02
```

测试要求 geometry pipeline 存在；资产确实加载为 70,074 顶点/索引和四张贴图；地面为 6 个索引；
背景仍等于 clear；
中心 albedo/metallic 已由材质覆盖；RGBA16F 法线归一化且 roughness 落在合法范围；validation 为零。
它会直接抓“退回全屏三角形”“只加载网格没采样贴图”和“法线图没有走 TBN”等假实现。

## 调试入口

通过信号是 CTest 报 `p04-t02 Passed` 且 validation error/warning 为零。先核对 accessor stride、vertex
attribute location 和 fragment output 与 `GBufferSlot` 的顺序。若整张图仍是
clear，检查 indexed draw 是否录在 geometry subpass；若只有法线异常，先查 tangent.w、TBN 乘法顺序与
normal texture 的 UNORM→[-1,1] 解码，再按 half-float 回读判断。资产数量/stride 属于 CPU 解析问题，
validation 属于 Vulkan 状态问题，中心像素/法线断言属于 GPU 输出问题；三者不要混在一起猜。
