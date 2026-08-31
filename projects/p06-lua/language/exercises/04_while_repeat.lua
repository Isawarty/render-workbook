return function(value, target)
    local steps = 0
    repeat
        value = math.floor(value / 2)
        steps = steps + 1
    until value <= target
    return steps
end
