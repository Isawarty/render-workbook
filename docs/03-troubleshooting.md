# 现象速查表

按「你看到了什么」查，而不是按「哪个 API 出错」查。

## 编译 / 链接

| 现象 | 原因 |
|---|---|
| `LNK2019: 无法解析的外部符号 vkCreateInstance` | 链接了 `volk::volk_headers` 而不是 `volk::volk`。前者只有声明 |
| `LNK1104: kernel32.lib` | 不在 MSVC 开发环境里，或没装 Windows SDK |
| `error C2065: VK_API_VERSION_1_2 未声明` | 该文件没 `#include <volk.h>` |
| 一屏第三方库的警告 | 目标没链 `rwb::warnings`，或警告选项被写成了全局 `add_compile_options` |
| FetchContent 下载后 SHA256 不匹配 | 上游 tarball 被重新打包过。不要改哈希去将就，先确认来源 |
| Linux/CI: `fatal error: GL/gl.h: No such file` | `glfw3.h` 默认会 include OpenGL 头。`GLFW_INCLUDE_VULKAN` 只是额外加 vulkan.h，**不会**抑制它；要用 `GLFW_INCLUDE_NONE`。Windows SDK 自带该头、macOS 走 `<OpenGL/gl.h>` 分支，所以这个疏漏只在 Linux 暴露 |
| Linux/CI: `Failed to find wayland-scanner` | GLFW 3.4 默认同时构建 Wayland 和 X11 后端。本仓库在 Linux 上只用于 CI（xvfb 提供 X11），已在 `cmake/Dependencies.cmake` 里关掉 Wayland 后端 |

## 运行期：起不来

| 现象 | 原因 |
|---|---|
| `volkInitialize 失败` | 找不到 Vulkan loader。Windows 更新驱动或装 SDK；macOS 装 SDK 并 `source setup-env.sh` |
| `vkCreateInstance` 返回 `VK_ERROR_INCOMPATIBLE_DRIVER` | **macOS**：漏了 `VK_KHR_portability_enumeration` + `ENUMERATE_PORTABILITY_BIT` |
| `请求了 validation layer 但系统里没有` | Vulkan SDK 没装，或装完没重开终端 |
| `尚未实现: p01-tXX ...`，退出码 10 | 正常。这是挖空题的进度提示，不是 bug |
| 找不到 `triangle.vert.spv` | shader 没编出来。`cmake --build` 一次，或检查 `RWB_SHADER_DIR` |
| `SPIR-V magic 不对` | .spv 文件是空的或被截断，通常是构建中断留下的产物。删掉 `build/shaders/` 重编 |

## 运行期：画面不对

| 现象 | 最可能的原因 |
|---|---|
| 全黑 | render pass 的 clear color 生效了但没画进去 —— 检查 `vkCmdDraw` 的 `instanceCount` 是不是 0 |
| 三角形上下颠倒 | Vulkan NDC 的 Y 轴向下，和 OpenGL 相反 |
| 画面只占左上角四分之一 | 用了窗口尺寸而非 framebuffer 尺寸（Retina 屏） |
| 颜色偏灰/发白 | swapchain 用了 `_UNORM` 而非 `_SRGB`，shader 输出被当成已编码值 |
| 画面闪烁 | 同步问题。多半是 `renderFinished` semaphore 按帧数而不是按图像数分配 |
| resize 后卡死 | `vkResetFences` 放在了 `vkAcquireNextImageKHR` 之前，OUT_OF_DATE 的提前 return 留下了永不 signal 的 fence |
| resize 后帧率暴跌但画面正常 | 重建之后没清 `m_framebufferResized`，每帧都在重建 |

## 运行期：validation 报错

| 报错关键词 | 通常意味着 |
|---|---|
| `SYNC-HAZARD-WRITE-AFTER-READ` on swapchain image | t04 的 subpass dependency 漏了或 stage 写错 |
| `dynamic state not set` | t05 声明了动态 viewport/scissor，t06 录制时没设 |
| `VUID-vkQueueSubmit-pCommandBuffers-00071` | 提交了正在被 GPU 使用的 command buffer —— fence 等待逻辑不对 |
| `VUID-VkDeviceQueueCreateInfo-queueFamilyIndex-02802` | 同一个队列族提交了两份 `VkDeviceQueueCreateInfo`，没去重 |
| 销毁时报 `object still in use` | `cleanup` 之前漏了 `vkDeviceWaitIdle` |

## 测试

| 现象 | 说明 |
|---|---|
| golden 测试显示 SKIP | 基准图还没入库。**这不是失败**，基准由 CI 的 lavapipe job 生成 |
| golden 测试在本机 GPU 上失败但 CI 通过 | 正常的跨 GPU 差异。本机用宽松容差；如果差异很大才是真问题，看 `.diff.png` |
| 测试全绿但程序跑起来画面不对 | 报给我 —— 说明测试覆盖有洞，那是判分系统的 bug |
| L1 失败但列出的全是 `Driver "xxx_icd.json" ignored` | 那是 Vulkan **loader** 的 GENERAL 消息，不是 validation layer 报的。回调必须把 `types` 一并转交给 `ValidationLog::record()`，由框架按类型分类。见 `ctest -L framework` |
| `ctest` 报 "No tests were found" | 没配置成功，或 preset 用错了平台 |

## 平台差异

| 项 | Windows (NV) | macOS (MoltenVK) | CI (lavapipe) |
|---|---|---|---|
| golden 容差 | 宽松 | 宽松 | 严格 |
| portability 扩展 | 不需要 | **必须** | 不需要 |
| geometry shader | 有 | 无 | 有 |
| subgroup 操作 | 完整 | 受限（P3-t02 需降级） | 受限 |
| 速度 | 快 | 快 | 很慢（纯 CPU） |

---

**这份表会随着你踩坑而增长。** 在 Mac 上遇到本文没写的现象，
请把它补进来 —— 那是这份文档最有价值的部分。
