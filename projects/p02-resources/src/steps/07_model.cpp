// t07 —— glTF 加载 / 多 mesh 绘制
//
// 到这里为止，几何数据都是代码里硬写的。真实项目的模型来自 DCC 工具，
// glTF 2.0 是当下事实上的交换格式（Khronos 出品，和 Vulkan 同门）。
//
// 这道题的核心不是「解析 JSON」——tinygltf 替你做了。核心是：
//   1. accessor / bufferView / buffer 三层间接，以及 byteStride
//   2. 一个文件里多个 mesh，各有自己的节点变换，怎么合并进同一对 buffer
//   3. 索引要加上 vertexOffset，否则第二个 mesh 会画出第一个 mesh 的顶点
#include "../ResourceApp.h"

#include "rwb/core/Log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstring>
#include <stdexcept>

// tinygltf 是 header-only，实现在这里展开一次。
// 关掉它自带的图片解码：这一课不需要贴图文件，少一堆和 stb 抢符号的麻烦。
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#if defined(_MSC_VER)
#  pragma warning(push)
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
namespace {

// accessor -> 连续的 float。
//
// 这里必须处理 byteStride：glTF 允许把 POSITION 和 NORMAL 交错放在同一个
// bufferView 里，那时相邻元素之间隔的不是 sizeof(元素) 而是 stride。
// 不处理 stride 的加载器在「导出时勾了交错」的模型上会读出一团乱麻。
std::vector<float> readFloats(const tinygltf::Model& model, int accessorIndex,
                              int componentsPerElement) {
    if (accessorIndex < 0) return {};

    const tinygltf::Accessor&   accessor = model.accessors[static_cast<std::size_t>(accessorIndex)];
    const tinygltf::BufferView& view     = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    const tinygltf::Buffer&     buffer   = model.buffers[static_cast<std::size_t>(view.buffer)];

    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        // 归一化的 u8/u16 顶点色也是合法的 glTF，真实加载器要展开处理。
        // 这门课的资产全是 float，遇到别的直接报错比静默读错好。
        throw std::runtime_error("这个 accessor 不是 float，本课的加载器不处理");
    }

    const std::size_t stride = view.byteStride != 0
                                   ? view.byteStride
                                   : static_cast<std::size_t>(componentsPerElement) * sizeof(float);
    const unsigned char* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;

    std::vector<float> out(accessor.count * static_cast<std::size_t>(componentsPerElement));
    for (std::size_t i = 0; i < accessor.count; ++i) {
        std::memcpy(&out[i * static_cast<std::size_t>(componentsPerElement)],
                    base + i * stride,
                    static_cast<std::size_t>(componentsPerElement) * sizeof(float));
    }
    return out;
}

// 索引可能是 u8 / u16 / u32，统一提成 u32。
std::vector<std::uint32_t> readIndices(const tinygltf::Model& model, int accessorIndex) {
    const tinygltf::Accessor&   accessor = model.accessors[static_cast<std::size_t>(accessorIndex)];
    const tinygltf::BufferView& view     = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    const tinygltf::Buffer&     buffer   = model.buffers[static_cast<std::size_t>(view.buffer)];

    const unsigned char* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;
    std::vector<std::uint32_t> out(accessor.count);

    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            for (std::size_t i = 0; i < accessor.count; ++i) out[i] = base[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const auto* p = reinterpret_cast<const std::uint16_t*>(base);
            for (std::size_t i = 0; i < accessor.count; ++i) out[i] = p[i];
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const auto* p = reinterpret_cast<const std::uint32_t*>(base);
            for (std::size_t i = 0; i < accessor.count; ++i) out[i] = p[i];
            break;
        }
        default:
            throw std::runtime_error("索引的 componentType 不认识");
    }
    return out;
}

