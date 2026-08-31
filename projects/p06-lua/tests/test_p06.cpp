#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "LuaMath.h"
#include "LuaRenderGraph.h"
#include "LuaVm.h"
#include "SceneScript.h"
#include "ScriptRuntime.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace p06;

namespace {

const std::string projectPath = "projects/p06-lua/";

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << text;
}

} // namespace

TEST_CASE("t01 Lua VM 保持栈平衡并给出 traceback", "[t01]") {
    LuaVm vm;
    REQUIRE(vm.stackTop() == 0);
    vm.runString("answer = 6 * 7", "answer-chunk");
    REQUIRE(vm.stackTop() == 0);
    lua_getglobal(vm.state(), "answer");
    REQUIRE(lua_tointeger(vm.state(), -1) == 42);
    lua_pop(vm.state(), 1);

    try {
        vm.runString("local function inner() error('broken') end; inner()", "trace-test");
        FAIL("expected LuaError");
    } catch (const LuaError& error) {
        const std::string message = error.what();
        REQUIRE(message.find("broken") != std::string::npos);
        REQUIRE(message.find("stack traceback") != std::string::npos);
        REQUIRE(message.find("trace-test") != std::string::npos);
    }
    REQUIRE(vm.stackTop() == 0);
}

TEST_CASE("t01 stack guard 恢复调用前高度", "[t01]") {
    LuaVm vm;
    lua_pushinteger(vm.state(), 7);
    {
        LuaStackGuard guard(vm.state());
        lua_pushstring(vm.state(), "temporary");
        REQUIRE_FALSE(guard.balanced());
    }
    REQUIRE(vm.stackTop() == 1);
    REQUIRE(lua_tointeger(vm.state(), -1) == 7);
    lua_pop(vm.state(), 1);
}

TEST_CASE("t02 vec3 与 mat4 userdata 支持字段和运算", "[t02]") {
    LuaVm vm;
    registerMathBindings(vm);
    vm.runString(R"(
        local a = vec3.new(1, 2, 3)
        local b = vec3.new(4, 5, 6)
        sum = a + b
        scaled = 2 * a
        dot_value = vec3.dot(a, b)
        transformed = mat4.translation(vec3.new(10, 20, 30)) * a
    )", "math-test");

    lua_getglobal(vm.state(), "sum");
    const Vec3 sum = checkVec3(vm.state(), -1);
    lua_pop(vm.state(), 1);
    REQUIRE(sum.x == 5.0);
    REQUIRE(sum.y == 7.0);
    REQUIRE(sum.z == 9.0);

    lua_getglobal(vm.state(), "scaled");
    const Vec3 scaled = checkVec3(vm.state(), -1);
    lua_pop(vm.state(), 1);
    REQUIRE(scaled.x == 2.0);
    REQUIRE(scaled.y == 4.0);
    REQUIRE(scaled.z == 6.0);

    lua_getglobal(vm.state(), "dot_value");
    REQUIRE(lua_tonumber(vm.state(), -1) == 32.0);
    lua_pop(vm.state(), 1);

    lua_getglobal(vm.state(), "transformed");
    const Vec3 transformed = checkVec3(vm.state(), -1);
    lua_pop(vm.state(), 1);
    REQUIRE(transformed.x == 11.0);
    REQUIRE(transformed.y == 22.0);
    REQUIRE(transformed.z == 33.0);
    REQUIRE(vm.stackTop() == 0);
}

TEST_CASE("t03 Lua 场景被解析为强类型材质和实体", "[t03]") {
    LuaVm vm;
    registerMathBindings(vm);
    const SceneDesc scene = loadSceneScript(vm, projectPath + "assets/scene.lua");
    REQUIRE(scene.materials.size() == 2);
    REQUIRE(scene.materials[0].name == "helmet");
    REQUIRE(scene.materials[0].metallic == 0.85);
    REQUIRE(scene.materials[1].roughness == 0.92);
    REQUIRE(scene.entities.size() == 2);
    REQUIRE(scene.entities[0].mesh == "SciFiHelmet.glb");
    REQUIRE(scene.entities[0].position.y == 0.6);
    REQUIRE(vm.stackTop() == 0);
}

