from pathlib import Path
import sys

COMMON_DIR = Path(__file__).resolve().parents[3] / "common" / "python"
if str(COMMON_DIR) not in sys.path:
    sys.path.insert(0, str(COMMON_DIR))

from benchmark_runner import run_main


def swarm_step(scale: int) -> int:
    enemy_count = 64 * scale
    bullet_count = 128 * scale
    ex = [(i * 73 + 19) % 4096 for i in range(enemy_count)]
    ey = [(i * 151 + 47) % 4096 for i in range(enemy_count)]
    vx = [1 + i % 3 for i in range(enemy_count)]
    vy = [1 + (i * 3) % 5 for i in range(enemy_count)]
    hp = [3 + i % 5 for i in range(enemy_count)]
    bx = [(i * 97 + 11) % 4096 for i in range(bullet_count)]
    by = [(i * 53 + 23) % 4096 for i in range(bullet_count)]
    life = [8 + i % 17 for i in range(bullet_count)]
    target = [(i * 7 + 3) % enemy_count for i in range(bullet_count)]
    checksum = 17
    hits = 0
    kills = 0

    for frame in range(24 * scale):
        px = (frame * 29 + 1024) % 4096
        py = (frame * 43 + 2048) % 4096
        for i in range(enemy_count):
            vx[i] += 1 if ex[i] < px else -1
            vy[i] += 1 if ey[i] < py else -1
            vx[i] = max(-4, min(4, vx[i]))
            vy[i] = max(-4, min(4, vy[i]))
            ex[i] = (ex[i] + vx[i] + 4096) % 4096
            ey[i] = (ey[i] + vy[i] + 4096) % 4096

        for i in range(bullet_count):
            t = target[i]
            if bx[i] < ex[t]:
                bx[i] += 12
            elif bx[i] > ex[t]:
                bx[i] -= 12
            if by[i] < ey[t]:
                by[i] += 12
            elif by[i] > ey[t]:
                by[i] -= 12
            bx[i] = (bx[i] + 4096) % 4096
            by[i] = (by[i] + 4096) % 4096
            life[i] -= 1
            dx = ex[t] - bx[i]
            dy = ey[t] - by[i]
            if dx * dx + dy * dy <= 256:
                hits += 1
                hp[t] -= 1
                life[i] = 0
                if hp[t] == 0:
                    kills += 1
                    ex[t] = (t * 73 + frame * 37 + 19) % 4096
                    ey[t] = (t * 151 + frame * 61 + 47) % 4096
                    vx[t] = 1 + t % 3
                    vy[t] = 1 + (t * 3) % 5
                    hp[t] = 3 + t % 5
            if life[i] <= 0:
                bx[i] = px
                by[i] = py
                life[i] = 8 + i % 17
                target[i] = (t + i + frame + 1) % enemy_count

        for i in range(enemy_count):
            checksum = (checksum * 131 + ex[i] * 3 + ey[i] * 5
                        + (vx[i] + 4) * 7 + (vy[i] + 4) * 11 + hp[i] * 13) % 1000000007
        for i in range(bullet_count):
            checksum = (checksum * 131 + bx[i] * 3 + by[i] * 5
                        + life[i] * 7 + target[i] * 11) % 1000000007
        checksum = (checksum * 131 + hits * 17 + kills * 19 + frame) % 1000000007

    return checksum


if __name__ == "__main__":
    run_main("swarm_step", ("BENCH_SWARM_STEP_PASS", swarm_step))
