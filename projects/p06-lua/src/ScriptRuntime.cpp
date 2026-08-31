#include "ScriptRuntime.h"

#include <fstream>
#include <iterator>

namespace p06 {
namespace {

void budgetExceeded(lua_State* state, lua_Debug*) {
    luaL_error(state, "sandbox instruction budget exceeded");
}

void openLibrary(lua_State* state, const char* name, lua_CFunction function) {
    luaL_requiref(state, name, function, 1);
    lua_pop(state, 1);
}

std::string readFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw LuaError(path + ": cannot open script");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

LuaSandbox::LuaSandbox(std::size_t instructionBudget)
    : m_vm(false), m_instructionBudget(instructionBudget) {
    lua_State* state = m_vm.state();
    openLibrary(state, LUA_GNAME, luaopen_base);
    openLibrary(state, LUA_TABLIBNAME, luaopen_table);
    openLibrary(state, LUA_STRLIBNAME, luaopen_string);
    openLibrary(state, LUA_MATHLIBNAME, luaopen_math);
    openLibrary(state, LUA_UTF8LIBNAME, luaopen_utf8);
    openLibrary(state, LUA_COLIBNAME, luaopen_coroutine);
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");
}

void LuaSandbox::runString(const std::string& source, const std::string& chunkName) {
    lua_State* state = m_vm.state();
    const int count = static_cast<int>(m_instructionBudget > 0 ? m_instructionBudget : 1);
    lua_sethook(state, budgetExceeded, LUA_MASKCOUNT, count);
    try {
        m_vm.runString(source, chunkName);
        lua_sethook(state, nullptr, 0, 0);
    } catch (...) {
        lua_sethook(state, nullptr, 0, 0);
        throw;
    }
}

double LuaSandbox::globalNumber(const std::string& name) const {
    lua_State* state = m_vm.state();
    LuaStackGuard guard(state);
    lua_getglobal(state, name.c_str());
    if (lua_isboolean(state, -1)) return lua_toboolean(state, -1) ? 1.0 : 0.0;
    if (!lua_isnumber(state, -1)) throw LuaError("global '" + name + "' is not numeric");
    return lua_tonumber(state, -1);
}

CoroutineScheduler::~CoroutineScheduler() {
    lua_State* state = m_sandbox.vm().state();
    for (const Task& task : m_tasks) {
        if (task.registryRef != LUA_NOREF) luaL_unref(state, LUA_REGISTRYINDEX, task.registryRef);
    }
}

void CoroutineScheduler::add(const std::string& name, const std::string& source) {
    lua_State* state = m_sandbox.vm().state();
    LuaStackGuard guard(state);
    if (luaL_loadbuffer(state, source.data(), source.size(), name.c_str()) != LUA_OK) {
        const std::string message = lua_tostring(state, -1);
        throw LuaError(name + ": " + message);
    }
    checkedCall(state, 0, 1, name);
    if (!lua_isfunction(state, -1)) throw LuaError(name + ": script must return a function");

    lua_State* thread = lua_newthread(state);
    lua_pushvalue(state, -2);
    lua_xmove(state, thread, 1);
    const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pop(state, 1);
    m_tasks.push_back({name, reference, false});
}

void CoroutineScheduler::tick() {
    lua_State* main = m_sandbox.vm().state();
    for (Task& task : m_tasks) {
        if (task.done) continue;
        lua_rawgeti(main, LUA_REGISTRYINDEX, task.registryRef);
        lua_State* thread = lua_tothread(main, -1);
        lua_sethook(thread, budgetExceeded, LUA_MASKCOUNT,
                    static_cast<int>(m_sandbox.instructionBudget()));
        int results = 0;
        const int status = lua_resume(thread, main, 0, &results);
        lua_sethook(thread, nullptr, 0, 0);
        if (status == LUA_YIELD) {
            lua_pop(thread, results);
        } else if (status == LUA_OK) {
            lua_pop(thread, results);
            task.done = true;
            ++m_completed;
        } else {
            const char* text = lua_tostring(thread, -1);
            const std::string message = task.name + ": " + (text ? text : "coroutine failed");
            lua_pop(thread, 1);
            lua_pop(main, 1);
            throw LuaError(message);
        }
        lua_pop(main, 1);
    }
}

std::size_t CoroutineScheduler::activeCount() const {
    std::size_t active = 0;
    for (const Task& task : m_tasks) {
        if (!task.done) ++active;
    }
    return active;
}

bool HotReloadRuntime::reload(const std::string& path) {
    const std::string content = readFile(path);
    if (content == m_content && m_active) return false;
    auto candidate = std::make_unique<LuaSandbox>(m_instructionBudget);
    try {
        candidate->runString(content, path);
    } catch (const LuaError& error) {
        m_lastError = error.what();
        return false;
    }
    m_active = std::move(candidate);
    m_content = content;
    m_lastError.clear();
    ++m_generation;
    return true;
}

double HotReloadRuntime::globalNumber(const std::string& name) const {
    if (!m_active) throw LuaError("hot reload runtime has no successful script");
    return m_active->globalNumber(name);
}

} // namespace p06
