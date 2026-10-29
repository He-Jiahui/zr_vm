"use strict";

const { runMain } = require("../../../common/node/benchmark_runner");

function swarmStep(scale) {
    const enemyCount = 64 * scale;
    const bulletCount = 128 * scale;
    const ex = [], ey = [], vx = [], vy = [], hp = [];
    const bx = [], by = [], life = [], target = [];
    for (let i = 0; i < enemyCount; i += 1) {
        ex.push((i * 73 + 19) % 4096);
        ey.push((i * 151 + 47) % 4096);
        vx.push(1 + i % 3);
        vy.push(1 + (i * 3) % 5);
        hp.push(3 + i % 5);
    }
    for (let i = 0; i < bulletCount; i += 1) {
        bx.push((i * 97 + 11) % 4096);
        by.push((i * 53 + 23) % 4096);
        life.push(8 + i % 17);
        target.push((i * 7 + 3) % enemyCount);
    }
    let checksum = 17;
    let hits = 0;
    let kills = 0;

    for (let frame = 0; frame < 24 * scale; frame += 1) {
        const px = (frame * 29 + 1024) % 4096;
        const py = (frame * 43 + 2048) % 4096;
        for (let i = 0; i < enemyCount; i += 1) {
            vx[i] += ex[i] < px ? 1 : -1;
            vy[i] += ey[i] < py ? 1 : -1;
            vx[i] = Math.max(-4, Math.min(4, vx[i]));
            vy[i] = Math.max(-4, Math.min(4, vy[i]));
            ex[i] = (ex[i] + vx[i] + 4096) % 4096;
            ey[i] = (ey[i] + vy[i] + 4096) % 4096;
        }
        for (let i = 0; i < bulletCount; i += 1) {
            const t = target[i];
            if (bx[i] < ex[t]) { bx[i] += 12; }
            else if (bx[i] > ex[t]) { bx[i] -= 12; }
            if (by[i] < ey[t]) { by[i] += 12; }
            else if (by[i] > ey[t]) { by[i] -= 12; }
            bx[i] = (bx[i] + 4096) % 4096;
            by[i] = (by[i] + 4096) % 4096;
            life[i] -= 1;
            const dx = ex[t] - bx[i];
            const dy = ey[t] - by[i];
            if (dx * dx + dy * dy <= 256) {
                hits += 1;
                hp[t] -= 1;
                life[i] = 0;
                if (hp[t] === 0) {
                    kills += 1;
                    ex[t] = (t * 73 + frame * 37 + 19) % 4096;
                    ey[t] = (t * 151 + frame * 61 + 47) % 4096;
                    vx[t] = 1 + t % 3;
                    vy[t] = 1 + (t * 3) % 5;
                    hp[t] = 3 + t % 5;
                }
            }
            if (life[i] <= 0) {
                bx[i] = px;
                by[i] = py;
                life[i] = 8 + i % 17;
                target[i] = (t + i + frame + 1) % enemyCount;
            }
        }
        for (let i = 0; i < enemyCount; i += 1) {
            checksum = (checksum * 131 + ex[i] * 3 + ey[i] * 5
                + (vx[i] + 4) * 7 + (vy[i] + 4) * 11 + hp[i] * 13) % 1000000007;
        }
        for (let i = 0; i < bulletCount; i += 1) {
            checksum = (checksum * 131 + bx[i] * 3 + by[i] * 5
                + life[i] * 7 + target[i] * 11) % 1000000007;
        }
        checksum = (checksum * 131 + hits * 17 + kills * 19 + frame) % 1000000007;
    }
    return checksum;
}

runMain("swarm_step", ["BENCH_SWARM_STEP_PASS", swarmStep]);
