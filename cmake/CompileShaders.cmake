# ---------------------------------------------------------------------------
# GLSL -> SPIR-V 编译
#
# 用 FetchContent 构建出来的 glslang 可执行文件，而不是 Vulkan SDK 里的 glslc。
# 这样「没装 SDK 也能完整编译」这条承诺才成立。
#
# P4-t07 之后这个文件会换成 Slang 编译器；届时接口 rwb_add_shaders() 不变。
# ---------------------------------------------------------------------------

# 找出 glslang 的可执行文件 target（不同版本 target 名不一样）
function(_rwb_glslang_exe OUT)
  if(TARGET glslang-standalone)
    set(${OUT} "$<TARGET_FILE:glslang-standalone>" PARENT_SCOPE)
  elseif(TARGET glslangValidator)
    set(${OUT} "$<TARGET_FILE:glslangValidator>" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "找不到 glslang 可执行文件 target。是否忘了 rwb_require(glslang)?")
  endif()
endfunction()

function(_rwb_glslang_dep OUT)
  if(TARGET glslang-standalone)
    set(${OUT} glslang-standalone PARENT_SCOPE)
  else()
    set(${OUT} glslangValidator PARENT_SCOPE)
  endif()
endfunction()

# rwb_add_shaders(<target> SHADERS a.vert b.frag ...)
#
# 把每个 shader 编译成 .spv，产物放在 <build>/shaders/<target>/ 下，
# 并给 target 定义 RWB_SHADER_DIR 宏指向该目录。
function(rwb_add_shaders TARGET)
  cmake_parse_arguments(ARG "" "" "SHADERS" ${ARGN})

  _rwb_glslang_exe(GLSLANG_EXE)
  _rwb_glslang_dep(GLSLANG_DEP)

  set(OUT_DIR "${CMAKE_BINARY_DIR}/shaders/${TARGET}")
  file(MAKE_DIRECTORY "${OUT_DIR}")

  set(SPV_FILES "")
  foreach(SRC ${ARG_SHADERS})
    get_filename_component(SRC_ABS "${SRC}" ABSOLUTE)
    get_filename_component(SRC_NAME "${SRC}" NAME)
    set(SPV "${OUT_DIR}/${SRC_NAME}.spv")

    add_custom_command(
      OUTPUT  "${SPV}"
      COMMAND "${GLSLANG_EXE}" -V --target-env vulkan1.2 -o "${SPV}" "${SRC_ABS}"
      DEPENDS "${SRC_ABS}" ${GLSLANG_DEP}
      COMMENT "glslang  ${SRC_NAME} -> ${SRC_NAME}.spv"
      VERBATIM)
    list(APPEND SPV_FILES "${SPV}")
  endforeach()

  add_custom_target(${TARGET}_shaders DEPENDS ${SPV_FILES})
  add_dependencies(${TARGET} ${TARGET}_shaders)
  target_compile_definitions(${TARGET} PRIVATE RWB_SHADER_DIR="${OUT_DIR}")
endfunction()
