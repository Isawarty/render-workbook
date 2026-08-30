# P4-t05 — Shadow map 与 3×3 PCF

## 目标与实现

建立独立且固定的 directional-light view/projection，以及 256×256 depth-only render pass。shadow pass
只接收 `lightViewProjection * model`，不复用观察相机 MVP。lighting pass 先从 G-buffer depth 重建世界
坐标，再投影进光源空间，以光源 UV/depth 通过 immutable comparison sampler 做半径 1 的九点 PCF。
光源投影只执行一次 Vulkan Y 翻转，shadow lookup 不再翻转屏幕 UV。immutable sampler 避免 MoltenVK
portability subset 禁止 mutable comparison sampler 的验证错误。

## 验收

```bash
python3 rwb.py test p04-t05
```

测试检查 shadow image、render pass、pipeline、sampler、尺寸和 PCF 半径，并用 CPU 断言验证深度重建
往返和固定 light VP。它会移动观察相机后再次回读 shadow depth，要求两张图逐字节相同，直接抓住
shadow pass 复用 camera MVP。t05 的离屏输出把 3×3 PCF visibility 同步写入 alpha（窗口仍按 opaque
composite 显示），判分统计 alpha<200 的真实阴影足迹，并同时设置上下界：绕过世界坐标重建会过小，
额外翻转 light-space Y 会过大。把 PCF 恒设为 1 或绕过 shadow sampling 也会失败。
validation 必须为零。

## 调试入口

通过信号是 CTest 报 `p04-t05 Passed`，shadow depth 非空、固定 light VP、足迹上下界和 validation 同时
通过。若出现 comparison sampler portability 错误，把 sampler 固化进 descriptor-set layout；若阴影全黑或全白，
按 `camera depth → inverse VP → world → light VP → light UV/depth` 顺序逐段核对。receiver bias 对
`LESS_OR_EQUAL` comparison sampler 应从 receiver depth 减去；不要重新加入 `uv.y = 1 - uv.y`。
矩阵往返/固定性属于 CPU 契约，render-pass/sampler 报错属于 Vulkan 状态，足迹过小或过大属于 shader
空间转换问题；需要抓帧时分别检查 G-buffer depth、shadow depth 和 lighting alpha。
