#include "rwb/rhi/Shader.h"

#include "rwb/core/File.h"

#include <utility>

namespace rwb::rhi {

ShaderModule::ShaderModule(VkDevice device, const std::vector<std::uint32_t>& spirv)
    : m_device(device) {
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    // 注意单位：codeSize 是「字节数」，而 pCode 是 uint32_t 指针。
    // 传 spirv.size() 会得到四分之一大小的模块，报错信息还相当难懂。
    info.codeSize = spirv.size() * sizeof(std::uint32_t);
    info.pCode    = spirv.data();
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &m_module));
}

ShaderModule ShaderModule::fromFile(VkDevice device, const std::string& path) {
    // readSpirv 顺便校验了 magic number 和 4 字节对齐 ——
    // 「shader 没编译出来」和「pipeline 建错了」是两种完全不同的病。
    return ShaderModule(device, readSpirv(path));
}

ShaderModule::~ShaderModule() { reset(); }

ShaderModule::ShaderModule(ShaderModule&& other) noexcept
    : m_device(other.m_device), m_module(other.m_module) {
    other.m_device = VK_NULL_HANDLE;
    other.m_module = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept {
    if (this != &other) {
        reset();
        m_device       = other.m_device;
        m_module       = other.m_module;
        other.m_device = VK_NULL_HANDLE;
        other.m_module = VK_NULL_HANDLE;
    }
    return *this;
}

void ShaderModule::reset() noexcept {
    if (m_module != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_module, nullptr);
    }
    m_module = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

VkPipelineShaderStageCreateInfo ShaderModule::stageInfo(VkShaderStageFlagBits stage,
                                                        const char* entry) const {
    VkPipelineShaderStageCreateInfo info{};
    info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage  = stage;
    info.module = m_module;
    info.pName  = entry;
    return info;
}

} // namespace rwb::rhi
