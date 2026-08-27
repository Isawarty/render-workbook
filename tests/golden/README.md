# 基准图

这个目录里的 PNG 是 L3 判分的基准。

**它们只能由 CI 的 lavapipe job 生成**（Mesa 的纯 CPU 软件光栅器）。
lavapipe 是唯一能在 NVIDIA、Apple Silicon、CI 三方产出相同像素的渲染器。

生成方式：在 GitHub Actions 上手动触发 `ci` workflow，
勾选 `update_golden`，然后把产出的 artifact 提交进这个目录。

**不要用本机 GPU 跑出来的图当基准。** 换一台机器就会全红。

本地跑 L3 测试时，框架会根据设备名自动切到宽松容差
（见 `tests/framework/ImageCompare.cpp` 的 `toleranceForDevice`）。
基准图不存在时，测试会 SKIP 而不是 FAIL —— 因为「基准未建立」
不等于「你的实现错了」。
