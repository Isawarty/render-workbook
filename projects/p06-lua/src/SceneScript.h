#pragma once

#include "LuaMath.h"

#include <string>
#include <vector>

namespace p06 {

struct MaterialDesc {
    std::string name;
    Vec3 baseColor;
    double metallic = 0.0;
    double roughness = 1.0;
};

struct EntityDesc {
    std::string name;
    std::string mesh;
    std::string material;
    Vec3 position;
};

struct SceneDesc {
    std::vector<MaterialDesc> materials;
    std::vector<EntityDesc> entities;
};

SceneDesc loadSceneScript(LuaVm& vm, const std::string& path);

} // namespace p06
