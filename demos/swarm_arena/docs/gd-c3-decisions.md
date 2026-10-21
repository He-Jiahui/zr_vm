# GD-C3 implementation decisions

GD-C3 implements only the CLI DOD smoke. GD-C4 host work and GD-C5 full gameplay
remain separate milestones. The exact smoke rules and hash ordering are in
[rules.md](rules.md); validation evidence is in [verification.md](verification.md).

The demo uses a **v1 manifest**, a `$swarmcore` local dependency and
`import("&swarmcore")`. This uses the production legacy package resolver, rather
than the deferred v2 `@swarmcore` dependency/export integration. RNG and hash
code remain in `game/packages/swarmcore`; there is no copied implementation.
P14's v2 package status and P19 runtime reflection status remain deferred.

Nested workspace imports use slash paths such as `lanes/dod/world`. Public
native array field types use canonical `zr.container.Array<T>` names. Dotted
workspace spellings and local aliases in exported field types did not resolve
consistently in this module graph. Column preallocation is written directly in
constructors to avoid resolving module helper functions from imported classes.

The event ring is concrete, with `Array<GameEvent>` storage and scalar `hit`,
`killed` and `playerHit` methods. Union construction and exhaustive switching
stay in the declaring module; decoded scalar fields are reused on each pop.
This avoids imported generic constructors/return metadata, exported union
constructors/patterns, and union method/out arguments that failed in this
composition. It allocates no decoded event object and keeps FIFO/capacity/wrap
semantics. The existing GD-C1 local generic/out/union probes remain unchanged;
their passing results do not establish every cross-module composition.

The stock `test` command enumerates `source` and ignores `testSource`.
`tools/run_zr_unit_tests.py` copies the game to a private
`/tmp/zr-swarm-gd-c3-units-*` project and stages the canonical `game/tests` files
under its `src/tests`, then invokes **stock `zr_vm_cli test`**. The repository
source tree is untouched. This also avoids package-qualified public type
metadata when testing the game as a second dependency package. The legacy
swarmcore dependency itself is exercised directly by the RNG tests.

These are demo workarounds, not engine changes or claims that the deferred
features work. Preallocated columns do not establish an allocation/performance
budget; that measurement belongs to the later performance milestones.
