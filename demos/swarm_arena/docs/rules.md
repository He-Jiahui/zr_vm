# Canonical Swarm Arena rules

Rules revision **gd-c3-smoke/1** implements the GD-C3 subset of design §§4,
6.1/6.3/6.8. Only `smoke`, DOD, CLI drive, the null backend and bulk submission
are implemented. GD-C4 and later milestones have not started. A rule or hash
ordering change needs a written reason before regenerating the golden.

## Implemented smoke

The arena is 4096 × 4096 units. Enemy centers clamp to [0, 4095]. The camera
follows the player with a 1280 × 720 view. Fixed dt is `1.0 / 60.0`; frame indices
are 0–119. Defaults are 120 frames and seed 12345. Overrides allow 1–120 frames
and seeds 1–2147483646; unsupported scenarios, lanes, backends and policies fail.

At boot, the bot builds its lookup tables for t = 1..frames:
`x = 2048 + 128*sin(t*6.283185307179586/240)` and
`y = 2048 + 96*sin(t*6.283185307179586/180)`. Input frame f reads table f (t=f+1).
The initial player is (2048,2048), HP 100. Trigonometry is confined to boot.

The one-wave director requests 200 Grunts on frame 0. On frames 6,12,… it
requests `min(4, 200-alive)` replacements. Each request consumes three RNG
values, in order: radius `192 + next()%257`, offset
`next()%(2*radius+1)-radius`, side `next()%4`. Coordinates relative to the current
player are (offset,+radius), (offset,-radius), (+radius,offset), or
(-radius,offset), for sides 0–3. Requests occupy preallocated x/y columns.

Grunts have HP 2, radius 10 and maximum speed 72. Seek computes a desired
velocity of magnitude 72 toward the player (zero below distance 0.000001).
Acceleration is `(desired-current)*6`; integration adds acceleration × dt,
clamps speed to 72, then advances and clamps position. Spawn velocity and
acceleration start at zero. There is no separation in this subset.

Each frame with any live enemy fires two identical bullets toward the nearest
enemy; equal squared distances keep the first dense slot. Speed is 480, radius
2, life 90 ticks, damage 1, with one hit and no pierce. A zero-length aim uses
(480,0). Integration occurs on the spawn frame; life decreases before expiry
removal and collision. The bullet pool has 512 slots.

Bullet collision examines the 3×3 cells around its current cell, increasing
row, column, then stable dense order within each cell. The first enemy within
squared distance 144 receives a queued hit, and the bullet is swap-removed.
The grid is built before enemy integration, as required by the canonical
order; maximum enemy displacement is 1.2, well inside this conservative cell
neighborhood. Touch collision tests squared distance ≤400. At most one
PlayerHit is queued when `frame >= immuneUntil`; it sets immunity through
`frame+30` and deals one damage when drained.

Each enemy has a monotonic ID starting at 1. Hit events carry IDs, so a prior
death and dense swap cannot redirect later queued damage to another enemy.
Unknown/dead IDs are ignored. Applied hits increment the hit counter; lethal
hits append EnemyKilled and remove the enemy immediately. Kill events add one
kill, ten score, and four cosmetic particles at the death position. Particles
move at (±24,0)/(0,±24), live 24 ticks (including the spawn frame's update), and
use a 256-slot pool; overflow may drop cosmetics. Particles consume no RNG.

Enemy and moving pools use `zr.container.Array<float/int>` columns, allocated
and filled at boot, with a separate active count. Every removal copies **all**
columns from the last live slot. The 64×64 grid uses 64-unit cells, zeroed
counts, exclusive prefix starts (4097 entries), cursors and stable scatter of
dense slots. Cell queries clamp to 0–63 in each axis. The event ring holds
1024 tagged-union entries, drains FIFO (including newly queued kill events),
and treats simulation queue or bullet overflow as an error. Union construction
and switching remain inside the events module; consumers use scalar fields.

## Canonical frame order

1. Input: read the deterministic bot table.
2. Director: emit RNG-driven requests for the one smoke wave.
3. Spawn: append requests in order; assign monotonic IDs.
4. Grid: counting-sort current enemy positions into cells.
5. Steer: compute seek acceleration.
6. Integrate: clamp velocity and advance enemies.
7. Fire: append the two nearest-target bullets.
8. Bullets: integrate, decrease life, swap-remove expired entries.
9. Collide: queue bullet Hit and player touch events; remove spent bullets.
10. Events: apply damage, remove deaths, emit particles and add score.
11. Particles: integrate and expire; cosmetic state is excluded from hashing.
12. Pickups: **no-op in GD-C3**.
13. Hash: mix counters; every 60 frames also mix full positional state.
14. Render: cull sprites, submit mode C to `zrgame.gfx`, consume with null backend.

Submission order is player, dense enemies, dense bullets, dense particles.
Sizes are 20,20,4,3. Colors are respectively 1717987071, 4284901119,
4294967295, 4290773247. Bounding squares intersect the camera view inclusively.
The preallocated render buffer has 969 slots. One `submitSoA` slice submits all
visible sprites per frame. No image, host executable or software renderer exists.

## Determinism and exact hash order

`packages/swarmcore` owns the only RNG and hash implementations. The supported
v1 dependency is `$swarmcore`, imported as `import("&swarmcore")`; v2
`@swarmcore` and runtime reflection remain deferred. See
[composition workarounds](gd-c3-decisions.md).

Park–Miller uses `s=(s*48271)%2147483647`. Hash mixing uses
`h=(h*31+q)%1000000007`, with negative q normalized modulo the prime.
Positions, velocity and acceleration quantize with `<int>(value*16.0)`
(truncation toward zero). Products fit checked signed i64. Runtime simulation
uses f64 arithmetic, comparisons and sqrt; clocks, GC, renderer and cosmetics
cannot change the simulation hash.

The rolling hash starts at 1. After each frame's events/particles, mix these
integers in order: completed frame number (f+1), RNG state, next enemy ID, enemy
count, bullet count, player HP, immunity end frame, shots, applied hits, kills,
score. Every 60 completed frames, append a positional pass:

- Player x,y.
- Each dense enemy: ID, kind (always 0), HP, x,y,vx,vy,ax,ay.
- Each dense bullet: x,y,vx,vy,remaining life.

A final positional pass is appended when reporting `fullHash`, including on
frame counts divisible by 60. Thus the default run includes positional passes
at 60,120 and once more for its final snapshot. This also hashes the complete
position state for partial smoke runs. Derived grid contents and empty queue
storage are excluded, as are particles, render hashes and timing/GC telemetry.

The default golden is **224663030**, checkpoint 60 is **227689432**, and the
last null-render content hash is **183257039**. [hashes.json](../goldens/hashes.json)
also records exact final counters. CTests require three repetitions per mode
and enforce both the simulation hash and those counters/render hash. This is
repeatability and interpreter/binary parity on this toolchain, not a claim of
cross-platform libm identity or CLI/host/lane equivalence.

## Deferred rules

GD-C5 owns separation, Runner/Tank kinds, dodge input, radial fire, pierce,
pickups/magnet, XP/upgrades, multi-wave/boss behavior and scenarios `arena_2k`,
`swarm_10k`, `bullet_storm`, `particle_burst`, `sweep`. GD-C4 owns host drive,
slack/manual GC policies and file metrics. Other lanes, native kernels,
software/GL rendering, screenshots and performance budgets remain later work.
