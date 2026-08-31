# P5-t04 — 生命周期与 transient alias

## 目标与实现

用拓扑位置计算每个使用中资源的 `[firstUse,lastUse]`。只有 transient、资源类型相同、
`compatibilityKey` 相同且区间严格不重叠时，才复用同一个 `physicalSlot`；imported resource 永不复用。
slot 容量记录其中最大 `sizeBytes`。`TransientImagePool` 为每个兼容 slot 创建一份 VMA allocation，
后续不重叠 image 用 `vmaCreateAliasingImage` 绑定同一 backing memory。

## 验收与调试入口

```bash
python3 rwb.py test p05-t04
```

通过信号：ShadowScratch 结束后 BloomScratch 复用其 image slot，而同 key 的 buffer Histogram 使用另一
slot；64×64 的 ScratchA/ScratchB 创建不同 `VkImage`、共享同一 `VmaAllocation`，validation 为零；
DOT 同时包含资源和 pass。生命周期失败属于 CPU 计划，alias 创建失败属于 Vulkan memory requirement。

关键不变量：`lastUse < firstUse` 才算不重叠；image/buffer 不混用；compatibility key 不同不复用；
未使用资源不生成虚假的 allocation；pool 只把格式、extent、usage 完全相同且 key 相同的 image
materialize 到同一 slot，不绕过 Vulkan 的 memory requirement/alignment 检查。
