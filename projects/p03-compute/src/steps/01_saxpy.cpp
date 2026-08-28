// t01 —— compute pipeline / storage buffer / dispatch / 回读
//
// 任务书: projects/p03-compute/docs/t01-saxpy.md
// 判分:   python rwb.py test p03-t01

#include "../ComputeApp.h"

#include "rwb/core/Todo.h"

namespace p03 {

VkDescriptorPool ComputeApp::descriptorPool() {
    // TODO(p03-t01): 惰性创建 storage-buffer descriptor pool。
    RWB_TODO("p03-t01 ComputeApp::descriptorPool");
}

ComputePipeline ComputeApp::createComputePipeline(const std::string& spvName,
                                                  std::uint32_t storageBindingCount,
                                                  std::uint32_t pushConstantSize) {
    // TODO(p03-t01): set layout -> pipeline layout -> compute pipeline。
    // 中途失败时必须释放已经创建的半成品。
    RWB_TODO("p03-t01 ComputeApp::createComputePipeline");
}

VkDescriptorSet ComputeApp::allocateStorageSet(const ComputePipeline& pipe,
                                               const std::vector<const Buffer*>& buffers) {
    // TODO(p03-t01): 分配 set，把 buffers 依次写入 binding 0..N-1。
    RWB_TODO("p03-t01 ComputeApp::allocateStorageSet");
}

std::vector<float> ComputeApp::runSaxpy(float a, const std::vector<float>& x,
                                        const std::vector<float>& y) {
    // TODO(p03-t01): 建资源、上传、bind、push、dispatch、barrier、回读。
    // workgroup 数必须向上取整；dispatch 写到 transfer read 之间要有依赖。
    RWB_TODO("p03-t01 ComputeApp::runSaxpy");
}

} // namespace p03
