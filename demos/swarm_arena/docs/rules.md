# Canonical Swarm Arena rules

Rules revision **gd-c5-v1/1** implements full v1 gameplay on DOD, shared by
CLI and host drive, with null rendering and bulk submission. The reason for
replacing the GD-C3 smoke golden is declared in
[GD-C5 decisions](gd-c5-decisions.md). A rule or hash-order change requires a
written reason before golden regeneration.

## Scenarios and storage

The arena is 4096 × 4096 units; enemy centers clamp to [0, 4095]. The camera
follows the player with a 1280 × 720 view. Fixed dt is `1.0 / 60.0`. Seed
defaults to 12345 and accepts 1–2147483646. A frame override accepts 1 through
the scenario's default length, independently of argument order.

| Scenario | Enemy cap | Default frames | Aimed bullets/frame | Base ring radius | Particles/kill |
| --- | --- | --- | --- | --- | --- |
| smoke | 200 | 120 | 2 | 192 | 4 |
| arena_2k | 2000 | 1800 | 10 | 1024 | 4 |
| swarm_10k | 10000 | 1800 | 10 | 1600 | 4 |
| bullet_storm | 1000 | 1800 | 100 | 384 | 4 |
| particle_burst | 2000 | 1800 | 10 | 512 | 64 |

The aimed rates are 120, 600, 600, 6000 and 600 bullets/second. The separate
radial weapon supplements them. Unsupported scenarios (including `sweep`),
lanes, renderers, submission modes and policies fail with a field-naming error.
Host options retain auto/slack/manual GC, output artifacts and expected-hash
checks. CLI artifacts remain textual; the host owns CSV/JSON output.

Enemies, bullets, pickups and particles use numeric `zr.container.Array`
columns and a separate active count. Removal copies every column from the last
live slot. Stable enemy IDs begin at 1; a derived ID-to-slot column makes queued
damage independent of swaps. The 64×64 grid has 64-unit cells, zeroed counts,
4097 prefix starts, cursors and stable scatter of dense slots. Queries clamp
to the arena. Grid and ID lookup storage are derived and excluded from hashing.

Bullet capacity is `(base aimed count + 8)*90 + 64`, bounding lifetime and
all weapon upgrades. Pickup capacity is `2*enemy cap + 256`. Particle capacity
is 512 in smoke, 8192 in bullet storm, 16384 in particle burst, and 4096
otherwise. Cosmetic overflow drops particles. Simulation capacity overflow is
an error. The event ring has `8*bullet capacity + 64` entries, bounding eight hits per
bullet plus player contact. Death events replace popped damage entries.
The ring appends storage as each slot is first written and reuses it on wrap;
unused slots need no placeholder values at boot. Logical capacity and FIFO
order are unchanged, and empty storage is never read.
The render buffer can hold the player and every live slot from all four pools;
these totals fit the existing native capacity of 65536 sprites.

## Player, steering and damage

At boot the deterministic bot retains its Lissajous lookup tables, for
`t=1..frames`: `x=2048+128*sin(t*6.283185307179586/240)` and
`y=2048+96*sin(t*6.283185307179586/180)`. Input frame f reads t=f+1. Using
the previous enemy positions, the nearest enemy within 48 units causes a
perpendicular displacement of `24*(1-distance/48)` from that table position.
A dodge distance at most 0.000001 uses positive x. Equal nearest distances
retain the first dense slot. The player starts with HP 100 and radius 10.
A completed run always advances its configured frame count; HP clamps to zero.

| Kind | Code | Maximum speed | Initial HP | Radius | Pickup value |
| --- | --- | --- | --- | --- | --- |
| Grunt | 0 | 72 | 2 | 10 | 1 |
| Runner | 1 | 120 | 1 | 7 | 2 |
| Tank | 2 | 42 | 8 | 16 | 4 |

A boss is a Tank with HP 40 and pickup value 8. Spawn velocity and
acceleration are zero. Seek computes a desired velocity toward the player
with magnitude equal to the kind's speed (zero at distance ≤0.000001).
Separation visits grid cells intersecting the 24-unit query square in increasing
row, column and stable dense order. For each other enemy at squared distance
`d2` in (0.000001, 576], add
`weight=(576-d2)/576*600`, then add `(self-neighbour)/d2*weight`
to desired velocity, preserving that operation order. At d2 ≤0.000001,
contribute x=-24 for the lower stable ID and x=24 for the higher ID.
Acceleration is `(desired-current)*6`. Integration adds acceleration*dt,
clamps velocity magnitude to the kind's speed, advances and clamps position.
Steering reads the phase-4 snapshot; no positions change during steering.

