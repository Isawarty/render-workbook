// ResourceApp —— P2 的框架部分。
//
// 这里放的是「P1 已经教过、或者本来就不是教学点」的东西：
// render pass / framebuffer / pipeline 的组装、每帧录制、场景数据。
// 八道题真正要你写的东西全在 src/steps/0N_*.cpp 里。
//
// render pass 和 pipeline 会随阶段变形（t06 加深度、t08 加多重采样），
// 所以这个文件里有不少 `if (stage >= ...)`。读的时候可以只看你当前那一档。
#include "ResourceApp.h"

#include "rwb/core/Log.h"
#include "rwb/core/Todo.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <stdexcept>

namespace p02 {

// ---------------------------------------------------------------------------
// 顶点格式
// ---------------------------------------------------------------------------

VkVertexInputBindingDescription Vertex::bindingDescription() {
    VkVertexInputBindingDescription desc{};
    desc.binding   = 0;
    desc.stride    = sizeof(Vertex);
    // PER_VERTEX: 每个顶点前进一个 stride。
    // PER_INSTANCE 则是每个实例前进一次 —— 那是实例化渲染的入口，P4 会用到。
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::vector<VkVertexInputAttributeDescription> Vertex::attributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attrs(3);

    attrs[0].binding  = 0;
    attrs[0].location = 0;                            // 对应 shader 里的 location = 0
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;   // vec3
    attrs[0].offset   = offsetof(Vertex, pos);

    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, color);

    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;      // vec2
    attrs[2].offset   = offsetof(Vertex, uv);

    return attrs;
}

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------

ResourceApp::ResourceApp(AppConfig config) : m_config(config) {
    rwb::rhi::ContextConfig cc;
    cc.width            = config.width;
    cc.height           = config.height;
    cc.title            = config.title;
    cc.enableValidation = config.enableValidation;
    // 「可选」而不是「必须」：lavapipe 这类软件渲染器可能没有各向异性过滤，
    // 但 CI 跑不了不该等于这门课不能教它。t05 的采样器代码要自己查有没有开成。
    cc.optionalFeatures.samplerAnisotropy = true;
    cc.optionalFeatures.sampleRateShading = true;

    m_ctx       = std::make_unique<Context>(cc);
    m_swapchain = std::make_unique<Swapchain>(*m_ctx);
    m_renderer  = std::make_unique<FrameRenderer>(*m_ctx, *m_swapchain, kFramesInFlight);
    m_renderer->setResizeCallback([this] { onSwapchainResized(); });
}

ResourceApp::~ResourceApp() { cleanup(); }

void ResourceApp::initUpTo(Stage stage) {
    m_reached = stage;
    const int target = stageIndex(stage);

    buildScene(stage);

    // t07：glTF 的顶点要在建 buffer 之前并进 m_vertices / m_indices。
    if (target >= stageIndex(Stage::Model)) {
        loadModel(std::string(RWB_P02_ASSET_DIR) + "/shapes.gltf");
    }

    // t01：顶点/索引缓冲。每一档都要。
    createVertexBuffer();
    createIndexBuffer();

    // t02：描述符布局 + UBO + 池。
    // 注意 createDescriptorSets 挪到后面 —— 到 t05 时它要把纹理也写进去，
    // 得等纹理和采样器都建好。
    if (target >= stageIndex(Stage::Uniforms)) {
        createDescriptorSetLayout();
        createUniformBuffers();
        createDescriptorPool();
    }

    // t04：纹理上传。t05：视图 + 采样器 + mip 链。
    if (target >= stageIndex(Stage::Texture))  createTextureImage();
    if (target >= stageIndex(Stage::Sampler)) {
        createTextureImageView();
        createTextureSampler();
    }

    if (target >= stageIndex(Stage::Uniforms)) createDescriptorSets();

    // t08：多重采样。要在 render pass 之前定下采样数。
    if (target >= stageIndex(Stage::Msaa)) {
        m_sampleCount = maxUsableSampleCount();
        createColorResources();
    }
    // t06：深度缓冲。它的采样数必须和颜色附件一致，所以排在 t08 之后。
    if (target >= stageIndex(Stage::Depth)) createDepthResources();

    createRenderPass();
    createFramebuffers();
    createGraphicsPipeline();
}

