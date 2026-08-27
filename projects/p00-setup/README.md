# P0 — 环境与骨架

**预估工时 8–12 小时**（其中大部分是装工具链和等下载）。

这个项目**没有挖空题**。它唯一的职责是让你的三台机器都确认：工具链装对了、
Vulkan 跑得起来、shader 编得出来、测试框架能跑。

## 先做

按你的平台读对应文档，把工具链装好：

- [docs/01-setup-windows.md](../../docs/01-setup-windows.md)
- [docs/01-setup-macos.md](../../docs/01-setup-macos.md)

## 然后

```bash
cmake --preset win-msvc          # macOS: mac-arm64
cmake --build build/win-msvc
./build/win-msvc/projects/p00-setup/p00_setup
```

期望看到类似输出：

```
volk 加载            : OK
instance API 版本    : 1.4.357
validation layer     : 可用

物理设备 (2 个):
  - NVIDIA GeForce RTX 5070
      API      : 1.4.341
      类型     : 独立显卡
      队列能力 : graphics compute

shader 编译链路      : OK (probe.comp.spv, 225 words)
```

**留意物理设备的枚举顺序。** 很多机器上核显排在独显前面 —— 这正是 P1-t02
里「不要无脑取第 0 个设备」的由来。

## 判分

```bash
ctest --preset win-msvc -L p00
```

七条检查：volk 能加载 loader、能建 instance、至少一个设备同时支持
graphics+compute、至少一个设备支持 Vulkan 1.2、validation layer 可用、
构建期 GLSL→SPIR-V 链路可用、CTest 的目录注入正常。

任何一条红了都不要往下走 —— P1 会以更难懂的方式失败。

## 关于 Vulkan SDK

本仓库把 Vulkan-Headers / volk / glslang 都纳入了 CMake FetchContent，
所以**没装 SDK 也能完整编译**。

SDK 只提供一样东西：**validation layer**。而 L1 是整套判分的第一层，
所以实际上你必须装。
