# P5-t03 — 自动 barrier 推导

## 目标与实现

按拓扑序同时维护每个资源的上一组 `stage/access/layout`，以及最后 writer 已发布到哪些 reader scope。
上一访问或当前访问包含写、layout 改变，或最后 writer 尚未对当前 reader stage 可见时，
在消费者 pass 前生成 `Barrier`；执行时把有实体 `VkImage` 的 pass 间 barrier 转成
`vkCmdPipelineBarrier`。第一次使用仍由资源创建/render pass 做 layout transition。

## 验收与调试入口

```bash
python3 rwb.py test p05-t03
```

通过信号：HDR 从 color-attachment write/general 到 compute read-write/general 的 source、destination
mask 精确匹配。CPU 断言红先看状态机；t05 validation 红再看实体 image 绑定和 render-pass 边界。
同步错误值得开 sync validation 或抓帧，不能靠“画面看起来正常”放过。

关键不变量：只有已覆盖同一 writer scope 的 read→read 才无需 barrier；多消费者 fan-out 不能在第一
个 reader 后丢掉 writer；写 hazard 不能漏；source 来自上一实际 use或最后 writer，
destination 来自当前 use；subpass 管理的资源仍保留推导快照，但不在 render pass 内注入非法 barrier。
