# P4-t04 — Compute 生成 IBL 数据

## 目标与实现

compute shader 写一个结构化 buffer：1 个 irradiance、5 个粗糙度层级 prefilter，以及 4×4 BRDF LUT；
随后用 compute-write → fragment-read barrier 把它交给 lighting pass。这里是便于回读判分的轻量 IBL
数据，不加载 HDRI/cubemap；程序化天空负责可见背景，两者的职责分开。

## 验收

```bash
python3 rwb.py test p04-t04
```

测试精确回读 22 个 `vec4`，检查 irradiance、最粗 prefilter、BRDF LUT 首格和 barrier 的 stage/access；
同时确认最终光照非空且 validation 为零。

## 调试入口

通过信号是 CTest 报 `p04-t04 Passed`，22 个 `vec4` 的关键值、barrier 和最终光照同时通过。若 buffer
全零，检查 dispatch 和 storage-buffer descriptor；若数值正确但画面错误，检查 barrier 的
`COMPUTE_SHADER/SHADER_WRITE` 到 `FRAGMENT_SHADER/SHADER_READ`，不要只依赖同队列偶然执行顺序。
精确数值失败先查 compute 逻辑，validation 失败先查 descriptor/layout，数值正确但消费错误再查同步。
