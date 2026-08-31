return function(prototype, fields)
    fields = fields or {}
    return setmetatable(fields, { __index = prototype })
end
