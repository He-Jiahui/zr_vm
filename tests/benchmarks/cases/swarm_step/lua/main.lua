local source = debug.getinfo(1, "S").source
local current_dir = source:match("^@(.+[\\/])") or "./"
package.path = current_dir .. "../../../common/lua/?.lua;" .. package.path
local runner = require("benchmark_runner")

local function swarm_step(scale)
    local enemy_count, bullet_count = 64 * scale, 128 * scale
    -- Keep entity IDs zero-based in all languages, including Lua tables.
    local ex, ey, vx, vy, hp = {}, {}, {}, {}, {}
    local bx, by, life, target = {}, {}, {}, {}
    for i = 0, enemy_count - 1 do
        ex[i] = (i * 73 + 19) % 4096
        ey[i] = (i * 151 + 47) % 4096
        vx[i] = 1 + i % 3
        vy[i] = 1 + (i * 3) % 5
        hp[i] = 3 + i % 5
    end
    for i = 0, bullet_count - 1 do
        bx[i] = (i * 97 + 11) % 4096
        by[i] = (i * 53 + 23) % 4096
        life[i] = 8 + i % 17
        target[i] = (i * 7 + 3) % enemy_count
    end
    local checksum, hits, kills = 17, 0, 0
    for frame = 0, 24 * scale - 1 do
        local px = (frame * 29 + 1024) % 4096
        local py = (frame * 43 + 2048) % 4096
        for i = 0, enemy_count - 1 do
            if ex[i] < px then vx[i] = vx[i] + 1 else vx[i] = vx[i] - 1 end
            if ey[i] < py then vy[i] = vy[i] + 1 else vy[i] = vy[i] - 1 end
            vx[i] = math.max(-4, math.min(4, vx[i]))
            vy[i] = math.max(-4, math.min(4, vy[i]))
            ex[i] = (ex[i] + vx[i] + 4096) % 4096
            ey[i] = (ey[i] + vy[i] + 4096) % 4096
        end
        for i = 0, bullet_count - 1 do
            local t = target[i]
            if bx[i] < ex[t] then bx[i] = bx[i] + 12
            elseif bx[i] > ex[t] then bx[i] = bx[i] - 12 end
            if by[i] < ey[t] then by[i] = by[i] + 12
            elseif by[i] > ey[t] then by[i] = by[i] - 12 end
            bx[i] = (bx[i] + 4096) % 4096
            by[i] = (by[i] + 4096) % 4096
            life[i] = life[i] - 1
            local dx, dy = ex[t] - bx[i], ey[t] - by[i]
            if dx * dx + dy * dy <= 256 then
                hits = hits + 1
                hp[t] = hp[t] - 1
                life[i] = 0
                if hp[t] == 0 then
                    kills = kills + 1
                    ex[t] = (t * 73 + frame * 37 + 19) % 4096
                    ey[t] = (t * 151 + frame * 61 + 47) % 4096
                    vx[t] = 1 + t % 3
                    vy[t] = 1 + (t * 3) % 5
                    hp[t] = 3 + t % 5
                end
            end
            if life[i] <= 0 then
                bx[i], by[i] = px, py
                life[i] = 8 + i % 17
                target[i] = (t + i + frame + 1) % enemy_count
            end
        end
        for i = 0, enemy_count - 1 do
            checksum = (checksum * 131 + ex[i] * 3 + ey[i] * 5
                + (vx[i] + 4) * 7 + (vy[i] + 4) * 11 + hp[i] * 13) % 1000000007
        end
        for i = 0, bullet_count - 1 do
            checksum = (checksum * 131 + bx[i] * 3 + by[i] * 5
                + life[i] * 7 + target[i] * 11) % 1000000007
        end
        checksum = (checksum * 131 + hits * 17 + kills * 19 + frame) % 1000000007
    end
    return checksum
end

runner.run_main("swarm_step", arg, { "BENCH_SWARM_STEP_PASS", swarm_step })