Touch collision uses radius `enemy radius + 10`. At most one PlayerHit event
is queued when `frame >= immuneUntil`; it sets immunity to frame+30 and deals
one damage when drained. Enemy processing stays in dense order.

## Both weapons and pierce

Weapon rules live in `player.zr`. Every frame with enemies fires the configured
aimed count toward the nearest enemy, retaining the first dense slot on ties.
An aim distance at most 0.000001 uses velocity (480,0); otherwise normalize
direction to speed 480. The radial weapon fires at frames 0,60,… when enemies
exist; its next deadline advances from the firing frame. It starts with eight
shots. The radial weapon uses boot-time 64-direction sine/cosine tables;
shot i uses table index `i*64/radial count` with integer division.

All bullets have radius 2, lifetime 90 ticks and initial damage 1. Aimed
bullets permit two hits; radial bullets permit three. Pierce upgrades add to
these limits, capped at eight hits. Each bullet stores damage at birth,
remaining hits and up to eight previously hit stable IDs. It can hit multiple
distinct enemies in a frame and cannot hit a previous ID again. Integration
and lifetime decrement occur on the spawn frame, before collision.

Collision queries the phase-4 grid over the bullet's ±20-unit square. This
covers the largest combined collision radius (18) plus the maximum enemy
displacement (2). Visit increasing row, column and stable dense slot order;
test current enemy positions against `(enemy radius+2)^2`. Each eligible hit
queues damage by stable ID, records that ID and consumes one permitted hit.
Exhausted bullets are swap-removed; bullets with remaining pierce survive
until another hit or lifetime expiry. All collision events precede deaths.

## Events, pickups and progression

The retained concrete tagged-union ring drains FIFO, including appended death
events. Union construction and decoding stay inside `events.zr`; consumers
use scalar fields. Applied damage increments hits. A lethal hit appends
EnemyKilled with ID, kind, boss flag and death position, then swap-removes the
enemy. Later hits to that dead ID are ignored. Each death adds one kill and
ten score, creates its cosmetic burst, and consumes one RNG draw. An even
draw spawns a pickup; a boss always drops one but still consumes the draw.

Pickups keep dense x/y/value columns. In phase 12, a pickup within the magnet
radius moves toward the player at 240 units/second, clamped to the remaining
distance. Collection tests distance ≤16 after movement, adds the pickup value
and swap-removes it. Initial magnet radius is 160.

The player begins at level 1 with XP 0 and threshold 4. Collection adds XP;
while XP reaches the current threshold, subtract that threshold, increment
level, increase the next threshold by 2, and consume exactly one RNG draw.
`next()%5` selects the following upgrade; capped selections do not reroll.

| Code | Upgrade | Maximum |
| --- | --- | --- |
| 0 | Bullet damage +1 | 8 |
| 1 | Additional permitted hits +1 | 8 total per bullet |
| 2 | Aimed bullets/frame +1 | Base +8 |
| 3 | Magnet radius +24 | 384 |
| 4 | Radial shots +2 | 32 |

Upgrades affect the next fire phase; existing bullets retain their birth
damage and pierce. A magnet upgrade applies to later dense pickups in the
same collection phase. The upgrade selection history is mixed into a separate
integer hash for verification.

## Wave state machine and cosmetics

Wave duration is 40 frames for smoke and 300 otherwise. Six definitions repeat
in order. Start a new wave when its duration elapses, reset the spawn deadline,
and preserve the RNG stream. The following thresholds are cumulative percent
cutoffs from `next()%100`; values above Runner select Tank.

| Wave | Spawn interval | Request size | Grunt cutoff | Runner cutoff | Radius offset | Boss burst |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 6 | 4 | 60 | 90 | 0 | no |
| 1 | 5 | 6 | 50 | 85 | 64 | no |
| 2 | 4 | 8 | 40 | 80 | 0 | yes |
| 3 | 4 | 10 | 45 | 85 | -64 | no |
| 4 | 3 | 12 | 35 | 80 | 32 | no |
| 5 | 3 | 14 | 25 | 75 | -128 | yes |

