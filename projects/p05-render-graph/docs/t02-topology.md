# P5-t02 — 有向图与拓扑排序

## 目标与实现

为每个资源追踪最后 writer 与尚未被覆盖的 readers，生成 RAW、WAR、WAW 边，再合并显式依赖。
用最小 pass id 优先的 Kahn 算法得到可重复的拓扑序；剩余入度非零就是环，编译必须失败。

## 验收与调试入口

```bash
python3 rwb.py test p05-t02
```

通过信号：Geometry 写 G-buffer 自动成为 Lighting 的依赖，显式双向依赖被判成环。建议断点放在
dependency 构建和 ready queue 出队处；顺序错误是 CPU 图逻辑问题，不要去改 Vulkan barrier。

关键不变量：连续 read 不互相制造边；write 等待此前 writer 和所有 readers；依赖去重；合法图的
输出顺序在不同 STL/平台上保持一致。
