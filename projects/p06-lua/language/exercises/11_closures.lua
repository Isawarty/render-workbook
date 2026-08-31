return function(initial)
    local value = initial or 0
    return function(delta)
        value = value + (delta or 1)
        return value
    end
end
