# macOS 环境搭建

目标平台：Apple Silicon (M 系列)。Vulkan 在 macOS 上通过 **MoltenVK**
（Vulkan → Metal 的转译层）运行。

> 已在 Apple M4 + Vulkan SDK 1.4.341.1 + MoltenVK 上验证 P0–P4 的本地构建与测试。

## 1. 命令行工具

```bash
xcode-select --install
```

## 2. Python + CMake + Ninja

```bash
brew install python cmake ninja
# 或者和 Windows 侧保持一致：
# python3 -m pip install cmake ninja
```

需要 Python ≥ 3.9、CMake ≥ 3.24。

## 3. Vulkan SDK for macOS

**这一步在 macOS 上是必须的**，不像 Windows 那样只为 validation layer。
macOS 没有系统级 Vulkan loader，SDK 提供的 MoltenVK 就是唯一的实现。

从 https://vulkan.lunarg.com/sdk/home#mac 下载安装包，运行安装器。
安装时确保勾选：

- **System Global Installation**（把 loader 和 ICD 装到系统路径）
- **Vulkan Validation Layers**
- **Slang compiler**（P4-t07 使用 `slangc`）

装完后新开一个终端，确认：

```bash
echo $VULKAN_SDK
vulkaninfo | head -20
slangc -version
```

如果 `vulkaninfo` 找不到设备，通常是 `VK_ICD_FILENAMES` 没设。SDK 的
`setup-env.sh` 会设置它：

```bash
source "$VULKAN_SDK/setup-env.sh"
```

把这行加进 `~/.zshrc`。

## 4. 构建

```bash
python3 rwb.py doctor
python3 rwb.py test p00
```

统一入口会自动选择 `mac-arm64`，首次运行依次完成 CMake configure、build 和 P0 环境测试。
如需学习或排查底层命令，运行 `python3 rwb.py --dry-run test p00`。

## MoltenVK 不是完整的 Vulkan

这是你在 Mac 上做这门课必须知道的前提。已知限制（会影响后面的项目）：

| 限制 | 影响的项目 |
|---|---|
| 必须启用 `VK_KHR_portability_enumeration`（instance）| P1-t01 |
| 设备上报 `VK_KHR_portability_subset` 时**必须**启用 | P1-t02 |
| 无 geometry shader | 全程（本课程不用它） |
| subgroup 操作能力受限 | **P3-t02** 需要降级路径 |
| bindless / descriptor indexing 受限 | P5 之后 |
| 部分纹理格式不支持 | P2-t04 |
| MSAA 行为与桌面 GPU 有差异 | P2-t08 |

本仓库的代码全程按 **Vulkan 1.2** 编写，正是为了照顾 MoltenVK
（它对 1.3 的支持仍不完整）。

## 关于 golden image

**不要用 Mac 上跑出来的图当基准图。** Metal 的光栅化规则和浮点行为
与 NVIDIA、与 lavapipe 都不同。基准图统一由 CI 的 lavapipe job 生成。

本地跑 L3 测试时框架会自动切到宽松容差（见
`tests/framework/ImageCompare.cpp` 的 `toleranceForDevice`）。
