return function(values)
    return coroutine.wrap(function()
        for _, value in ipairs(values) do coroutine.yield(value) end
    end)
end
