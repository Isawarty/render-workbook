// t06 —— GPU particles / cross-queue synchronization
// 任务书: projects/p03-compute/docs/t06-particles.md
// 判分:   python rwb.py test p03-t06
#include "../ComputeApp.h"
#include "rwb/core/Todo.h"
namespace p03 {
std::vector<float> ComputeApp::runParticles(const std::vector<float>& positions, float deltaX, float deltaY) {
    // TODO(p03-t06): compute 更新；semaphore 交给 graphics；跨 family 时做
    // release/acquire ownership transfer，同 family 时做普通 memory dependency。
    RWB_TODO("p03-t06 ComputeApp::runParticles");
}
} // namespace p03
