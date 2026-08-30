# P4-t03 — Cook–Torrance lighting pass

## 目标与实现

为第二 subpass 建立四个 input attachment descriptor 和一份每帧 lighting UBO。fullscreen fragment
shader 读取三张 G-buffer 与 depth，用 `inverseViewProjection` 从 Vulkan NDC 深度重建世界坐标，
再以真实世界空间观察向量执行 GGX NDF、Schlick-GGX geometry 和 Schlick Fresnel。没有几何的像素
输出程序化天空渐变，为头盔和地面提供稳定的空间背景。

## 验收

```bash
python3 rwb.py test p04-t03
```

测试要求 geometry/lighting pipeline、descriptor layout 与 mapped lighting UBO 均有效，最终帧尺寸正确，
头盔中心亮度显著高于背景，天空顶部与地平线颜色不同，alpha 为 255，且 validation 为零。

## 调试入口

通过信号是 CTest 报 `p04-t03 Passed`，天空层次、头盔亮度和 validation 同时满足。descriptor binding
0–3 必须与 render-pass input attachment index 一致，binding 6 是 lighting UBO。
黑屏时先查 `uv * 2 - 1`、Vulkan `[0,1]` depth 和齐次除法，再查 subpass dependency 的
color/depth write → fragment input read。矩阵往返失败属于 CPU/坐标契约；descriptor/validation 报错属于
Vulkan 状态；只有最终亮度不对时才进入 Cook–Torrance 数值调试。