void ResourceApp::run(int frameCount) {
    m_renderer->run(frameCount, [this](VkCommandBuffer cmd, std::uint32_t image, std::uint32_t frame) {
        recordFrame(cmd, image, frame);
    });
}

rwb::rhi::CapturedImage ResourceApp::renderAndCapture() {
    return m_renderer->renderAndCapture(
        [this](VkCommandBuffer cmd, std::uint32_t image, std::uint32_t frame) {
            recordFrame(cmd, image, frame);
        });
}

std::vector<VkPushConstantRange> ResourceApp::pushConstantRangesForTest() const {
    return pushConstantRanges();
}

// ---------------------------------------------------------------------------
// 场景数据
//
// 刻意「不」做动画：每一帧画的东西都一样，golden 图才有意义。
// 想看它转起来，把 m_time 接进 updateUniformBuffer 就行 —— 那是留给你的练习。
// ---------------------------------------------------------------------------

void ResourceApp::buildScene(Stage stage) {
    m_vertices.clear();
    m_indices.clear();
    m_draws.clear();

    const int s = stageIndex(stage);

    auto appendQuad = [this](float halfSize, float uvRepeat, const glm::vec3& c0,
                             const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3) {
        const auto base = static_cast<std::uint32_t>(m_vertices.size());
        const auto first = static_cast<std::uint32_t>(m_indices.size());
        m_vertices.push_back({{-halfSize, -halfSize, 0.0f}, c0, {0.0f,      0.0f}});
        m_vertices.push_back({{ halfSize, -halfSize, 0.0f}, c1, {uvRepeat,  0.0f}});
        m_vertices.push_back({{ halfSize,  halfSize, 0.0f}, c2, {uvRepeat,  uvRepeat}});
        m_vertices.push_back({{-halfSize,  halfSize, 0.0f}, c3, {0.0f,      uvRepeat}});
        for (std::uint32_t i : {0u, 1u, 2u, 2u, 3u, 0u}) m_indices.push_back(base + i);
        return std::pair<std::uint32_t, std::uint32_t>{first, 6u};
    };

    // 单位立方体，24 个顶点（每面独立，好让 uv 和颜色不跨面插值）
    auto appendCube = [this]() {
        const auto base  = static_cast<std::uint32_t>(m_vertices.size());
        const auto first = static_cast<std::uint32_t>(m_indices.size());

        const glm::vec3 faceColor[6] = {
            {1.0f, 0.35f, 0.35f}, {0.35f, 1.0f, 0.35f}, {0.35f, 0.45f, 1.0f},
            {1.0f, 0.9f,  0.35f}, {1.0f, 0.45f, 1.0f},  {0.4f,  0.95f, 1.0f},
        };
        // 每个面 4 个角，顺序 = 左下 右下 右上 左上
        const glm::vec3 corners[6][4] = {
            {{-.5f,-.5f, .5f}, { .5f,-.5f, .5f}, { .5f, .5f, .5f}, {-.5f, .5f, .5f}}, // +Z
            {{ .5f,-.5f,-.5f}, {-.5f,-.5f,-.5f}, {-.5f, .5f,-.5f}, { .5f, .5f,-.5f}}, // -Z
            {{ .5f,-.5f, .5f}, { .5f,-.5f,-.5f}, { .5f, .5f,-.5f}, { .5f, .5f, .5f}}, // +X
            {{-.5f,-.5f,-.5f}, {-.5f,-.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f,-.5f}}, // -X
            {{-.5f, .5f, .5f}, { .5f, .5f, .5f}, { .5f, .5f,-.5f}, {-.5f, .5f,-.5f}}, // +Y
            {{-.5f,-.5f,-.5f}, { .5f,-.5f,-.5f}, { .5f,-.5f, .5f}, {-.5f,-.5f, .5f}}, // -Y
        };
        const glm::vec2 uv[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

        for (int f = 0; f < 6; ++f) {
            for (int c = 0; c < 4; ++c) {
                m_vertices.push_back({corners[f][c], faceColor[f], uv[c]});
            }
            const std::uint32_t o = base + static_cast<std::uint32_t>(f * 4);
            for (std::uint32_t i : {0u, 1u, 2u, 2u, 3u, 0u}) m_indices.push_back(o + i);
        }
        return std::pair<std::uint32_t, std::uint32_t>{first, 36u};
    };

    if (s <= stageIndex(Stage::Uniforms)) {
        // t01 / t02：一个四边形。t01 的坐标直接就是 NDC，t02 起走相机矩阵。
        const auto [first, count] = appendQuad(0.6f, 1.0f,
                                               {1.0f, 0.2f, 0.2f}, {0.2f, 1.0f, 0.2f},
                                               {0.2f, 0.4f, 1.0f}, {1.0f, 0.9f, 0.2f});
        m_draws.push_back({first, count, 0, glm::mat4(1.0f)});

    } else if (s <= stageIndex(Stage::Texture)) {
        // t03 / t04：同一份几何画三次，每次一个不同的 model 矩阵。
        // 这正是 push constant 的典型用法 —— 数据小、每次 draw 都不同。
        const auto [first, count] = appendQuad(0.4f, 1.0f,
                                               {1.0f, 0.2f, 0.2f}, {0.2f, 1.0f, 0.2f},
                                               {0.2f, 0.4f, 1.0f}, {1.0f, 0.9f, 0.2f});
        const float angles[3]   = {-0.5f, 0.0f, 0.5f};
        const float offsets[3]  = {-1.1f, 0.0f, 1.1f};
        const float scales[3]   = {0.8f, 1.0f, 0.8f};
        for (int i = 0; i < 3; ++i) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), {offsets[i], 0.0f, 0.0f});
            model = glm::rotate(model, angles[i], glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(scales[i]));
            m_draws.push_back({first, count, 0, model});
        }

    } else {
        // t05 起：一块铺了棋盘格的地面。uv 重复 8 次，透视压缩之下
        // 远处必然进入 minification —— 没有 mip 链的话会闪成一片噪点。
        const auto [groundFirst, groundCount] =
            appendQuad(4.0f, 8.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
                       {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f});
        glm::mat4 ground = glm::translate(glm::mat4(1.0f), {0.0f, -0.8f, 0.0f});
        // 绕 X 轴转 90 度，把竖直的四边形放平成地面
        ground = glm::rotate(ground, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        m_draws.push_back({groundFirst, groundCount, 0, ground});

        if (s >= stageIndex(Stage::Depth)) {
            // t06 起：地面上放一个立方体。没有深度测试的话，
            // 立方体的背面会盖住正面 —— 这就是那一题要你亲眼看到的东西。
            const auto [cubeFirst, cubeCount] = appendCube();
            glm::mat4 cube = glm::translate(glm::mat4(1.0f), {0.0f, -0.3f, 0.0f});
            cube = glm::rotate(cube, glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            m_draws.push_back({cubeFirst, cubeCount, 0, cube});
        }
    }
}

// ---------------------------------------------------------------------------
// render pass / framebuffer / pipeline
// ---------------------------------------------------------------------------

void ResourceApp::createRenderPass() {
    const bool useDepth = stageIndex(m_reached) >= stageIndex(Stage::Depth);
    const bool useMsaa  = m_sampleCount != VK_SAMPLE_COUNT_1_BIT;

    std::vector<VkAttachmentDescription> attachments;

    // [0] 颜色。开了 MSAA 时它是多重采样的离屏图像，否则直接就是 swapchain 图像。
    VkAttachmentDescription color{};
    color.format         = m_swapchain->format();
    color.samples        = m_sampleCount;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // 开了 MSAA 时多重采样图像本身不需要留下来（resolve 出去就够了），
    // storeOp 用 DONT_CARE 能让驱动省掉一次显存写回 —— tile 架构上尤其明显。
    color.storeOp        = useMsaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                   : VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = useMsaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                   : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments.push_back(color);

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    if (useDepth) {
        VkAttachmentDescription depth{};
        depth.format         = m_depth.format;
        depth.samples        = m_sampleCount;
        depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // 深度只在这一趟里用，画完就没人要了
        depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthRef.attachment  = static_cast<std::uint32_t>(attachments.size());
        depthRef.layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depth);
    }

    VkAttachmentReference resolveRef{};
    if (useMsaa) {
        VkAttachmentDescription resolve{};
        resolve.format         = m_swapchain->format();
        resolve.samples        = VK_SAMPLE_COUNT_1_BIT;
        // resolve 目标会被完整覆写，不需要 clear
        resolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        resolveRef.attachment  = static_cast<std::uint32_t>(attachments.size());
        resolveRef.layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments.push_back(resolve);
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = useDepth ? &depthRef   : nullptr;
    subpass.pResolveAttachments     = useMsaa  ? &resolveRef : nullptr;

    // 外部依赖：等 swapchain 图像真正可写之后再开始写颜色附件。
    // srcAccessMask = 0 表示我们不等任何「写」，只等阶段推进 ——
    // 因为 acquire 的可见性由 imageAvailable 信号量保证。
    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    info.pAttachments    = attachments.data();
    info.subpassCount    = 1;
    info.pSubpasses      = &subpass;
    info.dependencyCount = 1;
    info.pDependencies   = &dep;

    VK_CHECK(vkCreateRenderPass(m_ctx->device(), &info, nullptr, &m_renderPass));
}

void ResourceApp::createFramebuffers() {
    const bool useDepth = stageIndex(m_reached) >= stageIndex(Stage::Depth);
    const bool useMsaa  = m_sampleCount != VK_SAMPLE_COUNT_1_BIT;

    m_framebuffers.resize(m_swapchain->imageCount());
    for (std::size_t i = 0; i < m_framebuffers.size(); ++i) {
        std::vector<VkImageView> views;
        // 顺序必须和 createRenderPass 里的 attachments 一一对应
        views.push_back(useMsaa ? m_colorTarget.view : m_swapchain->imageViews()[i]);
        if (useDepth) views.push_back(m_depth.view);
        if (useMsaa)  views.push_back(m_swapchain->imageViews()[i]);

        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = m_renderPass;
        info.attachmentCount = static_cast<std::uint32_t>(views.size());
        info.pAttachments    = views.data();
        info.width           = m_swapchain->extent().width;
        info.height          = m_swapchain->extent().height;
        info.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(m_ctx->device(), &info, nullptr, &m_framebuffers[i]));
    }
}

