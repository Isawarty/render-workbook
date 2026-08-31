if(NOT WIN32)
  message(STATUS "P07 is Windows-only: skipping Direct3D 12 targets on ${CMAKE_SYSTEM_NAME}.")
  return()
endif()

add_library(p07_lib STATIC
  src/D3D12App.cpp
  src/steps/01_core.cpp
  src/steps/02_pipeline.cpp
  src/steps/03_sync.cpp
  src/steps/04_cube.cpp
  src/steps/05_compute.cpp)
target_include_directories(p07_lib PUBLIC src)
target_include_directories(p07_lib PRIVATE "${CMAKE_SOURCE_DIR}/engine/include")
target_link_libraries(p07_lib
  PUBLIC d3d12 dxgi dxguid
  PRIVATE rwb::warnings)
target_compile_definitions(p07_lib PRIVATE
  RWB_P07_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/p07")

add_executable(p07_d3d12 src/main.cpp)
target_link_libraries(p07_d3d12 PRIVATE p07_lib rwb::warnings)

add_executable(p07_tests tests/test_p07.cpp)
target_link_libraries(p07_tests PRIVATE p07_lib rwb::testfw rwb::warnings)

foreach(NN 01 02 03)
  rwb_add_test(p07-t${NN} TARGET p07_tests LABELS "l1;p07" ARGS "[t${NN}]")
  set_tests_properties(p07-t${NN} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()
foreach(NN 04 05)
  rwb_add_test(p07-t${NN} TARGET p07_tests LABELS "l1;l2;p07" ARGS "[t${NN}]")
  set_tests_properties(p07-t${NN} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()
foreach(NN 06 07)
  rwb_add_test(p07-t${NN} TARGET p07_tests LABELS "l2;p07" ARGS "[t${NN}]")
  set_tests_properties(p07-t${NN} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()

# t06 才要求 Slang；早期 task 的骨架和测试 target 在未安装 slangc 时仍可编译。
find_program(P07_SLANGC slangc HINTS "$ENV{VULKAN_SDK}/bin")
set(P07_SLANG_OUT_DIR "${CMAKE_BINARY_DIR}/shaders/p07")
file(MAKE_DIRECTORY "${P07_SLANG_OUT_DIR}")
if(P07_SLANGC)
  set(P07_SLANG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared.slang")
  set(P07_DXIL "${P07_SLANG_OUT_DIR}/shared.dxil")
  add_custom_command(
    OUTPUT "${P07_DXIL}"
    COMMAND "${P07_SLANGC}" "${P07_SLANG_SOURCE}"
            -target dxil -profile sm_6_0 -entry computeMain -stage compute -o "${P07_DXIL}"
    DEPENDS "${P07_SLANG_SOURCE}"
    COMMENT "slangc   shared.slang -> shared.dxil"
    VERBATIM)
  # RWB_TODO(p07-t06): 为同一个 entry 增加 SPIR-V 1.5 custom command，
  # 并让 target 同时依赖 shared.dxil 与 shared.spv。
  add_custom_target(p07_slang_shaders DEPENDS "${P07_DXIL}")
  add_dependencies(p07_tests p07_slang_shaders)
else()
  message(STATUS "slangc not found: P07 t01-t05 remain buildable; t06 requires slangc.")
  add_custom_target(p07_slang_shaders
    COMMAND ${CMAKE_COMMAND} -E echo "P07-t06 requires slangc in PATH or VULKAN_SDK/bin."
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM)
endif()
