#include "SceneScript.h"

#include <sstream>

namespace p06 {
namespace {

int absolute(lua_State* state, int index) { return lua_absindex(state, index); }

void requireType(lua_State* state, int index, int type, const std::string& path) {
    if (lua_type(state, index) != type) {
        throw LuaError(path + " expected " + lua_typename(state, type) + ", got " +
                       luaL_typename(state, index));
    }
}

void field(lua_State* state, int table, const char* name) {
    lua_getfield(state, absolute(state, table), name);
}

std::string stringField(lua_State* state, int table, const char* name,
                        const std::string& path) {
    field(state, table, name);
    requireType(state, -1, LUA_TSTRING, path + "." + name);
    std::string value = lua_tostring(state, -1);
    lua_pop(state, 1);
    return value;
}

double numberField(lua_State* state, int table, const char* name,
                   const std::string& path) {
    field(state, table, name);
    requireType(state, -1, LUA_TNUMBER, path + "." + name);
    const double value = lua_tonumber(state, -1);
    lua_pop(state, 1);
    return value;
}

Vec3 vec3Field(lua_State* state, int table, const char* name,
               const std::string& path) {
    field(state, table, name);
    if (luaL_testudata(state, -1, "rwb.vec3") == nullptr) {
        throw LuaError(path + "." + name + " expected vec3 userdata, got " +
                       luaL_typename(state, -1));
    }
    const Vec3 value = checkVec3(state, -1);
    lua_pop(state, 1);
    return value;
}

void loadReturningTable(lua_State* state, const std::string& path) {
    if (luaL_loadfile(state, path.c_str()) != LUA_OK) {
        const std::string message = lua_tostring(state, -1);
        lua_pop(state, 1);
        throw LuaError(path + ": " + message);
    }
    checkedCall(state, 0, 1, path);
    requireType(state, -1, LUA_TTABLE, path + " return value");
}

} // namespace

SceneDesc loadSceneScript(LuaVm& vm, const std::string& path) {
    lua_State* state = vm.state();
    LuaStackGuard guard(state);
    loadReturningTable(state, path);
    const int root = absolute(state, -1);
    SceneDesc result;

    field(state, root, "materials");
    requireType(state, -1, LUA_TTABLE, "materials");
    const int materials = absolute(state, -1);
    const lua_Integer materialCount = luaL_len(state, materials);
    for (lua_Integer i = 1; i <= materialCount; ++i) {
        lua_geti(state, materials, i);
        const std::string itemPath = "materials[" + std::to_string(i) + "]";
        requireType(state, -1, LUA_TTABLE, itemPath);
        const int item = absolute(state, -1);
        result.materials.push_back({
            stringField(state, item, "name", itemPath),
            vec3Field(state, item, "base_color", itemPath),
            numberField(state, item, "metallic", itemPath),
            numberField(state, item, "roughness", itemPath),
        });
        lua_pop(state, 1);
    }
    lua_pop(state, 1);

    field(state, root, "entities");
    requireType(state, -1, LUA_TTABLE, "entities");
    const int entities = absolute(state, -1);
    const lua_Integer entityCount = luaL_len(state, entities);
    for (lua_Integer i = 1; i <= entityCount; ++i) {
        lua_geti(state, entities, i);
        const std::string itemPath = "entities[" + std::to_string(i) + "]";
        requireType(state, -1, LUA_TTABLE, itemPath);
        const int item = absolute(state, -1);
        result.entities.push_back({
            stringField(state, item, "name", itemPath),
            stringField(state, item, "mesh", itemPath),
            stringField(state, item, "material", itemPath),
            vec3Field(state, item, "position", itemPath),
        });
        lua_pop(state, 1);
    }
    return result;
}

} // namespace p06