TEST_CASE("t03 场景错误包含字段路径且不泄漏 Lua 栈", "[t03]") {
    LuaVm vm;
    registerMathBindings(vm);
    REQUIRE_THROWS_WITH(
        loadSceneScript(vm, projectPath + "assets/invalid_scene.lua"),
        Catch::Matchers::ContainsSubstring("materials[1].roughness"));
    REQUIRE(vm.stackTop() == 0);
}

TEST_CASE("t04 Lua 声明真实 P5 render graph 并推导依赖", "[t04]") {
    LuaVm vm;
    const auto graph = loadRenderGraphScript(vm, projectPath + "assets/render_graph.lua");
    REQUIRE(graph.resources().size() == 3);
    REQUIRE(graph.passes().size() == 3);
    REQUIRE(graph.passes()[0].name == "Geometry");
    REQUIRE(graph.passes()[1].name == "Lighting");
    REQUIRE(graph.passes()[2].name == "Tonemap");
    REQUIRE(graph.passes()[1].dependencies.size() == 1);
    REQUIRE(graph.passes()[2].dependencies.size() == 1);
    REQUIRE(graph.toDot().find("GBuffer") != std::string::npos);
    REQUIRE(vm.stackTop() == 0);
}

TEST_CASE("t04 Lua graph 拒绝未知资源并保持栈平衡", "[t04]") {
    LuaVm vm;
    REQUIRE_THROWS_WITH(
        loadRenderGraphScript(vm, projectPath + "assets/invalid_render_graph.lua"),
        Catch::Matchers::ContainsSubstring("passes[1].reads[1]"));
    REQUIRE(vm.stackTop() == 0);
}

TEST_CASE("t05 sandbox 移除危险库并限制指令数", "[t05]") {
    LuaSandbox sandbox(5000);
    sandbox.runString("safe = (io == nil and os == nil and debug == nil and package == nil)");
    REQUIRE(sandbox.globalNumber("safe") == 1.0);
    REQUIRE_THROWS_WITH(sandbox.runString("while true do end", "runaway"),
                        Catch::Matchers::ContainsSubstring("instruction budget"));
}

TEST_CASE("t05 coroutine scheduler 每帧只推进到下一个 yield", "[t05]") {
    LuaSandbox sandbox;
    sandbox.runString("ticks = 0");
    CoroutineScheduler scheduler(sandbox);
    scheduler.add("animation", R"(
        return function()
            for _ = 1, 3 do
                ticks = ticks + 1
                coroutine.yield()
            end
        end
    )");
    REQUIRE(scheduler.activeCount() == 1);
    scheduler.tick();
    REQUIRE(sandbox.globalNumber("ticks") == 1.0);
    scheduler.tick();
    REQUIRE(sandbox.globalNumber("ticks") == 2.0);
    scheduler.tick();
    REQUIRE(sandbox.globalNumber("ticks") == 3.0);
    scheduler.tick();
    REQUIRE(scheduler.activeCount() == 0);
    REQUIRE(scheduler.completedCount() == 1);
}

TEST_CASE("t05 热重载只在候选脚本成功后替换运行时", "[t05]") {
    const auto path = std::filesystem::temp_directory_path() / "rwb-p06-hot-reload.lua";
    writeText(path, "exposure = 1.25");
    HotReloadRuntime runtime;
    REQUIRE(runtime.reload(path.string()));
    REQUIRE(runtime.generation() == 1);
    REQUIRE(runtime.globalNumber("exposure") == 1.25);
    REQUIRE_FALSE(runtime.reload(path.string()));

    writeText(path, "exposure = ");
    REQUIRE_FALSE(runtime.reload(path.string()));
    REQUIRE(runtime.generation() == 1);
    REQUIRE(runtime.globalNumber("exposure") == 1.25);
    REQUIRE_FALSE(runtime.lastError().empty());

    writeText(path, "exposure = 2.0");
    REQUIRE(runtime.reload(path.string()));
    REQUIRE(runtime.generation() == 2);
    REQUIRE(runtime.globalNumber("exposure") == 2.0);
    std::filesystem::remove(path);
}
