#pragma once

// 所有 rhi 头文件的公共入口。
//
// 顺序要紧：volk 必须在任何 vulkan 头之前，因为它负责在 VK_NO_PROTOTYPES 之下
// 提供全部 vk* 函数指针。参见 rwb/platform/Window.h 里的长注释。
#include <volk.h>

#include "rwb/core/Log.h"

#include <stdexcept>
#include <string>

namespace rwb::rhi {

// VkResult 的名字。只覆盖实践中真会遇到的那些 ——
// 打出 "VK_ERROR_INITIALIZATION_FAILED" 比打出 "-3" 能省你十分钟。
const char* resultName(VkResult result);

[[noreturn]] void throwVkError(VkResult result, const char* call, const char* file, int line);

// VMA 检测到「销毁内存块时还有分配没释放」时的回调。
//
// VMA 默认把这条检查接到 assert()：在 Windows 上是一个模态弹窗，
// 在关掉弹窗的测试进程里则是静默 abort —— 后果是整个测试进程死在半路，
// 后面的用例连结果都报不出来（表现为判分表里的一片 "?"）。
//
// 挖空课程里这条路径太容易走到：任何一个还没实现的函数从 immediateSubmit
// 的 lambda 里抛出来，都会跳过调用方后面的 destroyBuffer。
// 那是「题还没做」，不是「你写错了」，不该以 abort 收场。
//
// 所以改成「大声记一条 error 然后继续」：泄漏检测的价值保留，失败方式变回可读。
// 真正的 API 误用仍然走 VMA 原本的硬 assert。
void reportVmaLeak(const char* expr, const char* file, int line);

} // namespace rwb::rhi

// 每一个可能失败的 Vulkan 调用都该包一层。
//
// 不这么做的典型后果：vkCreateSwapchainKHR 静默失败返回 VK_NULL_HANDLE，
// 你在三个函数之后才收到一条 "invalid handle" 的 validation error，
// 然后花半小时找错地方。
#define VK_CHECK(call)                                                        \
    do {                                                                      \
        const VkResult rwb_vk_result_ = (call);                               \
        if (rwb_vk_result_ != VK_SUCCESS) {                                   \
            ::rwb::rhi::throwVkError(rwb_vk_result_, #call, __FILE__, __LINE__); \
        }                                                                     \
    } while (false)
