#pragma once

#include "LuaVm.h"

#include <array>

namespace p06 {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Mat4 {
    std::array<double, 16> values{};
};

void registerMathBindings(LuaVm& vm);
Vec3 checkVec3(lua_State* state, int index);
Mat4 checkMat4(lua_State* state, int index);
void pushVec3(lua_State* state, Vec3 value);
void pushMat4(lua_State* state, const Mat4& value);

} // namespace p06
