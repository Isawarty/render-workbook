# P06-t00 — Lua 语言速通

完成 `language/exercises/` 中 20 道独立小题，覆盖值与类型、字符串、循环、数组与 map、
多返回值、高阶函数、闭包、迭代器、错误、metatable、模块和协程。这里不接触 C API，目标是
先能读写后续场景与 Render Graph 脚本。

## 验收

```bash
python3 rwb.py test p06-t00
```

通过信号是 `P06-t00: 20 passed, 0 failed`。runner 也接受 `01`–`20` 的单题编号。

## 调试入口

- runner 输出首个失败题号、文件名和 Lua traceback；先单跑该编号。
- 在 exercise 内用 `print` / `type` / `getmetatable` 观察值，不要改 runner 的期望值。
- 协程题同时检查 `resume` 返回值和 `coroutine.status`；死协程再次 resume 是错误路径。
- 这是纯 Lua 逻辑，失败不涉及 Vulkan、C++ ABI 或 GPU。
