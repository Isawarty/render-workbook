// t03 —— 分块 exclusive prefix sum
//
// 任务书: projects/p03-compute/docs/t03-scan.md
// 判分:   python rwb.py test p03-t03

#include "../ComputeApp.h"

#include "rwb/core/Todo.h"

namespace p03 {

std::vector<std::uint32_t> ComputeApp::runScan(const std::vector<std::uint32_t>& data) {
    // TODO(p03-t03): 三趟 dispatch：块内 scan -> block sums scan -> 偏移加回。
    // 两趟之间要 shader-write -> shader-read，最后通向 transfer-read。
    RWB_TODO("p03-t03 ComputeApp::runScan");
}

} // namespace p03
