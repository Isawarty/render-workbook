return function(path)
    return (path:gsub("\\", "/"):gsub("/+", "/"):lower())
end
