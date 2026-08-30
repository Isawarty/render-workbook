# P4-t06 — HDR、Compute Bloom 与 Tonemap

## 目标与实现

lighting 输出改为 RGBA16F HDR image；compute pass 做亮度阈值与 5×5 可分权重扩散；随后通过 barrier
交给独立 post render pass，用 ACES 近似曲线和 gamma 写回 swapchain。

## 验收

```bash
python3 rwb.py test p04-t06
```

HDR alpha 保留 light-space PCF visibility，供判分检查正确阴影足迹；tonemap 只消费 RGB，窗口输出仍为
opaque。这样 t06 的 GLSL HDR 路径若重新绕过世界坐标重建或翻转 shadow UV，会在 bloom 之前直接失败。

测试检查 HDR/bloom 格式、compute/post pipeline、compute-write → fragment/transfer-read barrier，原始 half-float
bloom 任一 RGB 通道中必须有大于 0.05 的能量，最终真实 PBR 镜面高光必须明亮；validation 为零。

## 调试入口

通过信号是 CTest 报 `p04-t06 Passed`，HDR 高光、PCF alpha、bloom 能量、barrier 和 validation 同时通过。
若最终画面正常但 bloom 回读全零，说明 tonemap 的基础 HDR 掩盖了错误；直接检查 storage image。
阈值必须在线性 HDR 空间应用，回读后恢复 GENERAL layout 时仍要保留后续 shader read/write 可见性。
非有限 half-float 属于 shader 数值问题，validation 属于 layout/barrier 问题，只有最终输出异常时才检查
ACES/gamma；RenderDoc 中先看 HDR，再看 bloom，最后看 post pass。
