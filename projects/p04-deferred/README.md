# P4 — 延迟渲染 + PBR

P4 把前面分散学过的 image、render pass、descriptor、compute 与同步组织成一条可扩展的
延迟渲染管线。P5 才会把它抽象成通用 Render Graph；P4 内部先保留清楚、具体的 G-buffer
所有权和生命周期，避免在理解数据流之前先藏进框架。

t01–t07 已完整交付，包含全套骨架（`start/*`）与参考实现（`done/*`）标签：

| task | 内容 | 判分 |
|---|---|---|
| t01 | 三张颜色附件 + 深度附件；两子通道 render pass；resize 与清理 | L1 + L2 raw readback |
| t02 | CC0 Sci-Fi Helmet glTF + PBR 地面写 MRT | L1 + 资产/G-buffer 结构断言 |
| t03 | input attachment + Cook-Torrance + 程序化天空 | L1 + final-frame readback |
| t04 | compute 生成轻量 irradiance / prefilter / BRDF LUT | L1 + buffer 精确回读 |
| t05 | 固定光源 shadow map + 世界坐标重建 + 3×3 PCF | L1 + 矩阵/visibility 回读 |
| t06 | RGBA16F HDR + compute bloom + ACES tonemap | L1 + bloom raw readback |
| t07 | 完整 shader 链从 GLSL 迁移到 Slang | L1 + Slang 独立数据/画面回读 |

## t01 的附件契约

| attachment | format | 通道语义 | pass 结束后的 layout |
|---|---|---|---|
| Albedo + Metallic | `R8G8B8A8_UNORM` | RGB = albedo，A = metallic | `SHADER_READ_ONLY_OPTIMAL` |
| Normal + Roughness | `R16G16B16A16_SFLOAT` | RGB = normal，A = roughness | `SHADER_READ_ONLY_OPTIMAL` |
| Emissive + AO | `R8G8B8A8_UNORM` | RGB = emissive，A = AO | `SHADER_READ_ONLY_OPTIMAL` |
| Depth | 设备支持的最佳深度格式 | depth | `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |

几何子通道写前三张颜色附件和深度；光照子通道通过 input attachment 消费它们。t01 只执行
clear，t02 开始加载 70,074 顶点的真实 glTF 网格与 base-color、metallic-roughness、normal、AO
贴图写 G-buffer；代码生成的地面也写入同一组 MRT 并接收阴影。t03 起输出 PBR 光照和程序化天空，
t06/t07 再经过 HDR bloom 与 tonemap。全屏三角形只保留在
lighting 与 post-process 这类本来就需要覆盖屏幕的 pass 中，不再冒充场景几何。

模型来自 Khronos glTF Sample Assets 的 Sci-Fi Helmet，资产为 CC0；本仓库仅把四张贴图缩到
1024×1024。作者、来源与变更记录见 `assets/scifi-helmet/README.md`。

## 运行与判分

```bash
python3 rwb.py test p04
python3 rwb.py test p04-t07
python3 rwb.py run p04 --stage 7
```

原生窗口启动后立即捕获鼠标：鼠标控制 yaw/pitch，`W/S` 前后、`A/D` 左右、`Q/E` 下降/上升，
按住 `Shift` 三倍加速，`R` 回到默认机位。第一次按 `Esc` 释放鼠标，再按一次关闭窗口；左键点击
窗口会重新捕获。键盘移动按每帧真实时间积分，鼠标 delta 不乘帧时间；离屏判分不会读取输入，
始终使用固定默认机位。

七题都不依赖 golden image，因此缺少基准不会静默 SKIP。判分组合 GPU raw readback、CPU
语义断言、阶段间对照与 validation error/warning=0；具体标准见 `docs/tNN-*.md`。

学习者可按仓库工作流保存自己的实现：`mine/p04-t01`，并主动比较
`done/p04-t01`：

```bash
git diff done/p04-t01 -- projects/p04-deferred/src/steps/01_gbuffer.cpp
```
