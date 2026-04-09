local M = {}

local MOD = 1000000007
local TIER_SCALES = {
    smoke = 1,
    core = 4,
    stress = 16,
    profile = 1,
}

local function parse_scale(args)
    local tier = "core"
    local explicit_scale = nil
    local index = 1

    while index <= #args do
        local arg = args[index]
        if arg == "--tier" then
            if index + 1 > #args then
                error("--tier requires smoke, core, stress, or profile")
            end
            tier = args[index + 1]
            index = index + 2
        elseif arg == "--scale" then
            local parsed_scale
            if index + 1 > #args then
                error("--scale requires a positive integer")
            end
            parsed_scale = tonumber(args[index + 1])
            if parsed_scale == nil or parsed_scale < 1 or math.floor(parsed_scale) ~= parsed_scale then
                error("--scale requires a positive integer")
            end
            explicit_scale = parsed_scale
            index = index + 2
        else
            error("unknown argument: " .. tostring(arg))
        end
    end

    if explicit_scale ~= nil then
        return explicit_scale
    end
    if TIER_SCALES[tier] == nil then
        error("unsupported tier: " .. tostring(tier))
    end
    return TIER_SCALES[tier]
end

local function numeric_loops(scale)
    local outer_limit = 24 * scale
    local inner_limit = 3000 * scale
    local value = 17
    local checksum = 0

    for outer = 0, outer_limit - 1 do
        for inner = 0, inner_limit - 1 do
            value = (value * 1103 + 97 + outer + inner) % 65521
            if value % 7 == 0 then
                checksum = checksum + math.floor(value / 7)
            elseif value % 5 == 0 then
                checksum = checksum + (value % 97)
            else
                checksum = checksum + (value % 31)
            end
            checksum = checksum % MOD
        end
        checksum = (checksum + outer * 17 + (value % 13)) % MOD
    end

    return checksum
end

local function int_xor(left, right)
    if type(bit32) == "table" and bit32.bxor ~= nil then
        return bit32.bxor(left, right)
    end

    local result = 0
    local bit = 1
    local a = left
    local b = right

    while a > 0 or b > 0 do
        local a_bit = a % 2
        local b_bit = b % 2
        if a_bit ~= b_bit then
            result = result + bit
        end
        a = math.floor(a / 2)
        b = math.floor(b / 2)
        bit = bit * 2
    end

    return result
end

local function worker_step(worker, delta)
    if worker.kind == 0 then
        worker.state = (worker.state * 13 + delta + 7) % 10007
    elseif worker.kind == 1 then
        worker.state = (worker.state * 17 + delta * 3 + 11) % 10009
    elseif worker.kind == 2 then
        worker.state = (int_xor(worker.state, delta + 31) + delta * 5 + 19) % 10037
    else
        worker.state = (worker.state + delta * delta + 23) % 10039
    end
    return worker.state
end

local function dispatch_loops(scale)
    local workers = {
        { state = 17, kind = 0 },
        { state = 29, kind = 1 },
        { state = 43, kind = 2 },
        { state = 61, kind = 3 },
    }
    local outer_limit = 120 * scale
    local inner_limit = 320 * scale
    local checksum = 0

    for outer = 0, outer_limit - 1 do
        for inner = 0, inner_limit - 1 do
            local index = ((outer + inner) % 4) + 1
            local delta = outer * 7 + inner * 11 + (index - 1)
            local value = worker_step(workers[index], delta)
            checksum = (checksum + value * index + (delta % 29)) % MOD
        end
        local worker = workers[(outer % 4) + 1]
        checksum = (checksum + worker.state * (outer + 1)) % MOD
    end

    return checksum
end

local function container_label(value)
    if value % 2 == 0 then
        return "even"
    end
    if value > 128 then
        return "odd_hi"
    end
    return "odd_lo"
end

