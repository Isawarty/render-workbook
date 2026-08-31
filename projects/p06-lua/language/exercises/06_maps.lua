return function(values)
    local counts = {}
    for _, value in ipairs(values) do counts[value] = (counts[value] or 0) + 1 end
    return counts
end
