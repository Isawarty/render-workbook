return function(values)
    local minimum, maximum = math.huge, -math.huge
    for _, value in ipairs(values) do
        minimum, maximum = math.min(minimum, value), math.max(maximum, value)
    end
    return minimum, maximum
end
