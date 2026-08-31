return function(defaults, overrides)
    return setmetatable(overrides or {}, { __index = defaults })
end
