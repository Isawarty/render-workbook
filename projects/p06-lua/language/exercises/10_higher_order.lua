return function(values, transform)
    local result = {}
    for index, value in ipairs(values) do result[index] = transform(value) end
    return result
end
