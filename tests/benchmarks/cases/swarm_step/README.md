# Swarm step

A self-contained integer kernel inspired by the data-oriented swarm arena game
step. Each invocation initializes separate arrays for enemy position, velocity
and health, and bullet position, lifetime and target. It steers enemies toward a
deterministic moving player, integrates wrapped coordinates, moves homing
bullets, applies target hits, respawns dead enemies, and recycles expired bullets.
Hits take effect in bullet index order. After each frame it hashes every state
column and cumulative hit/kill counts in entity index order.

Coordinates wrap modulo 4096. The hash starts at 17 and folds with multiplier
131 modulo 1000000007. Remainder operands are nonnegative; intermediate integers
stay below 2^53 for registered scales, including the hash multiplication, so C,
ZR, Python, Node and Lua compute the same result without overflow or floating
point rounding. Lua uses zero-based table keys for the same entity IDs.

| Tier | Scale | Enemies | Bullets | Frames | Checksum |
| --- | ---: | ---: | ---: | ---: | ---: |
| smoke | 1 | 64 | 128 | 24 | 436940475 |
| core | 4 | 256 | 512 | 96 | 49001808 |
| profile | 1 | 64 | 128 | 24 | 436940475 |
| stress | 16 | 1024 | 2048 | 384 | 662750722 |

This is a representative benchmark workload, with its own checksum contract.
Its integer steering and direct target collision checks do not reproduce the
full game's floating point physics, spatial grid, event queue or golden hashes.
It imports no game, host, graphics or renderer code. All kernels live here;
the shared runners supply argument parsing and the banner/checksum protocol.

Run the existing suite with `ZR_VM_PERF_ONLY_CASES=swarm_step` and
`ZR_VM_TEST_TIER=smoke` (or `core`, `profile`, `stress`). The suite copies the ZR
fixture and generates its tier-specific `bench_config.zr` in the build directory.
