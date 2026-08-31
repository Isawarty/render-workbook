#pragma once

#include "LuaVm.h"
#include "rwb/rendergraph/RenderGraph.h"

#include <string>

namespace p06 {

rwb::rg::CompiledGraph loadRenderGraphScript(LuaVm& vm, const std::string& path);

} // namespace p06
