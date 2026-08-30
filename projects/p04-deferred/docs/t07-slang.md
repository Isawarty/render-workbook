# P4-t07 — Shader 链迁移到 Slang

## 目标与实现

用 `slangc` 把 geometry、lighting、IBL、shadow、bloom、tonemap 的 10 个入口编译为 Vulkan 1.2 可用的
SPIR-V。stage 7 在创建任何 pipeline 前切换目录，保证运行时实际加载 Slang 产物，而不只是生成文件。
最终窗口启动即捕获鼠标，用鼠标转向、`WASD` 漫游、`Q/E` 升降、`Shift` 加速、`R` 复位；第一次
按 `Esc` 释放鼠标，第二次关闭，左键可重新捕获。

## 环境与验收

```bash
slangc -version
python3 rwb.py test p04-t07
```

测试检查 Slang 后端标志和六类 pipeline，精确回读 Slang 生成的 IBL 与 HDR，并确认最终真实 PBR
高光区域和 HDR alpha 中的 PCF visibility 足迹。GLSL 与 Slang 必须共享 binding 6 的 lighting UBO、
世界坐标重建、固定 light VP、
程序化天空和地面材质契约。shadow PCF 与 bloom threshold 会放大不同前端的浮点差异，因此不把
跨编译器逐像素相等当作语义门槛；每个资源阶段由独立断言负责。validation 必须为零。

## 调试入口

通过信号是 CTest 报 `p04-t07 Passed`，Slang 的 G-buffer、IBL、HDR/PCF alpha、bloom、最终输出和
validation 全部通过。`SV_VertexID` 生成的 SPIR-V 会声明 draw-parameters capability；input attachment 也可能声明 storage-image
read capability，逻辑设备必须显式开启设备支持的对应 feature。不要用反向 edge 的 `smoothstep`：那是
未定义行为，会让 GLSL 与 Slang 输出分叉。编译失败先看 `slangc` capability/diagnostic；validation 失败
看 descriptor 与 feature；资源回读正确但画面分叉时，再逐项对照矩阵乘法和数组初始化。
