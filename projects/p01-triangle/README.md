# P1 — 三角形

**预估工时 35–45 小时。这是全套里唯一一次从零写 Vulkan 初始化。**

前 3–4 周你可能只会得到一个三角形。这是「先细后粗」策略必然的代价，
也正是它值钱的原因：之后每一个项目都建立在你亲手写过的这套代码上。

## 9 个 task

| task | 内容 | 任务书 |
|---|---|---|
| t01 | instance / validation layer / debug messenger | [docs/t01-instance.md](docs/t01-instance.md) |
| t02 | surface / physical device / queue family / logical device | [docs/t02-device.md](docs/t02-device.md) |
| t03 | swapchain / image views | [docs/t03-swapchain.md](docs/t03-swapchain.md) |
| t04 | render pass / framebuffer | [docs/t04-renderpass.md](docs/t04-renderpass.md) |
| t05 | shader module / graphics pipeline | [docs/t05-pipeline.md](docs/t05-pipeline.md) |
| t06 | command pool / command buffer / 录制 | [docs/t06-commands.md](docs/t06-commands.md) |
| t07 | **semaphore / fence / 提交 / 呈现（最难）** | [docs/t07-sync.md](docs/t07-sync.md) |
| t08 | swapchain 重建 | [docs/t08-resize.md](docs/t08-resize.md) |
| t09 | 多帧并行 | [docs/t09-frames-in-flight.md](docs/t09-frames-in-flight.md) |

## 代码组织

```
src/
  TriangleApp.h        类声明。贯穿 9 个 task，不用改
  TriangleApp.cpp      构造/析构/initUpTo/主循环。框架提供
  Capture.cpp          截图回读。框架提供，P2 学完再回来看
  main.cpp             入口。框架提供
  steps/
    01_instance.cpp    <- t01 在这里
    02_device.cpp      <- t02
    ...                   一个 task 一个文件
shaders/
  triangle.vert        顶点硬编码在 shader 里，P1 没有 vertex buffer
  triangle.frag
tests/
  test_p01.cpp         判分逻辑。可以读，能帮你理解验收标准
```

`initUpTo(Stage)` 把初始化切成 9 段，所以每个 task 的测试只驱动到自己那一段 ——
t03 的测试不会因为 t05 还没写而失败。

## 单个 task 的工作流

```bash
# 1. 领题（自带前面所有 task 的官方基线）
git checkout -B work start/p01-t03

# 2. 填 src/steps/03_swapchain.cpp 里的 TODO

# 3. 判分（自动识别 Windows / macOS，首次运行会自动 configure）
python rwb.py test p01-t03

# 4. 对答案（可以选择不看）
git diff done/p01-t03 -- projects/p01-triangle/src/steps/03_swapchain.cpp

# 5. 存档你自己的实现，然后进下一题
git commit -am "p01-t03: my impl" && git tag mine/p01-t03
git checkout -B work start/p01-t04
```

完整说明见 [docs/02-git-workflow.md](../../docs/02-git-workflow.md)。

## `preset`、`build` 和 `test` 分别是什么

`python rwb.py test p01-t03` 按顺序编排三件不同的事：

```text
cmake --preset <preset>                 读取 CMakeLists，生成 build/<preset>
cmake --build --preset <preset> ...     调用 Ninja/MSVC 编译测试程序
ctest --preset <preset> ...             运行已经编译出来的测试程序
```

`--preset` 不是一个独立阶段，而是“选择哪套平台配置”的参数；Windows 自动选择
`win-msvc`，Apple Silicon Mac 自动选择 `mac-arm64`。`--build` 才表示进入编译阶段。
第一次运行必须先 configure；以后只要构建缓存还在，脚本会跳过 configure，直接 build + test。

想观察而不执行，可以运行：

```bash
python rwb.py --dry-run test p01-t03
```

## 直接运行

```bash
python rwb.py run p01              # 一直画到关窗
python rwb.py run p01 --frames 60
```

macOS 上若没有 `python` 命令，把上述 `python` 写成 `python3`。想看脚本替你执行的原始
CMake/CTest 命令，可以运行 `python rwb.py --dry-run test p01-t03`。

骨架状态下它会以退出码 10 退出，并告诉你下一个没实现的函数是哪个。
那不是 bug，是进度提示。

## 判分说明

每个 task 的验收由两到三层组成：

- **L1 validation** — 全程 validation layer 零 error 零 warning（含 sync validation）。
  这一层能抓住画面看起来正确但同步是错的情况 —— 那是 Vulkan 里最贵的一类 bug。
- **结构断言** — 该建的对象建出来了、数量关系对。
- **L3 golden**（仅 t07/t09）— 截图与基准图比对。
  基准图由 CI 的 lavapipe 生成；本地没有基准时该条会 SKIP 而不是 FAIL。

## 完成标志

```bash
python rwb.py test p01     # 9 个 task 全绿
```

之后 `engine/rhi/` 会在 P2 里由你这份代码提炼而成。
