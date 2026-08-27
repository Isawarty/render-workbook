// t05 — shader module / graphics pipeline
//
// 任务书: projects/p01-triangle/docs/t05-pipeline.md
// 判分:   ctest --preset win-msvc -R p01-t05
#include "../TriangleApp.h"

#include "rwb/core/File.h"
#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <array>
#include <stdexcept>

namespace p01 {

VkShaderModule TriangleApp::createShaderModule(const std::vector<std::uint32_t>& code) const {
    // TODO(p01-t05):
    //   vkCreateShaderModule。
    //   坑: codeSize 的单位是「字节」，而 pCode 的类型是 const uint32_t*。
    RWB_TODO("p01-t05 TriangleApp::createShaderModule");
}

void TriangleApp::createGraphicsPipeline() {
    // TODO(p01-t05):
    //   用 rwb::readSpirv() 读 RWB_SHADER_DIR 下的
    //   triangle.vert.spv / triangle.frag.spv，然后把整条 pipeline 搭出来:
    //
    //     shader stages       两个，入口都叫 "main"
    //     vertex input        空的 —— P1 的顶点硬编码在 shader 里，没有 vertex buffer
    //     input assembly      TRIANGLE_LIST
    //     dynamic state       VIEWPORT + SCISSOR
    //                         （这样 t08 窗口 resize 时不必重建 pipeline）
    //     viewport state      动态状态下只给 count，不给指针
    //     rasterization       FILL / lineWidth 1.0 / cullMode NONE
    //     multisample         1 sample
    //     color blend         不混合，写全部四个通道
    //     pipeline layout     空的（P1 的 shader 不读任何外部数据）-> m_pipelineLayout
    //
    //   最后 vkCreateGraphicsPipelines -> m_pipeline，
    //   并且记得销毁两个 shader module（SPIR-V 已经被编进 pipeline 了）。
    RWB_TODO("p01-t05 TriangleApp::createGraphicsPipeline");
}

} // namespace p01
