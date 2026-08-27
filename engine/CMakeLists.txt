# engine/ —— 所有项目共用的基础设施。
#
# 刻意只放「不是教学点」的东西：日志、文件、窗口、TODO 异常。
# Vulkan 封装（engine/rhi）从 P2 开始，由你在 P1 亲手写出来的代码提炼而成。
add_library(rwb_engine STATIC
  src/core/Log.cpp
  src/core/File.cpp
  src/core/ValidationLog.cpp
  src/platform/Window.cpp)

target_include_directories(rwb_engine PUBLIC include)

target_link_libraries(rwb_engine PRIVATE rwb::warnings)
target_link_libraries(rwb_engine PUBLIC
  volk::volk            # 含 volk.c: 定义全部 vk* 函数指针 + volkInitialize
                        # volk 用运行时 LoadLibrary/dlopen 找 loader,
                        # 不链接 vulkan-1.lib, 所以「零 SDK 也能编译」仍然成立
  glfw
  glm::glm)

add_library(rwb::engine ALIAS rwb_engine)
