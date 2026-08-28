#pragma once

// 需要 vmaCreateBuffer / vmaCreateImage 之类 API 的 .cpp 都该 include 这个，
// 而不是直接 include <vk_mem_alloc.h> —— 下面这几个宏必须在它之前定义好。
// 真正的实现（VMA_IMPLEMENTATION）只在 engine/src/rhi/VmaImpl.cpp 里展开一次。
#include "rwb/rhi/Vk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION           1002000

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4100 4127 4189 4324 4505)
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wnullability-completeness"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <vk_mem_alloc.h>

#if defined(_MSC_VER)
#  pragma warning(pop)
#elif defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