void ResourceApp::createGraphicsPipeline() {
    const int s = stageIndex(m_reached);

    // 按阶段挑 shader。四个变体的差别就是这门课的进度条：
    //   basic 什么都没有 -> ubo 有相机 -> push 每物体矩阵 -> tex 加采样
    const char* name = "scene_basic";
    if (s >= stageIndex(Stage::Sampler))            name = "scene_tex";
    else if (s >= stageIndex(Stage::PushConstants)) name = "scene_push";
    else if (s >= stageIndex(Stage::Uniforms))      name = "scene_ubo";

    const std::string dir = std::string(RWB_SHADER_DIR) + "/";
    ShaderModule vert = ShaderModule::fromFile(m_ctx->device(), dir + name + ".vert.spv");
    ShaderModule frag = ShaderModule::fromFile(m_ctx->device(), dir + name + ".frag.spv");

    const VkPipelineShaderStageCreateInfo stages[] = {
        vert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    const VkVertexInputBindingDescription binding = Vertex::bindingDescription();
    const std::vector<VkVertexInputAttributeDescription> attrs = Vertex::attributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // viewport/scissor 走动态状态：窗口一变大就重建 pipeline 太贵了。
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth   = 1.0f;
    // 全课程统一：不做背面剔除。这样立方体在没写深度测试时会「穿帮」，
    // t06 才看得见深度缓冲到底解决了什么问题。
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = m_sampleCount;
    if (m_sampleCount != VK_SAMPLE_COUNT_1_BIT && m_ctx->enabledFeatures().sampleRateShading) {
        // 逐采样着色：连三角形内部的纹理锯齿也一起处理，代价是片元着色器跑 N 次。
        multisample.sampleShadingEnable = VK_TRUE;
        multisample.minSampleShading    = 0.2f;
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    const bool useDepth = s >= stageIndex(Stage::Depth);
    depthStencil.depthTestEnable  = useDepth ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = useDepth ? VK_TRUE : VK_FALSE;
    // LESS：离相机更近的通过。配合 GLM_FORCE_DEPTH_ZERO_TO_ONE，
    // 近处 = 0、远处 = 1，clear 值要是 1.0。
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttachment;

    // pipeline layout = 「这条管线能看到哪些资源」的声明。
    const std::vector<VkPushConstantRange> pushRanges =
        (s >= stageIndex(Stage::PushConstants)) ? pushConstantRanges()
                                                : std::vector<VkPushConstantRange>{};

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts    = &m_descriptorSetLayout;
    }
    layoutInfo.pushConstantRangeCount = static_cast<std::uint32_t>(pushRanges.size());
    layoutInfo.pPushConstantRanges    = pushRanges.empty() ? nullptr : pushRanges.data();
    VK_CHECK(vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_pipelineLayout));

    VkGraphicsPipelineCreateInfo info{};
    info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState      = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState   = &multisample;
    info.pDepthStencilState  = &depthStencil;
    info.pColorBlendState    = &blend;
    info.pDynamicState       = &dynamicState;
    info.layout              = m_pipelineLayout;
    info.renderPass          = m_renderPass;
    info.subpass             = 0;

    VK_CHECK(vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &info,
                                       nullptr, &m_pipeline));
    // vert / frag 在这里析构。shader module 只在建管线时需要，建完就能扔。
}

