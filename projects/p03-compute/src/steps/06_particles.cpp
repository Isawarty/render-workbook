// t06 —— GPU particles / cross-queue synchronization
// 任务书: projects/p03-compute/docs/t06-particles.md
// 判分:   python rwb.py test p03-t06

#include "../ComputeApp.h"

#include "rwb/rhi/Readback.h"
#include "rwb/rhi/Shader.h"

#include <cstring>
#include <stdexcept>

namespace p03 {
namespace {

struct ParticlePush { std::uint32_t n; float dx; float dy; };

VkCommandPool makePool(VkDevice device, std::uint32_t family) {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = family;
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &pool));
    return pool;
}

VkCommandBuffer beginOneShot(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &alloc, &cmd));
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    return cmd;
}

} // namespace

std::vector<float> ComputeApp::runParticles(const std::vector<float>& positions,
                                            float deltaX, float deltaY) {
    if (positions.empty()) { m_lastQueueSync = {}; return {}; }
    if ((positions.size() % 4) != 0) throw std::runtime_error("particles: 输入必须是 vec4 数组");
    const auto n = static_cast<std::uint32_t>(positions.size() / 4);
    const VkDeviceSize bytes = sizeof(float) * static_cast<VkDeviceSize>(positions.size());
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    ScopedBuffer particles(*this, createBuffer(bytes, usage, true, true));
    ScopedBuffer consumed(*this, createBuffer(bytes, usage, false, false));
    std::memcpy(particles.get().mapped, positions.data(), static_cast<std::size_t>(bytes));

    ScopedPipeline update(*this, createComputePipeline(
        "particles_update.comp.spv", 1, sizeof(ParticlePush)));
    const VkDescriptorSet updateSet = allocateStorageSet(update.get(), {&particles.get()});

    VkDevice device = m_ctx->device();
    const std::uint32_t computeFamily = m_ctx->queueFamilies().compute.value();
    const std::uint32_t graphicsFamily = m_ctx->queueFamilies().graphics.value();
    const bool transfer = computeFamily != graphicsFamily;
    m_lastQueueSync = {computeFamily, graphicsFamily, transfer, false};

    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkCommandPool computePool = VK_NULL_HANDLE, graphicsPool = VK_NULL_HANDLE;
    VkSemaphore ready = VK_NULL_HANDLE;
    VkFence finished = VK_NULL_HANDLE;
    try {
        VkDescriptorSetLayoutBinding bindings[2]{};
        for (std::uint32_t i = 0; i < 2; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        }
        VkDescriptorSetLayoutCreateInfo setInfo{};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setInfo.bindingCount = 2; setInfo.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &setLayout));
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1; layoutInfo.pSetLayouts = &setLayout;
        VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &graphicsLayout));
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VkRenderPassCreateInfo rp{}; rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp.subpassCount = 1; rp.pSubpasses = &subpass;
        VK_CHECK(vkCreateRenderPass(device, &rp, nullptr, &renderPass));
        VkFramebufferCreateInfo fb{}; fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = renderPass; fb.width = 1; fb.height = 1; fb.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fb, nullptr, &framebuffer));

        auto vert = rwb::rhi::ShaderModule::fromFile(device, shaderPath("particles_consume.vert.spv"));
        auto stage = vert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT);
        VkPipelineVertexInputStateCreateInfo vi{}; vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo ia{}; ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; ia.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        VkViewport viewport{0, 0, 1, 1, 0, 1}; VkRect2D scissor{{0,0},{1,1}};
        VkPipelineViewportStateCreateInfo vp{}; vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vp.viewportCount=1; vp.pViewports=&viewport; vp.scissorCount=1; vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{}; rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; rs.rasterizerDiscardEnable=VK_TRUE; rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{}; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendStateCreateInfo cb{}; cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        VkGraphicsPipelineCreateInfo gp{}; gp.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; gp.stageCount=1; gp.pStages=&stage; gp.pVertexInputState=&vi; gp.pInputAssemblyState=&ia; gp.pViewportState=&vp; gp.pRasterizationState=&rs; gp.pMultisampleState=&ms; gp.pColorBlendState=&cb; gp.layout=graphicsLayout; gp.renderPass=renderPass;
        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &graphicsPipeline));

        VkDescriptorSetAllocateInfo da{}; da.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; da.descriptorPool=descriptorPool(); da.descriptorSetCount=1; da.pSetLayouts=&setLayout;
        VkDescriptorSet graphicsSet = VK_NULL_HANDLE; VK_CHECK(vkAllocateDescriptorSets(device, &da, &graphicsSet));
        VkDescriptorBufferInfo infos[]={{particles.handle(),0,VK_WHOLE_SIZE},{consumed.handle(),0,VK_WHOLE_SIZE}};
        VkWriteDescriptorSet writes[2]{};
        for(std::uint32_t i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=graphicsSet;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];}
        vkUpdateDescriptorSets(device,2,writes,0,nullptr);

        computePool=makePool(device,computeFamily); graphicsPool=makePool(device,graphicsFamily);
        VkCommandBuffer cc=beginOneShot(device,computePool), gc=beginOneShot(device,graphicsPool);
        vkCmdBindPipeline(cc,VK_PIPELINE_BIND_POINT_COMPUTE,update.get().pipeline);
        vkCmdBindDescriptorSets(cc,VK_PIPELINE_BIND_POINT_COMPUTE,update.get().layout,0,1,&updateSet,0,nullptr);
        const ParticlePush push{n,deltaX,deltaY}; vkCmdPushConstants(cc,update.get().layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
        vkCmdDispatch(cc,(n+kWorkgroupSize-1)/kWorkgroupSize,1,1);
        VkBufferMemoryBarrier release{}; release.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; release.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; release.dstAccessMask=transfer?0:VK_ACCESS_SHADER_READ_BIT; release.srcQueueFamilyIndex=transfer?computeFamily:VK_QUEUE_FAMILY_IGNORED; release.dstQueueFamilyIndex=transfer?graphicsFamily:VK_QUEUE_FAMILY_IGNORED; release.buffer=particles.handle(); release.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cc,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,transfer?VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,0,nullptr,1,&release,0,nullptr); VK_CHECK(vkEndCommandBuffer(cc));
        if(transfer){VkBufferMemoryBarrier acquire=release;acquire.srcAccessMask=0;acquire.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;vkCmdPipelineBarrier(gc,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,0,nullptr,1,&acquire,0,nullptr);}
        VkRenderPassBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;begin.renderPass=renderPass;begin.framebuffer=framebuffer;begin.renderArea.extent={1,1};vkCmdBeginRenderPass(gc,&begin,VK_SUBPASS_CONTENTS_INLINE);vkCmdBindPipeline(gc,VK_PIPELINE_BIND_POINT_GRAPHICS,graphicsPipeline);vkCmdBindDescriptorSets(gc,VK_PIPELINE_BIND_POINT_GRAPHICS,graphicsLayout,0,1,&graphicsSet,0,nullptr);vkCmdDraw(gc,n,1,0,0);vkCmdEndRenderPass(gc);
        VkMemoryBarrier host{};host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;host.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier(gc,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&host,0,nullptr,0,nullptr);VK_CHECK(vkEndCommandBuffer(gc));
        VkSemaphoreCreateInfo si{};si.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;VK_CHECK(vkCreateSemaphore(device,&si,nullptr,&ready));VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;VK_CHECK(vkCreateFence(device,&fi,nullptr,&finished));
        VkSubmitInfo cs{};cs.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;cs.commandBufferCount=1;cs.pCommandBuffers=&cc;cs.signalSemaphoreCount=1;cs.pSignalSemaphores=&ready;VK_CHECK(vkQueueSubmit(m_ctx->computeQueue(),1,&cs,VK_NULL_HANDLE));
        VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;VkSubmitInfo gs{};gs.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;gs.waitSemaphoreCount=1;gs.pWaitSemaphores=&ready;gs.pWaitDstStageMask=&waitStage;gs.commandBufferCount=1;gs.pCommandBuffers=&gc;VK_CHECK(vkQueueSubmit(m_ctx->graphicsQueue(),1,&gs,finished));m_lastQueueSync.semaphoreWaitedByGraphics=true;VK_CHECK(vkWaitForFences(device,1,&finished,VK_TRUE,UINT64_MAX));
    } catch (...) {
        if (finished) vkDestroyFence(device,finished,nullptr); if(ready)vkDestroySemaphore(device,ready,nullptr); if(graphicsPool)vkDestroyCommandPool(device,graphicsPool,nullptr); if(computePool)vkDestroyCommandPool(device,computePool,nullptr); if(graphicsPipeline)vkDestroyPipeline(device,graphicsPipeline,nullptr); if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr); if(renderPass)vkDestroyRenderPass(device,renderPass,nullptr); if(graphicsLayout)vkDestroyPipelineLayout(device,graphicsLayout,nullptr); if(setLayout)vkDestroyDescriptorSetLayout(device,setLayout,nullptr); throw;
    }
    vkDestroyFence(device,finished,nullptr);vkDestroySemaphore(device,ready,nullptr);vkDestroyCommandPool(device,graphicsPool,nullptr);vkDestroyCommandPool(device,computePool,nullptr);vkDestroyPipeline(device,graphicsPipeline,nullptr);vkDestroyFramebuffer(device,framebuffer,nullptr);vkDestroyRenderPass(device,renderPass,nullptr);vkDestroyPipelineLayout(device,graphicsLayout,nullptr);vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
    return rwb::rhi::readbackBufferAs<float>(*m_ctx,consumed.handle(),positions.size());
}

} // namespace p03
