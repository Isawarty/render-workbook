# P4 —— 延迟渲染。
#
# t01 只建立 G-buffer 的资源和 render-pass 契约；真实几何写入从 t02 开始。
rwb_require(vma tinygltf)

add_library(p04_lib STATIC
  src/DeferredApp.cpp
  src/steps/01_gbuffer.cpp
  src/steps/02_geometry.cpp
  src/steps/03_lighting.cpp
  src/steps/04_ibl.cpp
  src/steps/05_shadows.cpp
  src/steps/06_bloom.cpp
  src/steps/07_slang.cpp)
target_include_directories(p04_lib PUBLIC src)
target_link_libraries(p04_lib PUBLIC rwb::engine PRIVATE rwb::warnings vma tinygltf)
target_compile_definitions(p04_lib PRIVATE
  RWB_P04_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

rwb_add_shaders(p04_lib SHADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_ibl.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_shadow.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_hdr.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ibl.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.frag)

find_program(P04_SLANGC slangc HINTS "$ENV{VULKAN_SDK}/bin")
set(P04_SLANG_OUT_DIR "${CMAKE_BINARY_DIR}/shaders/p04_slang")
file(MAKE_DIRECTORY "${P04_SLANG_OUT_DIR}")
if(P04_SLANGC)
  set(P04_SLANG_SPV "")
  function(p04_add_slang_shader SOURCE STAGE)
    get_filename_component(SOURCE_ABS "${SOURCE}" ABSOLUTE)
    get_filename_component(SOURCE_NAME "${SOURCE}" NAME)
    set(OUTPUT "${P04_SLANG_OUT_DIR}/${SOURCE_NAME}.spv")
    add_custom_command(
      OUTPUT "${OUTPUT}"
      COMMAND "${P04_SLANGC}" "${SOURCE_ABS}"
              -target spirv -profile spirv_1_3
              -entry main -stage "${STAGE}" -o "${OUTPUT}"
      DEPENDS "${SOURCE_ABS}"
      COMMENT "slangc   ${SOURCE_NAME} -> ${SOURCE_NAME}.spv"
      VERBATIM)
    set(P04_SLANG_SPV "${P04_SLANG_SPV};${OUTPUT}" PARENT_SCOPE)
  endfunction()

  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry.vert.slang vertex)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry.frag.slang fragment)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting.vert.slang vertex)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_hdr.frag.slang fragment)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/ibl.comp.slang compute)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.vert.slang vertex)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.frag.slang fragment)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom.comp.slang compute)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.vert.slang vertex)
  p04_add_slang_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.frag.slang fragment)
  add_custom_target(p04_slang_shaders DEPENDS ${P04_SLANG_SPV})
  add_dependencies(p04_lib p04_slang_shaders)
else()
  message(STATUS
          "slangc not found: P04 t01-t06 remain buildable; target "
          "p04_slang_shaders (and task t07) requires slangc.")
  add_custom_target(p04_slang_shaders
    COMMAND ${CMAKE_COMMAND} -E echo
            "P04-t07 requires slangc in PATH or VULKAN_SDK/bin."
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM)
endif()
target_compile_definitions(p04_lib PRIVATE RWB_SLANG_SHADER_DIR="${P04_SLANG_OUT_DIR}")

add_executable(p04_deferred src/main.cpp)
target_link_libraries(p04_deferred PRIVATE p04_lib rwb::warnings)

add_executable(p04_tests tests/test_p04.cpp)
target_link_libraries(p04_tests PRIVATE p04_lib rwb::testfw rwb::warnings)

foreach(NN 01 02 03 04 05 06 07)
  rwb_add_test(p04-t${NN} TARGET p04_tests LABELS "l1;l2;p04" ARGS "[t${NN}]")
  set_tests_properties(p04-t${NN} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()
