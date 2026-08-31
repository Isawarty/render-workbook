#include "LuaMath.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace p06 {
namespace {

constexpr const char* vec3Metatable = "rwb.vec3";
constexpr const char* mat4Metatable = "rwb.mat4";

Vec3* vec(lua_State* state, int index) {
    return static_cast<Vec3*>(luaL_checkudata(state, index, vec3Metatable));
}

Mat4* mat(lua_State* state, int index) {
    return static_cast<Mat4*>(luaL_checkudata(state, index, mat4Metatable));
}

int newVec3(lua_State* state) {
    pushVec3(state, {luaL_checknumber(state, 1), luaL_checknumber(state, 2),
                     luaL_checknumber(state, 3)});
    return 1;
}

int vecIndex(lua_State* state) {
    const Vec3 value = *vec(state, 1);
    const char* key = luaL_checkstring(state, 2);
    if (std::strcmp(key, "x") == 0) lua_pushnumber(state, value.x);
    else if (std::strcmp(key, "y") == 0) lua_pushnumber(state, value.y);
    else if (std::strcmp(key, "z") == 0) lua_pushnumber(state, value.z);
    else lua_pushnil(state);
    return 1;
}

int vecAdd(lua_State* state) {
    const Vec3 a = *vec(state, 1);
    const Vec3 b = *vec(state, 2);
    pushVec3(state, {a.x + b.x, a.y + b.y, a.z + b.z});
    return 1;
}

int vecSub(lua_State* state) {
    const Vec3 a = *vec(state, 1);
    const Vec3 b = *vec(state, 2);
    pushVec3(state, {a.x - b.x, a.y - b.y, a.z - b.z});
    return 1;
}

int vecMul(lua_State* state) {
    if (lua_isnumber(state, 1)) {
        const double scalar = lua_tonumber(state, 1);
        const Vec3 value = *vec(state, 2);
        pushVec3(state, {value.x * scalar, value.y * scalar, value.z * scalar});
    } else {
        const Vec3 value = *vec(state, 1);
        const double scalar = luaL_checknumber(state, 2);
        pushVec3(state, {value.x * scalar, value.y * scalar, value.z * scalar});
    }
    return 1;
}

int vecDot(lua_State* state) {
    const Vec3 a = *vec(state, 1);
    const Vec3 b = *vec(state, 2);
    lua_pushnumber(state, a.x * b.x + a.y * b.y + a.z * b.z);
    return 1;
}

int vecLength(lua_State* state) {
    const Vec3 value = *vec(state, 1);
    lua_pushnumber(state, std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z));
    return 1;
}

int vecToString(lua_State* state) {
    const Vec3 value = *vec(state, 1);
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "vec3(%.3f, %.3f, %.3f)",
                  value.x, value.y, value.z);
    lua_pushstring(state, buffer);
    return 1;
}

Mat4 identity() {
    Mat4 value;
    value.values[0] = value.values[5] = value.values[10] = value.values[15] = 1.0;
    return value;
}

int matIdentity(lua_State* state) {
    pushMat4(state, identity());
    return 1;
}

int matTranslation(lua_State* state) {
    const Vec3 offset = *vec(state, 1);
    Mat4 value = identity();
    value.values[12] = offset.x;
    value.values[13] = offset.y;
    value.values[14] = offset.z;
    pushMat4(state, value);
    return 1;
}

int matMul(lua_State* state) {
    const Mat4 left = *mat(state, 1);
    if (luaL_testudata(state, 2, vec3Metatable) != nullptr) {
        const Vec3 right = *vec(state, 2);
        pushVec3(state, {
            left.values[0] * right.x + left.values[4] * right.y +
                left.values[8] * right.z + left.values[12],
            left.values[1] * right.x + left.values[5] * right.y +
                left.values[9] * right.z + left.values[13],
            left.values[2] * right.x + left.values[6] * right.y +
                left.values[10] * right.z + left.values[14],
        });
        return 1;
    }
    const Mat4 right = *mat(state, 2);
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += left.values[k * 4 + row] * right.values[column * 4 + k];
            }
            result.values[column * 4 + row] = sum;
        }
    }
    pushMat4(state, result);
    return 1;
}

void createMetatable(lua_State* state, const char* name,
                     const luaL_Reg* functions) {
    luaL_newmetatable(state, name);
    luaL_setfuncs(state, functions, 0);
    lua_pop(state, 1);
}

} // namespace

void pushVec3(lua_State* state, Vec3 value) {
    auto* storage = static_cast<Vec3*>(lua_newuserdatauv(state, sizeof(Vec3), 0));
    *storage = value;
    luaL_setmetatable(state, vec3Metatable);
}

void pushMat4(lua_State* state, const Mat4& value) {
    auto* storage = static_cast<Mat4*>(lua_newuserdatauv(state, sizeof(Mat4), 0));
    *storage = value;
    luaL_setmetatable(state, mat4Metatable);
}

Vec3 checkVec3(lua_State* state, int index) { return *vec(state, index); }
Mat4 checkMat4(lua_State* state, int index) { return *mat(state, index); }

void registerMathBindings(LuaVm& vm) {
    lua_State* state = vm.state();
    LuaStackGuard guard(state);
    const luaL_Reg vecMeta[] = {
        {"__index", vecIndex}, {"__add", vecAdd}, {"__sub", vecSub},
        {"__mul", vecMul}, {"__tostring", vecToString}, {nullptr, nullptr}};
    createMetatable(state, vec3Metatable, vecMeta);
    const luaL_Reg matMeta[] = {{"__mul", matMul}, {nullptr, nullptr}};
    createMetatable(state, mat4Metatable, matMeta);

    const luaL_Reg vecLibrary[] = {
        {"new", newVec3}, {"dot", vecDot}, {"length", vecLength}, {nullptr, nullptr}};
    luaL_newlib(state, vecLibrary);
    lua_setglobal(state, "vec3");
    const luaL_Reg matLibrary[] = {
        {"identity", matIdentity}, {"translation", matTranslation}, {nullptr, nullptr}};
    luaL_newlib(state, matLibrary);
    lua_setglobal(state, "mat4");
}

} // namespace p06
