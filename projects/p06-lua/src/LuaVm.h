#pragma once

#include <lua.hpp>

#include <stdexcept>
#include <string>

namespace p06 {

class LuaError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class LuaVm {
public:
    explicit LuaVm(bool openStandardLibraries = true);
    ~LuaVm();
    LuaVm(const LuaVm&) = delete;
    LuaVm& operator=(const LuaVm&) = delete;
    LuaVm(LuaVm&& other) noexcept;
    LuaVm& operator=(LuaVm&& other) noexcept;

    lua_State* state() const { return m_state; }
    int stackTop() const { return lua_gettop(m_state); }
    void runString(const std::string& source, const std::string& chunkName = "chunk");
    void runFile(const std::string& path);

private:
    lua_State* m_state = nullptr;
};

class LuaStackGuard {
public:
    explicit LuaStackGuard(lua_State* state) : m_state(state), m_top(lua_gettop(state)) {}
    ~LuaStackGuard() { lua_settop(m_state, m_top); }
    bool balanced() const { return lua_gettop(m_state) == m_top; }
    int initialTop() const { return m_top; }

private:
    lua_State* m_state;
    int m_top;
};

void checkedCall(lua_State* state, int arguments, int results,
                 const std::string& context);

} // namespace p06
