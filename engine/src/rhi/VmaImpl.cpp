// VMA 的实现单元。整个工程里只能有这一个文件定义 VMA_IMPLEMENTATION。
//
// 与 volk 配合的三个关键设置（写错了会在链接期报一堆 vk* 未定义符号）：
//   VMA_STATIC_VULKAN_FUNCTIONS  0  —— 我们全局定义了 VK_NO_PROTOTYPES，
//                                      没有任何 vk* 符号可以静态链接
//   VMA_DYNAMIC_VULKAN_FUNCTIONS 1  —— 让 VMA 自己用 vkGetInstanceProcAddr /
//                                      vkGetDeviceProcAddr 把需要的函数取齐
//   VMA_VULKAN_VERSION       1002000 —— 全课程按 Vulkan 1.2 写。不写的话 VMA
//                                      默认按 1.3 编，会去要设备上不存在的函数。
//
// 注意 include 顺序：先 volk 拿到全部 Vulkan 类型，再 VMA。
#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION           1002000
#define VMA_IMPLEMENTATION

// 只改「泄漏检测」这一条断言，其它照旧。理由见 rwb/rhi/Vk.h 里 reportVmaLeak 的注释：
// 默认接的 assert() 在关掉了弹窗的测试进程里是静默 abort，会让整个进程死在半路，
// 后面的用例连结果都报不出来。VMA 专门留了 VMA_ASSERT_LEAK 这个钩子。
#include "rwb/rhi/Vk.h"
#define VMA_ASSERT_LEAK(expr)                                              \
    do {                                                                   \
        if (!(expr)) ::rwb::rhi::reportVmaLeak(#expr, __FILE__, __LINE__); \
    } while (false)

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4100 4127 4189 4324 4505 4820 4189)
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-variable"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wmissing-field-initializers"
#  pragma clang diagnostic ignored "-Wnullability-completeness"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <vk_mem_alloc.h>

#if defined(_MSC_VER)
#  pragma warning(pop)
#elif defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
