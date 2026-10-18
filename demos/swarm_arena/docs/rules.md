# Canonical Swarm Arena rules

Source: design report §§4, 6–8, dated 2026-10-18. This is the intended simulation
contract; the scaffold does not implement it yet. Each lane must follow the same
order, dense append and swap-remove semantics. Changes require a written reason
before regenerating goldens.

The arena is 4096 × 4096 units, with a player-following 1280 × 720 view. Simulation
uses fixed dt = 1/60 s. Headless input is a deterministic Lissajous bot plus dodge.
Weapons auto-fire at the nearest enemy and in radial bursts. Grunt, Runner and
Tank enemies differ in speed, HP and radius, seek the player, separate from nearby
enemies and deal touch damage. Bullets have velocity, lifetime and pierce count.
Enemy deaths emit events, particles and occasional XP pickups. Pickups magnet to
the player; XP unlocks deterministic RNG-selected upgrades. A wave state machine
changes spawn rates and enemy mix, including boss burst waves.

Every frame executes:

1. Input: bot, replay or keyboard → PlayerInput.
2. Director: advance waves and emit RNG-driven spawn requests.
3. Spawn: dense append requested enemies.
4. Grid: counting-sort enemies into 64-unit cells.
5. Steer: seek the player and separate within radius 24 using neighbouring cells.
6. Integrate: clamp velocity after acceleration × dt; position += velocity × dt.
7. Fire: both weapons append bullets.
8. Bullets: integrate, decrease lifetime and swap-remove expired bullets.
9. Collide: grid queries produce bullet Hit events and touch PlayerHit events.
10. Events: drain damage, deaths, swap-removals, particles, pickups and score.
11. Particles: integrate and expire; cosmetic state is excluded from the hash.
12. Pickups: magnet and collect, then XP and upgrades.
13. Hash: counters each frame and full positional state every 60 frames.
14. Render: view culling and lane/submission-specific sprite submission.

Simulation arithmetic uses f64 (`float`) with `+ - * /`, comparisons and `sqrt`.
Trigonometry only builds boot-time lookup tables with the same libm. C kernels use
`-ffp-contract=off`, without `-ffast-math`. RNG is Park–Miller and hashes use a
mod-prime recurrence that avoids checked integer overflow.

| Scenario | Enemy cap | Bullets/s | Particles | Frames |
|---|---:|---:|---|---:|
| smoke | 200 | 120 | on | 120 |
| arena_2k | 2,000 | 600 | on | 1,800 |
| swarm_10k | 10,000 | 600 | on | 1,800 |
| bullet_storm | 1,000 | 6,000 | on | 1,800 |
| particle_burst | 2,000 | 600 | 64/kill | 1,800 |
| sweep | 500, 1k, 2k, 4k, 8k, 16k, 32k | 600 | on | 600 each |

All lanes share seed, scenario, frame count and simulation order. CLI/host and
interp/binary hashes must match. OOP equality or any divergence is documented.
Null submission checksums sprites; software snapshots use bit-exact SHA-256.
