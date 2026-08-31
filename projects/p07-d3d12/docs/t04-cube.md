# P07-t04 — 从三角形到纹理立方体

## 目标

用 upload heap 把 vertex/index 数据和一张最小 checkerboard 纹理传进 default heap，创建 SRV，录制
indexed draw，并用常量缓冲更新旋转 MVP。上传资源必须活到 copy 完成，纹理状态必须从
`COPY_DEST` 转到 shader resource。

## 验收与调试入口

```powershell
python rwb.py test p07-t04
python rwb.py run p07 --stage 4
```

通过信号是 WARP 连续绘制两帧、立方体资源摘要有效、128×128 RGBA8 回读含足够的非背景像素，且
debug layer 干净。全黑先看 RTV clear/draw，
有几何无纹理看 row pitch、SRV format 和 GPU descriptor handle，device removed 立即查看 DRED breadcrumbs。
需要抓帧时用 PIX 核对 copy、transition、VB/IB view 与 descriptor table。

<details><summary>Hint 1</summary>D3D12 texture upload 的 row pitch 有 256-byte 对齐要求；不要把紧密排列的 CPU
行宽直接当成 footprint row pitch。</details>
