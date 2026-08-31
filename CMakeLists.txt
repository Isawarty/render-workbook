cmake_minimum_required(VERSION 3.24)   # 3.24+ 才有稳定的 FetchContent + URL_HASH 行为
project(render-workbook LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
include(CompileShaders)
include(Testing)

# --- 编译选项 --------------------------------------------------------------
# 警告等级只加在「我们自己的」目标上。
# 如果用 add_compile_options 全局设置, FetchContent 拉进来的 glslang/glfw
# 会跟着一起编, 几千行第三方警告会把你自己的错误淹掉。
add_library(rwb_warnings INTERFACE)
if(MSVC)
  # /wd4100 = 未引用的形参。骨架里大量函数体是 RWB_TODO，参数必然没用上，
  # 不关掉的话每次编译都会刷一屏噪音（GCC/Clang 侧对应 -Wno-unused-parameter）。
  target_compile_options(rwb_warnings INTERFACE /W4 /permissive- /Zc:__cplusplus /wd4100)
else()
  target_compile_options(rwb_warnings INTERFACE -Wall -Wextra -Wno-unused-parameter)
endif()
add_library(rwb::warnings ALIAS rwb_warnings)

# 这几个宏必须全局生效（第三方也要看到）
if(MSVC)
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# volk 用动态加载，不要让任何地方直接链接 vulkan-1.lib / libvulkan.dylib
add_compile_definitions(VK_NO_PROTOTYPES)

# glm 默认按 OpenGL 的约定来，两条都要改：
#   GLM_FORCE_RADIANS           —— 不许再有隐式的角度制重载
#   GLM_FORCE_DEPTH_ZERO_TO_ONE —— OpenGL 的 NDC 深度是 [-1,1]，Vulkan 是 [0,1]。
#                                  不改的话近一半深度范围会被裁掉，症状是
#                                  「近处的东西莫名其妙消失」，而且极难联想到根因。
# 还有第三条改不了的：Vulkan 的 NDC +Y 朝下。glm::perspective 出来的矩阵
# 必须自己做 proj[1][1] *= -1，见 P2-t02。
add_compile_definitions(GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)

if(APPLE)
  # MoltenVK 走 portability subset，实例创建时必须带上对应 flag（P1-t01 会讲）
  add_compile_definitions(RWB_PLATFORM_MACOS=1)
elseif(WIN32)
  add_compile_definitions(RWB_PLATFORM_WINDOWS=1)
else()
  add_compile_definitions(RWB_PLATFORM_LINUX=1)
endif()

# --- 共享依赖（所有项目都要） ----------------------------------------------
rwb_require(vulkan_headers volk glm glfw glslang catch2)

add_subdirectory(engine)
add_subdirectory(tests/framework)

# --- 各项目 ----------------------------------------------------------------
add_subdirectory(projects/p00-setup)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p01-triangle/CMakeLists.txt")
  add_subdirectory(projects/p01-triangle)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p02-resources/CMakeLists.txt")
  add_subdirectory(projects/p02-resources)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p03-compute/CMakeLists.txt")
  add_subdirectory(projects/p03-compute)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p04-deferred/CMakeLists.txt")
  add_subdirectory(projects/p04-deferred)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p05-render-graph/CMakeLists.txt")
  add_subdirectory(projects/p05-render-graph)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p06-lua/CMakeLists.txt")
  add_subdirectory(projects/p06-lua)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/p07-d3d12/CMakeLists.txt")
  add_subdirectory(projects/p07-d3d12)
endif()
