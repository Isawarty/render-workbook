#include "LuaRenderGraph.h"

#include <unordered_map>

namespace p06 {
namespace {

using namespace rwb::rg;

void expect(lua_State* state, int index, int type, const std::string& path) {
    if (lua_type(state, index) != type) {
        throw LuaError(path + " expected " + lua_typename(state, type) + ", got " +
                       luaL_typename(state, index));
    }
}

std::string getString(lua_State* state, int table, const char* field,
                      const std::string& path) {
    lua_getfield(state, lua_absindex(state, table), field);
    expect(state, -1, LUA_TSTRING, path + "." + field);
    std::string value = lua_tostring(state, -1);
    lua_pop(state, 1);
    return value;
}

bool getBoolean(lua_State* state, int table, const char* field, bool fallback) {
    lua_getfield(state, lua_absindex(state, table), field);
    const bool value = lua_isnil(state, -1) ? fallback : lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return value;
}

std::uint64_t getInteger(lua_State* state, int table, const char* field,
                         std::uint64_t fallback, const std::string& path) {
    lua_getfield(state, lua_absindex(state, table), field);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return fallback;
    }
    if (!lua_isinteger(state, -1)) throw LuaError(path + "." + field + " expected integer");
    const lua_Integer value = lua_tointeger(state, -1);
    lua_pop(state, 1);
    if (value < 0) throw LuaError(path + "." + field + " must be non-negative");
    return static_cast<std::uint64_t>(value);
}

ResourceState stateFor(const std::string& name, const std::string& path) {
    if (name == "color-write") {
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }
    if (name == "shader-read") {
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }
    if (name == "compute-write") {
        return {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    }
    if (name == "present") {
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    }
    throw LuaError(path + ".state has unknown value '" + name + "'");
}

void loadTable(lua_State* state, const std::string& path) {
    if (luaL_loadfile(state, path.c_str()) != LUA_OK) {
        const std::string message = lua_tostring(state, -1);
        lua_pop(state, 1);
        throw LuaError(path + ": " + message);
    }
    checkedCall(state, 0, 1, path);
    expect(state, -1, LUA_TTABLE, path + " return value");
}

void addUses(lua_State* state, int passTable, const char* fieldName, Access access,
             const std::string& passPath,
             const std::unordered_map<std::string, ResourceHandle>& resources,
             PassBuilder& builder) {
    lua_getfield(state, lua_absindex(state, passTable), fieldName);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return;
    }
    expect(state, -1, LUA_TTABLE, passPath + "." + fieldName);
    const int uses = lua_absindex(state, -1);
    const lua_Integer count = luaL_len(state, uses);
    for (lua_Integer i = 1; i <= count; ++i) {
        lua_geti(state, uses, i);
        const std::string usePath = passPath + "." + fieldName + "[" +
                                    std::to_string(i) + "]";
        expect(state, -1, LUA_TTABLE, usePath);
        const std::string resourceName = getString(state, -1, "resource", usePath);
        const auto found = resources.find(resourceName);
        if (found == resources.end()) throw LuaError(usePath + ": unknown resource '" + resourceName + "'");
        const ResourceState resourceState = stateFor(getString(state, -1, "state", usePath), usePath);
        if (access == Access::Read) builder.read(found->second, resourceState);
        else builder.write(found->second, resourceState);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

} // namespace

CompiledGraph loadRenderGraphScript(LuaVm& vm, const std::string& path) {
    lua_State* state = vm.state();
    LuaStackGuard guard(state);
    loadTable(state, path);
    const int root = lua_absindex(state, -1);
    RenderGraph graph;
    std::unordered_map<std::string, ResourceHandle> resources;

    lua_getfield(state, root, "resources");
    expect(state, -1, LUA_TTABLE, "resources");
    const int resourceTable = lua_absindex(state, -1);
    const lua_Integer resourceCount = luaL_len(state, resourceTable);
    for (lua_Integer i = 1; i <= resourceCount; ++i) {
        lua_geti(state, resourceTable, i);
        const std::string itemPath = "resources[" + std::to_string(i) + "]";
        expect(state, -1, LUA_TTABLE, itemPath);
        ResourceDesc desc;
        desc.name = getString(state, -1, "name", itemPath);
        const std::string kind = getString(state, -1, "kind", itemPath);
        if (kind == "image") desc.kind = ResourceKind::Image;
        else if (kind == "buffer") desc.kind = ResourceKind::Buffer;
        else throw LuaError(itemPath + ".kind must be 'image' or 'buffer'");
        desc.sizeBytes = getInteger(state, -1, "size", 0, itemPath);
        desc.compatibilityKey = getInteger(state, -1, "compatibility", 0, itemPath);
        const bool imported = getBoolean(state, -1, "imported", false);
        const ResourceHandle handle = imported ? graph.importResource(desc) : graph.createResource(desc);
        if (!resources.emplace(desc.name, handle).second) {
            throw LuaError(itemPath + ".name duplicates resource '" + desc.name + "'");
        }
        lua_pop(state, 1);
    }
    lua_pop(state, 1);

    lua_getfield(state, root, "passes");
    expect(state, -1, LUA_TTABLE, "passes");
    const int passTable = lua_absindex(state, -1);
    const lua_Integer passCount = luaL_len(state, passTable);
    for (lua_Integer i = 1; i <= passCount; ++i) {
        lua_geti(state, passTable, i);
        const std::string itemPath = "passes[" + std::to_string(i) + "]";
        expect(state, -1, LUA_TTABLE, itemPath);
        const int pass = lua_absindex(state, -1);
        const std::string name = getString(state, pass, "name", itemPath);
        graph.addPass(name, [&](PassBuilder& builder) {
            addUses(state, pass, "reads", Access::Read, itemPath, resources, builder);
            addUses(state, pass, "writes", Access::Write, itemPath, resources, builder);
        });
        lua_pop(state, 1);
    }
    return graph.compile();
}

} // namespace p06
