# Windows 环境搭建

已在 Windows 11 (26200) + RTX 5070 + VS 18 Community 上实测通过。

## 1. C++ 工具链

需要 **MSVC + Windows SDK**。只装了 Visual Studio 而没勾选
「使用 C++ 的桌面开发」工作负载是不够的 —— `cl.exe` 会在，但没有
`vcvarsall.bat` 和 Windows SDK，链接会失败。

```powershell
# 已装 VS 但缺工作负载时（把 installPath 换成你的实际路径）
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify `
  --installPath "C:\Program Files\Microsoft Visual Studio\18\Community" `
  --add Microsoft.VisualStudio.Workload.NativeDesktop `
  --includeRecommended --quiet --norestart
```

约 7 GB。装之前**关掉 Visual Studio / Rider / 任何 MSBuild 进程**，
否则安装器会以退出码 8006（`VSProcessesRunning`）静默失败。

退出码 3010 表示「装好了，建议重启」。实测不重启也能直接用。

验证：

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
where cl
```

## 2. Python + CMake + Ninja

统一课程入口需要 Python ≥ 3.9。用 pip 安装 CMake 与 Ninja，全部落在用户目录，不动系统：

```powershell
python --version
python -m pip install cmake ninja
```

需要 CMake ≥ 3.24（FetchContent 的 `URL_HASH` 行为在 3.24 才稳定）。

## 3. Vulkan SDK 与 Slang

Vulkan 部分只为 **validation layer** 而装 —— 编译本身不需要它（headers/volk/glslang
都走 FetchContent）。但 L1 判分整层建立在 validation layer 上。P4-t07 与 P7-t06 还需要
`slangc`；安装 Vulkan SDK 时保留 Slang 组件，或使用 Slang 官方独立发行包。

```powershell
winget install --id KhronosGroup.VulkanSDK --exact --source winget
```

`--source winget` 不能省：如果你的机器上有第三方 winget 源，
不指定源会因包名歧义而失败。

```powershell
slangc -version
```

## 4. P7 的 D3D12/WARP 组件

「使用 C++ 的桌面开发」工作负载中的 Windows SDK 提供 D3D12 headers 与 import libraries。
P7 使用的 **WARP software adapter** 和 D3D12 debug layer 属于 Windows 的 **Graphics Tools**
可选功能；在“系统 → 可选功能”中安装它。P7 不需要独立显卡，也不会退回硬件 adapter 判分。

## 5. 构建

```powershell
python rwb.py doctor
python rwb.py test p00
```

统一入口会自动选择 `win-msvc`，首次运行依次完成 CMake configure、build 和 P0 环境测试。
它仍需在「x64 Native Tools Command Prompt」或已调用 `vcvars64.bat` 的终端中运行。
如需学习或排查底层命令，运行 `python rwb.py --dry-run test p00`。

首次配置会下载全部依赖（约 20 MB，按 SHA256 校验），耗时 1–2 分钟。

## 常见问题

**`cl.exe` 找不到 / 链接报 `LNK1104: kernel32.lib`**
没在 MSVC 开发环境里。要么在「x64 Native Tools Command Prompt」里跑，
要么先 `call vcvars64.bat`。

**Git Bash 里 `cl /nologo` 报奇怪的路径错误**
MSYS 会把 `/nologo` 当成路径转换成 `\nologo`。把命令写进 `.bat` 文件里执行，
或设 `MSYS_NO_PATHCONV=1`。

**`ctest` 报 validation layer 不可用**
Vulkan SDK 没装，或装完没重开终端（`VULKAN_SDK` 环境变量没生效）。

**核显被选中而不是独显**
这是 P1-t02 的考点，不是环境问题。先跑 `p00_setup` 看枚举顺序。
