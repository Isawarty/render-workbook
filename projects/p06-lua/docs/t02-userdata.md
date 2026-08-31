# P06-t02 — C 函数、userdata 与 metatable

向 Lua 注册 `vec3` 与 `mat4`。对象使用 full userdata 保存 C++ 值；vec3 支持字段读取、加减、
标量乘、dot、length 和字符串化，mat4 支持 identity、translation、矩阵乘法及变换 vec3。

非目标：不实现完整 GLM 绑定、任意字段写入、透视矩阵或脚本侧继承。

## 验收

```bash
python3 rwb.py test p06-t02
```

测试从 Lua 构造并运算，再由 C++ 读取 userdata 的精确分量；最终栈顶必须回到零。

## 调试入口

- 在 `newVec3` / `matMul` 处检查参数类型和 userdata metatable 名 `rwb.vec3` / `rwb.mat4`。
- `__mul` 要区分 `number * vec3`、`vec3 * number` 和 `mat4 * vec3`。
- 平移矩阵沿用列主序，平移在索引 12–14；错误属于 CPU 数据布局，不用抓帧。