// 节点的局部变换。glTF 允许两种写法：一个 4x4 matrix，或 TRS 三件套。
// 只处理其中一种，是加载器最常见的漏洞 —— 不同导出器的习惯不一样。
glm::mat4 nodeTransform(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        // glTF 的矩阵是列主序，和 glm 一致，可以直接喂进去
        std::array<float, 16> m{};
        for (int i = 0; i < 16; ++i) m[static_cast<std::size_t>(i)] = static_cast<float>(node.matrix[static_cast<std::size_t>(i)]);
        return glm::make_mat4(m.data());
    }

    glm::mat4 t(1.0f);
    if (node.translation.size() == 3) {
        t = glm::translate(t, glm::vec3(static_cast<float>(node.translation[0]),
                                        static_cast<float>(node.translation[1]),
                                        static_cast<float>(node.translation[2])));
    }
    if (node.rotation.size() == 4) {
        // glTF 的四元数是 (x, y, z, w)，glm::quat 的构造是 (w, x, y, z)。
        // 顺序搞反不会报错，只会让模型转到一个诡异的角度。
        const glm::quat q(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                          static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
        t = t * glm::mat4_cast(q);
    }
    if (node.scale.size() == 3) {
        t = glm::scale(t, glm::vec3(static_cast<float>(node.scale[0]),
                                    static_cast<float>(node.scale[1]),
                                    static_cast<float>(node.scale[2])));
    }
    return t;
}

} // namespace

void ResourceApp::loadModel(const std::string& path) {
    tinygltf::Model     model;
    tinygltf::TinyGLTF  loader;
    std::string         err, warn;

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path)) {
        throw std::runtime_error("glTF 加载失败: " + path + "\n  " + err);
    }
    if (!warn.empty()) rwb::logWarn("glTF: " + warn);

    const tinygltf::Scene& scene = model.scenes[static_cast<std::size_t>(
        model.defaultScene >= 0 ? model.defaultScene : 0)];

    for (int nodeIndex : scene.nodes) {
        const tinygltf::Node& node = model.nodes[static_cast<std::size_t>(nodeIndex)];
        if (node.mesh < 0) continue;   // 纯变换节点（相机、灯光、空组）

        const glm::mat4 local = nodeTransform(node);
        const tinygltf::Mesh& mesh = model.meshes[static_cast<std::size_t>(node.mesh)];

        for (const tinygltf::Primitive& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            auto attr = [&](const char* name) {
                const auto it = prim.attributes.find(name);
                return it == prim.attributes.end() ? -1 : it->second;
            };

            const std::vector<float> positions = readFloats(model, attr("POSITION"), 3);
            const std::vector<float> uvs       = readFloats(model, attr("TEXCOORD_0"), 2);
            const std::vector<float> colors    = readFloats(model, attr("COLOR_0"), 3);
            const std::vector<std::uint32_t> indices = readIndices(model, prim.indices);

            const std::size_t vertexCount = positions.size() / 3;

            // 关键：这一批顶点会追加在已有顶点之后，所以索引要整体平移。
            // 用 vertexOffset 让 GPU 在 draw 时替我们加，比在 CPU 上改索引值省事，
            // 也让同一份索引数据能被多个实例复用。
            const auto vertexOffset = static_cast<std::int32_t>(m_vertices.size());
            const auto firstIndex   = static_cast<std::uint32_t>(m_indices.size());

            for (std::size_t i = 0; i < vertexCount; ++i) {
                Vertex v{};
                v.pos = {positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]};
                v.uv  = uvs.size() >= (i + 1) * 2 ? glm::vec2{uvs[i * 2], uvs[i * 2 + 1]}
                                                  : glm::vec2{0.0f, 0.0f};
                v.color = colors.size() >= (i + 1) * 3
                              ? glm::vec3{colors[i * 3], colors[i * 3 + 1], colors[i * 3 + 2]}
                              : glm::vec3{1.0f};
                m_vertices.push_back(v);
            }
            m_indices.insert(m_indices.end(), indices.begin(), indices.end());

            MeshDraw draw{};
            draw.firstIndex   = firstIndex;
            draw.indexCount   = static_cast<std::uint32_t>(indices.size());
            draw.vertexOffset = vertexOffset;
            // 模型自带的节点变换之外，再抬高一点让它站在地面上
            draw.model = glm::translate(glm::mat4(1.0f), {0.0f, -0.55f, 0.0f}) * local;
            m_draws.push_back(draw);
        }
    }

    rwb::logInfo(rwb::format("glTF 加载完成: %zu 个顶点, %zu 个索引, %zu 次 draw",
                             m_vertices.size(), m_indices.size(), m_draws.size()));
}

} // namespace p02
