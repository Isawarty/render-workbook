# P06-t01 — lua_State 嵌入、栈与错误

实现 `LuaVm` 对 `lua_State` 的唯一所有权，用 protected call 执行字符串和文件，并让错误带
traceback。每个公共调用返回时必须恢复调用前栈顶；`LuaStackGuard` 用于跨异常路径恢复栈。

非目标：本题不注册引擎类型，不做场景解析，也不开放 raw `lua_State*` 的所有权转移。

## 验收

```bash
python3 rwb.py test p06-t01
```

两个用例分别验证成功/失败执行后的栈顶、chunk 名与 traceback，以及 guard 在临时值存在时恢复
原高度。通过信号是 CTest 的 `p06-t01 Passed`。

## 调试入口

- 在 `LuaVm::runString`、`checkedCall` 前后观察 `lua_gettop`。
- `lua_pcall` 的错误对象位于栈顶；先把它转成 C++ 字符串，再弹栈并抛异常。
- 编译错误和运行错误都属于 CPU/Lua 边界；本题没有 Vulkan 路径。
- 若错误只有原始消息没有调用链，检查 message handler 是否插在被调函数下面。
