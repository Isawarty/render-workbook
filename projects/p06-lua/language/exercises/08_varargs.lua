return function(...)
    local count, sum = select("#", ...), 0
    for index = 1, count do sum = sum + select(index, ...) end
    return count, sum
end
