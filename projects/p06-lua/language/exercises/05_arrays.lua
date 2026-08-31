return function(values)
    local result, seen = {}, {}
    for _, value in ipairs(values) do
        if not seen[value] then
            seen[value] = true
            result[#result + 1] = value
        end
    end
    return result
end
