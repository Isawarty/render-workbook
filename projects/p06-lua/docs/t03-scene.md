# P06-t03 — Lua 材质与场景

Lua 文件返回 `materials` 与 `entities` 两张数组表。C++ 将它们解析为 `SceneDesc`，并对字符串、
数字和 vec3 userdata 做强类型检查。错误必须包含如 `materials[1].roughness` 的字段路径。

非目标：本题不加载 glTF、不创建 GPU 资源，也不允许脚本直接持有引擎对象。

## 验收

```bash
python3 rwb.py test p06-t03
```

成功资产验证两个材质和两个实体；失败资产真实把 roughness 写成字符串并验证字段诊断。两个路径
结束后 Lua 栈都必须平衡。

## 调试入口

- 在 `loadSceneScript` 逐层观察 absolute stack index；push 新字段后负索引会移动。
- 失败消息指向解析字段，属于 Lua 数据契约，不是渲染输出差异。
- 若新增必做字段，必须同时给成功断言与实际触发错误的最小资产。