Request sizes scale by `enemy cap/200`. Reserve `min(enemy cap/20,32)` slots
from normal population. Frame 0 fills to cap minus reserve. Normal requests
are bounded by that population; a boss transition requests up to reserve
free slots at the full cap, with its first request forced to a boss Tank.
Thus the first boss burst is guaranteed room. Smoke reaches it at frame 80;
long scenarios reach it at frame 600, then another boss wave at frame 1500.

Each request consumes three draws, in order: angle index `next()%256`, radius
`base radius + wave offset + next()%257`, mix `next()%100`. Its position is
the current player position plus the radius times a boot-time 256-direction
circle table, clamped to [0,4095]. Forced boss kind still consumes the mix draw.
Requests append in order and receive monotonic IDs in the spawn phase.

Particles move at speed 24 in four cardinal directions, or 64 evenly spaced
boot-table directions in particle burst, and live 24 ticks including their
spawn frame's update. Particle burst creates a temporary numeric SoA request
per death before copying it into the retained particle pool, exercising burst
allocation. Particle processing consumes no RNG. Positions, counts, overflow,
allocation and rendering of particles are all excluded from simulation hashing.

## Canonical frame and hash order

1. Input: bot lookup and dodge against the previous enemy positions.
2. Director: advance the state machine and emit RNG-driven requests.
3. Spawn: append requests in order and assign stable IDs.
4. Grid: counting-sort enemy positions.
5. Steer: seek plus 24-unit separation.
6. Integrate: clamp enemy velocities and advance positions.
7. Fire: nearest weapon, then radial burst.
8. Bullets: integrate, decrease life, remove expired entries.
9. Collide: queue bullet damage and player touch events; remove spent bullets.
10. Events: apply damage, remove deaths, emit particles, pickups and score.
11. Particles: integrate and expire.
12. Pickups: magnet movement, collection and deterministic upgrades.
13. Hash: cheap state each frame; full positional state every 60 frames.
14. Render: cull and bulk-submit to the existing null backend.

The supported v1 dependency remains `$swarmcore` / `import("&swarmcore")`.
Park–Miller uses `s=(s*48271)%2147483647`; hash mixing uses
`h=(h*31+q)%1000000007`, normalizing negative q. Quantization truncates
`<int>(value*16.0)` toward zero. All products fit checked signed i64. Runtime
simulation uses f64 arithmetic, comparisons and sqrt; trigonometry occurs only
at boot. Clocks, GC policy and renderer state cannot affect the simulation hash.

The rolling hash starts at 1. Every completed frame mixes, in order: frame
number, RNG state, next enemy ID, enemy count, bullet count, player HP,
immunity deadline, shots, applied hits, kills, score, drops, collected pickups,
level, XP, next XP threshold, bullet damage, pierce upgrade, aimed count,
radial count, quantized magnet radius, next burst deadline, upgrade history
hash, director wave index/start/deadline, transitions, boss waves, burst spawn
count, total requests applied, and pickup count.

Every 60 completed frames append this positional pass:

- Player x,y.
- Each dense enemy: ID, kind, boss flag, HP, x,y,vx,vy,ax,ay.
- Each dense bullet: x,y,vx,vy,remaining life,damage,remaining pierce,hit-history
  length, then its previous hit IDs in order.
- Each dense pickup: x,y,value.

Reporting `fullHash` appends one further positional pass, including on frame
counts divisible by 60. Checkpoint 60 is the rolling hash after that frame's
positional pass. Empty queue storage, derived tables/grid/ID lookup, particles,
render content and timing/GC observations are excluded.

Render order is player, dense enemies, bullets, particles, pickups. Sizes are
20, twice enemy radius, 4, 3, 8; colors are respectively 1717987071,
4284901119, 4294967295, 4290773247, 1717960959. Bounding squares intersect
the camera inclusively. One `submitSoA` slice submits the visible batch.
Host drive consumes that batch in C; CLI drive consumes it in script.

GD-H1's host `--lane c` is an independent native reference for these same
rules. Its DOD==C smoke gate changes no rule, hash order or golden; see
[GD-H1 decisions](gd-h1-decisions.md).

Goldens and final counters live in [hashes.json](../goldens/hashes.json).
CLI interpreter/source-removed binary smoke repetition and CLI/host parity
remain required. Full scenario completion establishes correctness, without a
performance-budget or cross-platform libm claim. OOP/Hybrid lanes, `sweep`,
software/GL renderers, interactive input and GD-H2+ / M work remain deferred.
