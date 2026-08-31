return function(first, last, step)
    local current = first - step
    return function()
        current = current + step
        if (step > 0 and current <= last) or (step < 0 and current >= last) then
            return current
        end
    end
end
