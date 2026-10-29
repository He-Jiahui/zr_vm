#include <stdio.h>
#include <stdlib.h>

#include "benchmark_case.h"

static ZrBenchInt zr_bench_case_swarm_step_run(int scale) {
    const ZrBenchInt enemy_count = 64 * (ZrBenchInt)scale;
    const ZrBenchInt bullet_count = 128 * (ZrBenchInt)scale;
    ZrBenchInt *columns = malloc((size_t)(5 * enemy_count + 4 * bullet_count) * sizeof(*columns));
    ZrBenchInt *ex, *ey, *vx, *vy, *hp, *bx, *by, *life, *target;
    ZrBenchInt checksum = 17, hits = 0, kills = 0;

    if (columns == NULL) {
        fprintf(stderr, "swarm_step allocation failed\n");
        exit(EXIT_FAILURE);
    }
    ex = columns;
    ey = ex + enemy_count;
    vx = ey + enemy_count;
    vy = vx + enemy_count;
    hp = vy + enemy_count;
    bx = hp + enemy_count;
    by = bx + bullet_count;
    life = by + bullet_count;
    target = life + bullet_count;
    for (ZrBenchInt i = 0; i < enemy_count; i++) {
        ex[i] = (i * 73 + 19) % 4096;
        ey[i] = (i * 151 + 47) % 4096;
        vx[i] = 1 + i % 3;
        vy[i] = 1 + (i * 3) % 5;
        hp[i] = 3 + i % 5;
    }
    for (ZrBenchInt i = 0; i < bullet_count; i++) {
        bx[i] = (i * 97 + 11) % 4096;
        by[i] = (i * 53 + 23) % 4096;
        life[i] = 8 + i % 17;
        target[i] = (i * 7 + 3) % enemy_count;
    }
    for (ZrBenchInt frame = 0; frame < 24 * (ZrBenchInt)scale; frame++) {
        const ZrBenchInt px = (frame * 29 + 1024) % 4096;
        const ZrBenchInt py = (frame * 43 + 2048) % 4096;
        for (ZrBenchInt i = 0; i < enemy_count; i++) {
            vx[i] += ex[i] < px ? 1 : -1;
            vy[i] += ey[i] < py ? 1 : -1;
            if (vx[i] < -4) { vx[i] = -4; }
            if (vx[i] > 4) { vx[i] = 4; }
            if (vy[i] < -4) { vy[i] = -4; }
            if (vy[i] > 4) { vy[i] = 4; }
            ex[i] = (ex[i] + vx[i] + 4096) % 4096;
            ey[i] = (ey[i] + vy[i] + 4096) % 4096;
        }
        for (ZrBenchInt i = 0; i < bullet_count; i++) {
            const ZrBenchInt t = target[i];
            ZrBenchInt dx, dy;
            if (bx[i] < ex[t]) { bx[i] += 12; }
            else if (bx[i] > ex[t]) { bx[i] -= 12; }
            if (by[i] < ey[t]) { by[i] += 12; }
            else if (by[i] > ey[t]) { by[i] -= 12; }
            bx[i] = (bx[i] + 4096) % 4096;
            by[i] = (by[i] + 4096) % 4096;
            life[i] -= 1;
            dx = ex[t] - bx[i];
            dy = ey[t] - by[i];
            if (dx * dx + dy * dy <= 256) {
                hits += 1;
                hp[t] -= 1;
                life[i] = 0;
                if (hp[t] == 0) {
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
                target[i] = (t + i + frame + 1) % enemy_count;
            }
        }
        for (ZrBenchInt i = 0; i < enemy_count; i++) {
            checksum = (checksum * 131 + ex[i] * 3 + ey[i] * 5
                + (vx[i] + 4) * 7 + (vy[i] + 4) * 11 + hp[i] * 13) % ZR_BENCH_MOD;
        }
        for (ZrBenchInt i = 0; i < bullet_count; i++) {
            checksum = (checksum * 131 + bx[i] * 3 + by[i] * 5
                + life[i] * 7 + target[i] * 11) % ZR_BENCH_MOD;
        }
        checksum = (checksum * 131 + hits * 17 + kills * 19 + frame) % ZR_BENCH_MOD;
    }
    free(columns);
    return checksum;
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_swarm_step = {
        "swarm_step",
        "BENCH_SWARM_STEP_PASS",
        zr_bench_case_swarm_step_run
};
