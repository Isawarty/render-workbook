#include "LuaVm.h"

#include <utility>

namespace p06 {
namespace {

int traceback(lua_State* state) {
    const char* message = lua_tostring(state, 1);
    if (message == nullptr) message = "(non-string Lua error)";
    luaL_traceback(state, state, message, 1);
    return 1;
}

std::string popError(lua_State* state, const std::string& context) {
    const char* text = lua_tostring(state, -1);
    const std::string message = context + ": " + (text ? text : "unknown Lua error");
    lua_pop(state, 1);
    return message;
}

} // namespace

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

void checkedCall(lua_State* state, int arguments, int results,
                 const std::string& context) {
    const int functionIndex = lua_gettop(state) - arguments;
    lua_pushcfunction(state, traceback);
    lua_insert(state, functionIndex);
    const int status = lua_pcall(state, arguments, results, functionIndex);
    lua_remove(state, functionIndex);
    if (status != LUA_OK) throw LuaError(popError(state, context));
}

void LuaVm::runString(const std::string& source, const std::string& chunkName) {
    LuaStackGuard guard(m_state);
    if (luaL_loadbuffer(m_state, source.data(), source.size(), chunkName.c_str()) != LUA_OK) {
        throw LuaError(popError(m_state, chunkName));
    }
    checkedCall(m_state, 0, 0, chunkName);
}

void LuaVm::runFile(const std::string& path) {
    LuaStackGuard guard(m_state);
    if (luaL_loadfile(m_state, path.c_str()) != LUA_OK) {
        throw LuaError(popError(m_state, path));
    }
    checkedCall(m_state, 0, 0, path);
}

} // namespace p06
