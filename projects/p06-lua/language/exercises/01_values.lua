return function(value)
    if value == nil then return "nil" end
    return type(value)
end
