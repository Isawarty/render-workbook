#pragma once

namespace rwb::test {

// 关掉 Windows 上的 abort/崩溃/assert 模态对话框，让测试进程干脆地失败。
// 由 ImageCompare.cpp 在静态初始化期引用一次，确保这个 TU 不被链接器丢弃。
// 非 Windows 平台是空实现。
int installCrashDialogSuppression();

} // namespace rwb::test
