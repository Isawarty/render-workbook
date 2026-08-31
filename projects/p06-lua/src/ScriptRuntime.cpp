#include "ScriptRuntime.h"

namespace p06 {

LuaSandbox::LuaSandbox(std::size_t instructionBudget)
    : m_vm(false), m_instructionBudget(instructionBudget) {}

void LuaSandbox::runString(const std::string&, const std::string&) {
    throw LuaError("尚未实现: p06-t05 LuaSandbox::runString");
}

double LuaSandbox::globalNumber(const std::string&) const {
    throw LuaError("尚未实现: p06-t05 LuaSandbox::globalNumber");
}

CoroutineScheduler::~CoroutineScheduler() = default;

void CoroutineScheduler::add(const std::string&, const std::string&) {
    throw LuaError("尚未实现: p06-t05 CoroutineScheduler::add");
}

void CoroutineScheduler::tick() {
    throw LuaError("尚未实现: p06-t05 CoroutineScheduler::tick");
}

std::size_t CoroutineScheduler::activeCount() const { return 0; }

bool HotReloadRuntime::reload(const std::string&) {
    throw LuaError("尚未实现: p06-t05 HotReloadRuntime::reload");
}

double HotReloadRuntime::globalNumber(const std::string&) const {
    throw LuaError("尚未实现: p06-t05 HotReloadRuntime::globalNumber");
}

} // namespace p06
