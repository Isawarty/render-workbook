#include "D3D12App.h"
#include <rwb/core/Todo.h>

namespace p07 {

void D3D12App::createComputePipeline() {
    RWB_TODO("p07-t05 compute root signature / PSO / UAV");
}

std::vector<float> D3D12App::runSaxpy(const std::vector<float>&,
                                      const std::vector<float>&,
                                      float) {
    RWB_TODO("p07-t05 dispatch and read back SAXPY");
}

} // namespace p07