local function container_pipeline(scale)
    local total = 1024 * scale
    local queue = {}
    local seen = {}
    local buckets = {
        even = {},
        odd_hi = {},
        odd_lo = {},
    }
    local seed = 41

    for index = 0, total - 1 do
        seed = (seed * 29 + 17 + index) % 257
        queue[#queue + 1] = { container_label(seed), seed * scale + (index % 13) }
    end

    for _, item in ipairs(queue) do
        local label = item[1]
        local value = item[2]
        local key = label .. ":" .. tostring(value)
        if seen[key] == nil then
            seen[key] = true
            buckets[label][#buckets[label] + 1] = value
        end
    end

    local odd_lo_sum = 0
    for _, value in ipairs(buckets.odd_lo) do
        odd_lo_sum = odd_lo_sum + value
    end
    local odd_hi_sum = 0
    for _, value in ipairs(buckets.odd_hi) do
        odd_hi_sum = odd_hi_sum + value
    end
    local even_sum = 0
    for _, value in ipairs(buckets.even) do
        even_sum = even_sum + value
    end

    local unique_count = 0
    for _ in pairs(seen) do
        unique_count = unique_count + 1
    end

    return (even_sum * 100000 + odd_hi_sum * 100 + odd_lo_sum + unique_count) % MOD
end

local function insertion_sort(values)
    for index = 2, #values do
        local key = values[index]
        local cursor = index - 1
        while cursor >= 1 and values[cursor] > key do
            values[cursor + 1] = values[cursor]
            cursor = cursor - 1
        end
        values[cursor + 1] = key
    end
end

local function mod_mul(left, right, modulus)
    local result = 0
    local a = left % modulus
    local b = right

    while b > 0 do
        if b % 2 == 1 then
            result = (result + a) % modulus
        end
        b = math.floor(b / 2)
        a = (a * 2) % modulus
    end

    return result
end

local function build_sort_pattern(pattern, length)
    local values = {}
    local seed = 97

    if pattern == 0 then
        for index = 0, length - 1 do
            seed = (mod_mul(seed, 1103515245, 2147483647) + 12345 + index) % 2147483647
            values[#values + 1] = seed % 100000
        end
        return values
    end

    if pattern == 1 then
        for index = 0, length - 1 do
            values[#values + 1] = length - index
        end
        return values
    end

    if pattern == 2 then
        for index = 0, length - 1 do
            values[#values + 1] = (index * 17 + 3) % (math.floor(length / 8) + 5)
        end
        return values
    end

    for index = 0, length - 1 do
        values[#values + 1] = index * 3 + (index % 7)
    end
    for index = 1, length, 7 do
        local swap_index = ((index - 1) * 13 + 5) % length + 1
        values[index], values[swap_index] = values[swap_index], values[index]
    end
    return values
end

local function sort_array(scale)
    local length = 16 * scale
    local step = math.floor(length / 7)
    local checksum = 0
    if step < 1 then
        step = 1
    end

    for pattern = 0, 3 do
        local values = build_sort_pattern(pattern, length)
        insertion_sort(values)
        local cursor = 1
        local subtotal = 0
        while cursor <= length do
            subtotal = (subtotal + values[cursor] * cursor) % MOD
            cursor = cursor + step
        end
        subtotal = (subtotal
            + values[1] * 3
            + values[math.floor(length / 2) + 1] * 5
            + values[length] * 7
            + pattern * 11) % MOD
        checksum = (checksum * 131 + subtotal) % MOD
    end

    return checksum
end

local function prime_trial_division(scale)
    local limit = 5000 * scale
    local checksum = 0
    local count = 0

    for candidate = 2, limit do
        local is_prime = true
        local divisor = 2
        while divisor * divisor <= candidate do
            if candidate % divisor == 0 then
                is_prime = false
                break
            end
            divisor = divisor + 1
        end

        if is_prime then
            count = count + 1
            checksum = (checksum + candidate * ((count % 97) + 1)) % MOD
        end
    end

    return checksum
end

local function matrix_add_2d(scale)
    local rows = 24 * scale
    local cols = 32 * scale
    local cells = rows * cols
    local lhs = {}
    local rhs = {}
    local dst = {}
    local scratch = {}
    local checksum = 0

    for index = 1, cells do
        lhs[index] = ((index - 1) * 13 + 7) % 997
        rhs[index] = ((index - 1) * 17 + 11) % 991
    end

    for row = 0, rows - 1 do
        local row_sum = 0
        for col = 0, cols - 1 do
            local index = row * cols + col + 1
            dst[index] = lhs[index] + rhs[index] + ((row + col) % 7)
            scratch[index] = dst[index] - math.floor(lhs[index] / 3) + (rhs[index] % 11)
            row_sum = row_sum + scratch[index] * (col + 1)
        end
        checksum = (checksum + row_sum * (row + 1)) % MOD
    end

    for index = 1, cells do
        checksum = (checksum + scratch[index] * (((index - 1) % 17) + 1)) % MOD
    end

    return checksum
end

local function string_build(scale)
    local fragments = { "al", "be", "cy", "do", "ex", "fu" }
    local counts = {}
    local keys = {}
    local assembled = ""
    local assembled_score = 0
    local checksum = 0
    local seed = 17
    local iterations = 180 * scale

    for index = 0, iterations - 1 do
        seed = (seed * 73 + 19 + index) % 997
        local token_id = (seed + index) % 23
        local token = fragments[(seed % #fragments) + 1]
            .. "-"
            .. fragments[(token_id % #fragments) + 1]
            .. fragments[((token_id + 2) % #fragments) + 1]
        local token_score = (seed % 211) + token_id * 17 + index
        assembled = assembled .. token
        assembled_score = (assembled_score * 41 + token_score) % MOD
        if index % 4 == 0 then
            assembled = assembled .. "|"
            assembled_score = (assembled_score + 3) % MOD
        else
            assembled = assembled .. ":"
            assembled_score = (assembled_score + 7) % MOD
        end

        if index % 9 == 8 then
            if counts[assembled] == nil then
                counts[assembled] = 0
                keys[#keys + 1] = assembled
            end
            counts[assembled] = (counts[assembled] + assembled_score + index + 1) % MOD
            checksum = (checksum + counts[assembled] + (seed % 97)) % MOD
            assembled = token
            assembled_score = token_score % MOD
        end
    end

    if #assembled > 0 then
        if counts[assembled] == nil then
            counts[assembled] = 0
            keys[#keys + 1] = assembled
        end
        counts[assembled] = (counts[assembled] + assembled_score + iterations) % MOD
    end

    for index = 1, #keys do
        checksum = (checksum + counts[keys[index]] * index) % MOD
    end

    return checksum
end

local function map_object_access(scale)
    local labels = { "aa", "bb", "cc", "dd" }
    local buckets = {}
    local checksum = 0
    local left = 3
    local right = 7
    local hits = 0
    local outer_limit = 64 * scale

    for outer = 0, outer_limit - 1 do
        for inner = 0, 31 do
            left = (left * 31 + outer + inner + hits) % 10007
            right = (right + left + inner * 3 + 5) % 10009
            hits = hits + 1
            local label = labels[((outer + inner) % #labels) + 1]
            local key = label .. "_slot"
            buckets[key] = ((buckets[key] or 0) + left + right + hits) % MOD
            checksum = (checksum + buckets[key] + left + hits) % MOD
        end
    end

    checksum = checksum
        + (buckets["aa_slot"] or 0)
        + (buckets["bb_slot"] or 0)
        + (buckets["cc_slot"] or 0)
        + (buckets["dd_slot"] or 0)
    return checksum % MOD
end

local CASE_HANDLERS = {
    numeric_loops = { "BENCH_NUMERIC_LOOPS_PASS", numeric_loops },
    dispatch_loops = { "BENCH_DISPATCH_LOOPS_PASS", dispatch_loops },
    container_pipeline = { "BENCH_CONTAINER_PIPELINE_PASS", container_pipeline },
    sort_array = { "BENCH_SORT_ARRAY_PASS", sort_array },
    prime_trial_division = { "BENCH_PRIME_TRIAL_DIVISION_PASS", prime_trial_division },
    matrix_add_2d = { "BENCH_MATRIX_ADD_2D_PASS", matrix_add_2d },
    string_build = { "BENCH_STRING_BUILD_PASS", string_build },
    map_object_access = { "BENCH_MAP_OBJECT_ACCESS_PASS", map_object_access },
}

function M.run_main(case_name, args)
    local handler = CASE_HANDLERS[case_name]
    if handler == nil then
        error("unknown benchmark case: " .. tostring(case_name))
    end

    local scale = parse_scale(args or {})
    print(handler[1])
    print(handler[2](scale))
end

return M