// ---------------------------------------------------------------------------
// 每帧录制
// ---------------------------------------------------------------------------

void ResourceApp::recordFrame(VkCommandBuffer cmd, std::uint32_t imageIndex,
                              std::uint32_t frameIndex) {
    const int s = stageIndex(m_reached);

    // FrameRenderer 已经等过这个帧槽位的 fence，所以现在改它的 UBO 是安全的。
    if (s >= stageIndex(Stage::Uniforms)) updateUniformBuffer(frameIndex);

    std::vector<VkClearValue> clears;
    VkClearValue clearColor{};
    clearColor.color = {{0.02f, 0.02f, 0.06f, 1.0f}};
    clears.push_back(clearColor);
    if (s >= stageIndex(Stage::Depth)) {
        VkClearValue clearDepth{};
        clearDepth.depthStencil = {1.0f, 0};   // 1.0 = 最远
        clears.push_back(clearDepth);
    }

    VkRenderPassBeginInfo begin{};
    begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass        = m_renderPass;
    begin.framebuffer       = m_framebuffers[imageIndex];
    begin.renderArea.offset = {0, 0};
    begin.renderArea.extent = m_swapchain->extent();
    begin.clearValueCount   = static_cast<std::uint32_t>(clears.size());
    begin.pClearValues      = clears.data();

    vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapchain->extent().width);
    viewport.height   = static_cast<float>(m_swapchain->extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain->extent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const VkDeviceSize offsets[] = {0};
    const VkBuffer vertexBuffers[] = {m_vertexBuffer.handle};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    // 顶点数可能超过 65535，全课程统一用 32 位索引。
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

    if (s >= stageIndex(Stage::Uniforms) && !m_descriptorSets.empty()) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                                0, 1, &m_descriptorSets[frameIndex], 0, nullptr);
    }

    for (const MeshDraw& draw : m_draws) {
        if (s >= stageIndex(Stage::PushConstants)) {
            ObjectPush push{};
            push.model = draw.model;
            pushObjectData(cmd, push);
        }
        vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, draw.vertexOffset, 0);
    }

    vkCmdEndRenderPass(cmd);
}

