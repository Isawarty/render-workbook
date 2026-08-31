# P06-t05 — 热重载、沙箱与协程调度

`LuaSandbox` 只开放 base/table/string/math/utf8/coroutine，并移除 `io`、`os`、`debug`、`package`、
`dofile` 与 `loadfile`。count hook 限制单次执行指令数。协程 scheduler 每 tick 只推进到下一个
yield；热重载先在候选 VM 执行，成功后原子替换，失败时保留上一代状态。

非目标：这不是针对敌对代码的进程级安全边界，不限制原生内存总量，也不提供多线程调度。

## 验收

```bash
python3 rwb.py test p06-t05
```

测试覆盖危险库不可见、死循环预算中断、三次 yield 的逐 tick 顺序，以及无效热重载不会覆盖上一代。

## 调试入口

- runaway 脚本应报告 `instruction budget exceeded`，不是 hang 或进程消失。
- 在 `lua_resume` 后同时检查 status 和 results；yield 的返回值要及时弹栈。
- 热重载失败先看 `lastError()` 与 generation；generation 不增加证明旧 VM 仍有效。
- 这些都是 CPU/runtime 失败，不涉及 Vulkan validation。
