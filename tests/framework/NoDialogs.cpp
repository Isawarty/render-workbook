// 防御性设置：任何情况下测试进程都不得弹出模态对话框。
//
// 为什么需要：一旦某处触发 abort()（比如析构函数里抛异常导致 std::terminate），
// MSVC 的 CRT 默认会弹一个「中止/重试/忽略」窗口。在批量跑 ctest 或在 CI 上，
// 这会变成无人应答的挂起，甚至弹窗刷屏。
//
// 测试进程应该干脆地失败并留下退出码，而不是等人点按钮。
// 注意: 本文件在静态库里, 如果没有任何符号被引用, 链接器会整个丢掉它 ——
// 静态初始化也就不会发生。所以对外暴露 installCrashDialogSuppression(),
// 由 ImageCompare.cpp 显式引用一次。
#include "NoDialogs.h"

#if defined(_WIN32)
#include <windows.h>
#include <crtdbg.h>
#include <cstdlib>

namespace rwb::test {

int installCrashDialogSuppression() {
        // 不弹 Windows Error Reporting / 崩溃对话框
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        // abort() 不弹「中止/重试/忽略」, 也不走 WER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        // assert / CRT 错误改为输出到 stderr 而不是弹窗
    const int modes[] = {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT};
    for (int mode : modes) {
        _CrtSetReportMode(mode, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(mode, _CRTDBG_FILE_STDERR);
    }
    return 1;
}

} // namespace rwb::test
#else
namespace rwb::test {
int installCrashDialogSuppression() { return 0; }   // 非 Windows 平台无需处理
}
#endif
