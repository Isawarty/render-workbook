# P6 — Lua 语言与引擎嵌入

P6 从纯 Lua 语言练习开始，最终把脚本接到 P5 的 Render Graph。本项目从接口契约与测试骨架
起步，按 `start/p06-*` / `done/p06-*` tags 逐题推进。

| task | 能力 | 判分 |
|---|---|---|
| t00 | 20 道 Lua 小题：table、闭包、metatable、模块、协程 | L2 Lua runner |
| t01 | `lua_State` RAII、protected call、traceback、栈恢复 | L2 CPU |
| t02 | vec3 / mat4 full userdata 与 metatable | L2 CPU |
| t03 | Lua 材质和场景表解析为强类型 C++ 数据 | L2 CPU |
| t04 | Lua 声明 P5 Render Graph 并编译真实 DAG | L2 CPU |
| t05 | 热重载、受限标准库、指令预算、协程调度 | L2 CPU |

统一验收：

```bash
python3 rwb.py test p06
python3 rwb.py test p06-t04
```

t00 也能逐题运行：

```bash
build/mac-arm64/projects/p06-lua/lua projects/p06-lua/language/runner.lua 12
```

Lua 5.4.9 从 lua.org 获取，CMake 以 SHA256 锁定。P06 没有 Vulkan 窗口；t04 复用
`rwb::rendergraph` 的真实编译器，但只验证确定性的 CPU 图结构。

## 代码组织

```text
language/exercises/       t00 的 20 道纯 Lua 小题
src/LuaVm.*               t01 VM 生命周期、错误与栈契约
src/LuaMath.*             t02 userdata 和 metatable
src/SceneScript.*         t03 强类型场景解析
src/LuaRenderGraph.*      t04 Lua -> P5 RenderGraph
src/ScriptRuntime.*       t05 sandbox、热重载、协程
assets/                   触发成功与失败路径的最小脚本
tests/test_p06.cpp         t01–t05 的 C++ 判分
```
