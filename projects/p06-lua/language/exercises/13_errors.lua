return function(callback)
    local ok, result = xpcall(callback, debug.traceback)
    if ok then return true, result end
    return false, result
end
