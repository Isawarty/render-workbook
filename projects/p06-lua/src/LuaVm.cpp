#include "LuaVm.h"

#include <utility>

namespace p06 {

LuaVm::LuaVm(bool openStandardLibraries) : m_state(luaL_newstate()) {
    if (m_state == nullptr) throw LuaError("luaL_newstate: allocation failed");
    if (openStandardLibraries) luaL_openlibs(m_state);
}

LuaVm::~LuaVm() {
    if (m_state != nullptr) lua_close(m_state);
}

LuaVm::LuaVm(LuaVm&& other) noexcept : m_state(std::exchange(other.m_state, nullptr)) {}

LuaVm& LuaVm::operator=(LuaVm&& other) noexcept {
    if (this != &other) {
        if (m_state != nullptr) lua_close(m_state);
        m_state = std::exchange(other.m_state, nullptr);
    }
    return *this;
}

void checkedCall(lua_State*, int, int, const std::string&) {
    throw LuaError("尚未实现: p06-t01 checkedCall");
}

void LuaVm::runString(const std::string&, const std::string&) {
    throw LuaError("尚未实现: p06-t01 LuaVm::runString");
}

void LuaVm::runFile(const std::string&) {
    throw LuaError("尚未实现: p06-t01 LuaVm::runFile");
}

} // namespace p06