// ---------------------------------------------------------------------------
// 尺寸相关资源的重建与销毁
// ---------------------------------------------------------------------------

void ResourceApp::onSwapchainResized() {
    destroySizeDependent();
    if (stageIndex(m_reached) >= stageIndex(Stage::Msaa))  createColorResources();
    if (stageIndex(m_reached) >= stageIndex(Stage::Depth)) createDepthResources();
    createFramebuffers();
    // render pass 只依赖格式和采样数，两者都没变，不用重建。
    // pipeline 的 viewport/scissor 是动态状态，也不用重建。
}

void ResourceApp::destroySizeDependent() noexcept {
    VkDevice device = m_ctx ? m_ctx->device() : VK_NULL_HANDLE;
    if (device == VK_NULL_HANDLE) return;

    for (VkFramebuffer fb : m_framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    m_framebuffers.clear();
    destroyImage(m_depth);
    destroyImage(m_colorTarget);
}

void ResourceApp::cleanup() noexcept {
    if (!m_ctx) return;
    m_ctx->waitIdle();

    // FrameRenderer 引用了 Context 的 command pool，必须先于它死掉。
    m_renderer.reset();

    VkDevice device = m_ctx->device();

    destroySizeDependent();

    if (m_pipeline != VK_NULL_HANDLE)       vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_renderPass != VK_NULL_HANDLE)     vkDestroyRenderPass(device, m_renderPass, nullptr);
    m_pipeline       = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_renderPass     = VK_NULL_HANDLE;

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    destroyImage(m_texture);

    // descriptor set 随 pool 一起销毁，不用单独 free
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSets.clear();
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    for (Buffer& b : m_uniformBuffers) destroyBuffer(b);
    m_uniformBuffers.clear();
    destroyBuffer(m_indexBuffer);
    destroyBuffer(m_vertexBuffer);

    m_swapchain.reset();
    m_ctx.reset();
}

