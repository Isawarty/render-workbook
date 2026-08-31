local module = {}
function module.to_linear(srgb)
    if srgb <= 0.04045 then return srgb / 12.92 end
    return ((srgb + 0.055) / 1.055) ^ 2.4
end
return module
