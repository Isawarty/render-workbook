# engine/ —— 所有项目共用的基础设施。
#
# core/     日志、文件、TODO 异常、validation 收集器
# platform/ GLFW 窗口
# rhi/      Vulkan 封装。**这一层是 P1 的产物**：instance / device / swapchain /
#           同步与呈现，你已经在 P1 亲手写过一遍。从 P2 起它变成框架，好把注意力
#           让给资源管理、compute、延迟渲染这些还没学过的东西。
#           它不是黑盒 —— 做 P2-t01 之前值得对着自己的 mine/p01-t02 读一遍。
rwb_require(vma)

add_library(rwb_engine STATIC
  src/core/Log.cpp
  src/core/File.cpp
  src/core/ValidationLog.cpp
  src/platform/Window.cpp
  src/rhi/Vk.cpp
  src/rhi/VmaImpl.cpp
  src/rhi/Context.cpp
  src/rhi/Swapchain.cpp
  src/rhi/FrameRenderer.cpp
  src/rhi/Readback.cpp
  src/rhi/Shader.cpp)

add_library(rwb_rendergraph STATIC
  src/rendergraph/RenderGraph.cpp
  src/rendergraph/TransientImagePool.cpp)

target_include_directories(rwb_rendergraph PUBLIC include)
target_link_libraries(rwb_rendergraph
  PUBLIC volk::volk rwb_engine
  PRIVATE rwb::warnings vma)

add_library(rwb::rendergraph ALIAS rwb_rendergraph)

target_include_directories(rwb_engine PUBLIC include)

target_link_libraries(rwb_engine PRIVATE rwb::warnings)
target_link_libraries(rwb_engine PUBLIC
  volk::volk            # 含 volk.c: 定义全部 vk* 函数指针 + volkInitialize
                        # volk 用运行时 LoadLibrary/dlopen 找 loader,
                        # 不链接 vulkan-1.lib, 所以「零 SDK 也能编译」仍然成立
  glfw
  glm::glm
  vma)                  # 纯头文件；实现在 src/rhi/VmaImpl.cpp 里展开一次

add_library(rwb::engine ALIAS rwb_engine)
