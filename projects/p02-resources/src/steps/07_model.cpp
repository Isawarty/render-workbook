// t07 —— glTF 加载 / 多 mesh 绘制
//
// 任务书: projects/p02-resources/docs/t07-model.md
// 判分:   ctest --preset win-msvc -R p02-t07
#include "../ResourceApp.h"
#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstring>
#include <stdexcept>

// tinygltf 是 header-only，实现在这里展开一次。这段是给你的样板，不是考点。
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#if defined(_MSC_VER)
#  pragma warning(push)
// 4005 = APIENTRY 重定义。tinygltf 会拉进 <windows.h>，而 volk/GLFW 已经定义过它。
#  pragma warning(disable : 4005 4018 4100 4127 4189 4244 4267 4456 4457 4459 4996)
#elif defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <tiny_gltf.h>
#if defined(_MSC_VER)
#  pragma warning(pop)
#elif defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

namespace p02 {

void ResourceApp::loadModel(const std::string& path) {
    // TODO(p02-t07):
    //
    //   用 tinygltf 加载 path，把里面每个 mesh 的顶点追加进 m_vertices / m_indices，
    //   并为每个 mesh 追加一条 MeshDraw。
    //
    //   骨架流程：
    //     tinygltf::Model model; tinygltf::TinyGLTF loader;
    //     loader.LoadASCIIFromFile(&model, &err, &warn, path)   // 本课的资产是 .gltf 文本
    //     取默认 scene（model.defaultScene 可能是 -1，那就用 0）
    //     遍历 scene.nodes -> model.nodes[i]
    //       node.mesh < 0 的跳过（相机、灯光、空组节点）
    //       算出节点的局部变换
    //       遍历 mesh.primitives（prim.mode != TINYGLTF_MODE_TRIANGLES 的跳过）
    //         读 POSITION(vec3) / TEXCOORD_0(vec2) / COLOR_0(vec3) 和 indices
    //         追加顶点、追加索引、追加一条 MeshDraw
    //
    //   ---- 三个真正的考点 ----
    //
    //   1) accessor / bufferView / buffer 三层间接，以及 byteStride。
    //      数据地址 = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset
    //      相邻元素之间隔的是 bufferView.byteStride（为 0 时才等于 sizeof(元素)）。
    //      glTF 允许把 POSITION 和 NORMAL 交错放在同一个 bufferView 里 ——
    //      不处理 stride 的加载器在「导出时勾了交错」的模型上会读出一团乱麻。
    //
    //   2) 索引的 componentType 可能是 UNSIGNED_BYTE / SHORT / INT，要统一提成 uint32。
    //
    //   3) vertexOffset。第二个 mesh 的顶点追加在第一个之后，它自己的索引仍然是
    //      从 0 开始编号的。把 MeshDraw::vertexOffset 设成「追加前 m_vertices 的大小」，
    //      让 vkCmdDrawIndexed 在 GPU 上替你加 —— 比在 CPU 上改索引值省事，
    //      也让同一份索引数据能被多个实例复用。
    //      忘了它的话，第二个模型会画出第一个模型的顶点。
    //
    //   ---- 节点变换 ----
    //   glTF 允许两种写法，只处理其中一种是加载器最常见的漏洞：
    //     node.matrix.size() == 16  -> 列主序 4x4，和 glm 一致，可以直接 glm::make_mat4
    //     否则 TRS 三件套            -> translation / rotation / scale
    //   注意四元数顺序：glTF 是 (x,y,z,w)，glm::quat 的构造是 (w,x,y,z)。
    //   搞反不会报错，只会让模型转到一个诡异的角度。
    //
    //   最后给 MeshDraw::model 乘一个 translate({0, -0.55, 0})，让模型站在地面上。
    RWB_TODO("p02-t07 ResourceApp::loadModel");
}

} // namespace p02
