return function(limit)
    local sum = 0
    for value = 1, limit do
        if value % 3 == 0 or value % 5 == 0 then sum = sum + value end
    end
    return sum
end
