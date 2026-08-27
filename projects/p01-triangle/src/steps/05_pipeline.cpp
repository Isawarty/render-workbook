// t05 — shader module / graphics pipeline
//
// OpenGL 里绝大多数渲染状态是全局的、可以随时 glEnable 改。
// Vulkan 把混合、深度、光栅化、视口、shader 全部固化进一个不可变的
// VkPipeline 对象，创建时一次性校验完毕。
// 代价：状态组合爆炸（PSO 数量管理是引擎的真问题之一）。
// 回报：draw call 期间驱动无事可做，也就没有「驱动在你背后重编 shader」的卡顿。
#include "../TriangleApp.h"

#include "rwb/core/File.h"
#include "rwb/core/Log.h"

#include <array>
#include <stdexcept>

namespace p01 {

VkShaderModule TriangleApp::createShaderModule(const std::vector<std::uint32_t>& code) const {
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    // 注意 codeSize 的单位是「字节」，而 pCode 的类型是 uint32_t*。
    // 这是 Vulkan API 里一个经典的踩坑点。
    info.codeSize = code.size() * sizeof(std::uint32_t);
    info.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule 失败");
    }
    return module;
}

void TriangleApp::createGraphicsPipeline() {
    const std::vector<std::uint32_t> vertCode =
        rwb::readSpirv(std::string(RWB_SHADER_DIR) + "/triangle.vert.spv");
    const std::vector<std::uint32_t> fragCode =
        rwb::readSpirv(std::string(RWB_SHADER_DIR) + "/triangle.frag.spv");

    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";   // 入口函数名, 一个模块可以有多个入口

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    const std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    // P1 的顶点数据是硬编码在 vertex shader 里的常量数组，
    // 由 gl_VertexIndex 索引，所以这里没有任何顶点输入绑定。
    // P2-t01 才会引入真正的 vertex buffer。
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 视口和裁剪设为动态状态：这样窗口 resize 时不必重建整条 pipeline（t08 会用到）。
    const std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;   // 动态状态下只需给数量, 不给指针
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;   // 非 1.0 需要 wideLines 特性
    rasterizer.cullMode                = VK_CULL_MODE_NONE;   // P1 只有一个三角形, 不剔除
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable  = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &blendAttachment;

    // pipeline layout 描述 shader 能访问哪些 descriptor set 和 push constant。
    // P1 的 shader 什么外部数据都不读，所以是个空 layout。P2-t02 会填满它。
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, fragModule, nullptr);
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        throw std::runtime_error("vkCreatePipelineLayout 失败");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast<std::uint32_t>(stages.size());
    pipelineInfo.pStages             = stages.data();
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = nullptr;   // P2-t05 才加深度
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = m_renderPass;
    pipelineInfo.subpass             = 0;

    const VkResult res = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                                   nullptr, &m_pipeline);

    // shader module 只在建 pipeline 时被读取, 建完就能扔。
    // SPIR-V 已经被编译进 pipeline 里了。
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    vkDestroyShaderModule(m_device, vertModule, nullptr);

    if (res != VK_SUCCESS) {
        throw std::runtime_error(rwb::format("vkCreateGraphicsPipelines 失败, VkResult = %d", res));
    }
}

} // namespace p01
