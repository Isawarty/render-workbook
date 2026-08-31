local methods = {}
local meta = {
    __index = methods,
    __add = function(a, b) return methods.new(a.x + b.x, a.y + b.y) end,
    __tostring = function(v) return ("vec2(%g, %g)"):format(v.x, v.y) end,
}
function methods.new(x, y) return setmetatable({ x = x, y = y }, meta) end
return methods