// ---------------------------------------------------------------------------
// 程序化棋盘纹理。
//
// 刻意不从磁盘读 PNG：这一课的教学点是「Vulkan 怎么管理图像资源」，
// 不是「怎么解码 PNG」。程序化生成还有个好处 —— 基准图完全确定，
// 不受 stb_image 版本差异影响。真实项目里用 stb_image 就好。
// ---------------------------------------------------------------------------
std::vector<std::uint8_t> ResourceApp::checkerboardPixels(std::uint32_t size) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
    const std::uint32_t cell = size / 8;

    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const bool dark = ((x / cell) + (y / cell)) % 2 == 0;
            // 加一层随位置变化的色调，好让 mip 链的每一级看起来都不一样 ——
            // 少了这个，mipmap 有没有生成对了在画面上根本看不出来。
            const auto tint = static_cast<std::uint8_t>(40 + (x * 200) / size);
            const std::size_t i = (static_cast<std::size_t>(y) * size + x) * 4;
            pixels[i + 0] = dark ? static_cast<std::uint8_t>(30) : static_cast<std::uint8_t>(235);
            pixels[i + 1] = dark ? static_cast<std::uint8_t>(30) : tint;
            pixels[i + 2] = dark ? tint : static_cast<std::uint8_t>(235);
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

} // namespace p02
