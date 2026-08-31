#pragma once

#include "LuaVm.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace p06 {

class LuaSandbox {
public:
    explicit LuaSandbox(std::size_t instructionBudget = 100000);
    LuaVm& vm() { return m_vm; }
    const LuaVm& vm() const { return m_vm; }
    std::size_t instructionBudget() const { return m_instructionBudget; }
    void runString(const std::string& source, const std::string& chunkName = "sandbox");
    double globalNumber(const std::string& name) const;

private:
    LuaVm m_vm;
    std::size_t m_instructionBudget;
};

class CoroutineScheduler {
public:
    explicit CoroutineScheduler(LuaSandbox& sandbox) : m_sandbox(sandbox) {}
    void add(const std::string& name, const std::string& source);
    void tick();
    std::size_t activeCount() const;
    std::size_t completedCount() const { return m_completed; }
    ~CoroutineScheduler();

private:
    struct Task { std::string name; int registryRef = LUA_NOREF; bool done = false; };
    LuaSandbox& m_sandbox;
    std::vector<Task> m_tasks;
    std::size_t m_completed = 0;
};

class HotReloadRuntime {
public:
    explicit HotReloadRuntime(std::size_t instructionBudget = 100000)
        : m_instructionBudget(instructionBudget) {}
    bool reload(const std::string& path);
    std::size_t generation() const { return m_generation; }
    const std::string& lastError() const { return m_lastError; }
    double globalNumber(const std::string& name) const;

private:
    std::size_t m_instructionBudget;
    std::string m_content;
    std::unique_ptr<LuaSandbox> m_active;
    std::size_t m_generation = 0;
    std::string m_lastError;
};

} // namespace p06
