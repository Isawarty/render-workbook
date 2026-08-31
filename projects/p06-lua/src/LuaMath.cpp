#include "LuaMath.h"

namespace p06 {

void registerMathBindings(LuaVm&) {
    throw LuaError("尚未实现: p06-t02 registerMathBindings");
}

Vec3 checkVec3(lua_State*, int) {
    throw LuaError("尚未实现: p06-t02 checkVec3");
}

Mat4 checkMat4(lua_State*, int) {
    throw LuaError("尚未实现: p06-t02 checkMat4");
}

void pushVec3(lua_State*, Vec3) {
    throw LuaError("尚未实现: p06-t02 pushVec3");
}

void pushMat4(lua_State*, const Mat4&) {
    throw LuaError("尚未实现: p06-t02 pushMat4");
}

} // namespace p06
