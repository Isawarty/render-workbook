return function(tasks)
    local trace = {}
    while #tasks > 0 do
        local next_round = {}
        for _, task in ipairs(tasks) do
            local ok, value = coroutine.resume(task)
            assert(ok, value)
            if value ~= nil then trace[#trace + 1] = value end
            if coroutine.status(task) ~= "dead" then next_round[#next_round + 1] = task end
        end
        tasks = next_round
    end
    return trace
end
