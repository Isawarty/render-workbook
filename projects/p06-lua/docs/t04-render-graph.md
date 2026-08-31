# P06-t04 — 用 Lua 声明 Render Graph

脚本声明 resource、pass 及其 reads/writes，C++ 将状态名映射到 Vulkan stage/access/layout，随后
调用 P5 的真实 `rwb::rg::RenderGraph::compile()`。依赖、barrier 与 DOT 都由同一个 P5 编译器产生。

支持的状态是 `color-write`、`shader-read`、`compute-write` 和 `present`；更多状态属于扩展练习。
本题不让 Lua 回调录制 Vulkan command buffer。

## 验收

```bash
python3 rwb.py test p06-t04
```

成功资产必须编译出 Geometry → Lighting → Tonemap；失败资产真实引用不存在的 resource，错误路径
必须是 `passes[1].reads[1]`。所有调用前后检查栈平衡。

## 调试入口

- 先打印脚本返回表，再在 `addUses` 观察 resource 名到 `ResourceHandle` 的查找。
- DAG 顺序和依赖错误属于 P5 纯 CPU 图逻辑；状态映射错误在本题直接检查枚举值。
- 只有未来接入 command recording 后才需要 validation 或抓帧，本题不应以图像判解析语义。
