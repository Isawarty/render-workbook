local script_dir = assert(arg[0]:match("^(.*)[/\\]"), "runner path must include a directory")
local exercise_dir = script_dir .. "/exercises"

local function same_array(actual, expected)
    if #actual ~= #expected then return false end
    for index, value in ipairs(expected) do
        if actual[index] ~= value then return false end
    end
    return true
end

local checks = {
    [1] = function(f) assert(f(nil) == "nil" and f(3) == "number" and f({}) == "table") end,
    [2] = function(f) assert(f("Assets\\Helmet//ALBEDO.PNG") == "assets/helmet/albedo.png") end,
    [3] = function(f) assert(f(10) == 33 and f(1) == 0) end,
    [4] = function(f) assert(f(100, 10) == 4 and f(8, 4) == 1) end,
    [5] = function(f) assert(same_array(f({3, 1, 3, 2, 1}), {3, 1, 2})) end,
    [6] = function(f) local c = f({"a", "b", "a"}); assert(c.a == 2 and c.b == 1) end,
    [7] = function(f) assert(same_array(f({z=1, a=2, m=3}), {"a", "m", "z"})) end,
    [8] = function(f) local count, sum = f(2, 4, 8); assert(count == 3 and sum == 14) end,
    [9] = function(f) local lo, hi = f({7, -2, 9, 1}); assert(lo == -2 and hi == 9) end,
    [10] = function(f) assert(same_array(f({1,2,3}, function(v) return v*v end), {1,4,9})) end,
    [11] = function(f) local c = f(10); assert(c() == 11 and c(4) == 15) end,
    [12] = function(f) local values = {}; for v in f(2, 8, 2) do values[#values+1]=v end; assert(same_array(values,{2,4,6,8})) end,
    [13] = function(f) local ok, value = f(function() return 42 end); assert(ok and value == 42); local good, err = f(function() error("boom") end); assert(not good and err:find("boom")) end,
    [14] = function(f) local v = f({roughness=0.5}, {metallic=1}); assert(v.roughness == 0.5 and v.metallic == 1) end,
    [15] = function(m) local v = m.new(1,2) + m.new(3,4); assert(v.x == 4 and v.y == 6 and tostring(v) == "vec2(4, 6)") end,
    [16] = function(f) local material = f("metal", {roughness=0.2}); local instance = material({roughness=0.7}); assert(instance.name == "metal" and instance.roughness == 0.7) end,
    [17] = function(f) local v = f({kind="light"}, {intensity=4}); assert(v.kind == "light" and v.intensity == 4) end,
    [18] = function(m) assert(math.abs(m.to_linear(1.0)-1.0)<1e-9 and math.abs(m.to_linear(0.0))<1e-9) end,
    [19] = function(f) local next_value=f({5,6}); assert(next_value()==5 and next_value()==6 and next_value()==nil) end,
    [20] = function(f)
        local function task(prefix) return coroutine.create(function() coroutine.yield(prefix.."1"); coroutine.yield(prefix.."2") end) end
        assert(same_array(f({task("a"), task("b")}), {"a1","b1","a2","b2"}))
    end,
}

local files = {
    "01_values.lua", "02_strings.lua", "03_numeric_for.lua", "04_while_repeat.lua",
    "05_arrays.lua", "06_maps.lua", "07_generic_for.lua", "08_varargs.lua",
    "09_multiple_returns.lua", "10_higher_order.lua", "11_closures.lua",
    "12_iterators.lua", "13_errors.lua", "14_index_metatable.lua",
    "15_operator_metatable.lua", "16_callable_tables.lua", "17_prototypes.lua",
    "18_modules.lua", "19_coroutine_generator.lua", "20_coroutine_scheduler.lua",
}

local selected = arg[1] and tonumber(arg[1]) or nil
if selected and (selected < 1 or selected > #checks) then
    io.stderr:write("exercise must be between 01 and 20\n")
    os.exit(2)
end

local passed, failed = 0, 0
for index, check in ipairs(checks) do
    if not selected or selected == index then
        local filename = files[index]
        local ok_load, exercise = pcall(dofile, exercise_dir .. "/" .. filename)
        local ok, error_message = false, exercise
        if ok_load then ok, error_message = xpcall(function() check(exercise) end, debug.traceback) end
        if ok then
            passed = passed + 1
            print(("[PASS] %02d %s"):format(index, filename))
        else
            failed = failed + 1
            io.stderr:write(("[FAIL] %02d %s\n%s\n"):format(index, filename or "missing", tostring(error_message)))
        end
    end
end
print(("P06-t00: %d passed, %d failed"):format(passed, failed))
if failed > 0 then os.exit(1) end
