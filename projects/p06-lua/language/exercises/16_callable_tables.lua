return function(name, fields)
    return setmetatable(fields, {
        __call = function(self, overrides)
            local copy = { name = name }
            for key, value in pairs(self) do copy[key] = value end
            for key, value in pairs(overrides or {}) do copy[key] = value end
            return copy
        end,
    })
end
