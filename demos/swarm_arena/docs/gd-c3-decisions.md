# GD-C3 implementation decisions

GD-C3 implements only the CLI DOD smoke. GD-C4 host work and GD-C5 full gameplay
remain separate milestones. The exact smoke rules and hash ordering are in
[rules.md](rules.md); validation evidence is in [verification.md](verification.md).

Smoke starts at its 200-enemy cap so the grid and collision path do useful
work immediately. Its minimal director spawns on a square perimeter using
integer RNG values; this keeps trigonometry confined to boot-time bot tables.
Two bullets per fixed frame provide the scenario's 120 bullets/second. These
are explicit smoke rules, not a claim to implement GD-C5's full wave/weapons
behavior.

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

Imported class methods bind module values with local literal imports when they
need native functions or other script functions. This avoids missing lexical
module bindings during imported-class metadata compilation. The CLI loop is a
normal `run` function called from a small outer try/catch/finally; the original
large top-level protected block failed before entry execution.

Plugin calls from imported class methods/ordinary functions also failed binary
loading before script execution. Host timing/zone calls use a dynamic `object`
module value in the CLI run function, and `render.submit` uses the same form for
gfx. This retains the GD-C2 descriptor APIs and mode C `submitSoA`, and avoids
the failing direct native-call binding during artifact loading. The binary
gate removes **all** `.zr` files, including swarmcore and tests, before starting
each fresh process. No source fallback or engine patch is involved.

`String.toArray()` returned null in a runtime check. Cold numeric argument
parsing instead uses dynamic string subtraction (remove one trailing codepoint)
and decimal suffix comparisons, bounding every multiply/add before it executes.
Numeric spellings are bounded to the decimal width of each option's maximum;
excess leading zeroes are rejected along with other oversized input.

These are demo workarounds, not engine changes or claims that the deferred
features work. Preallocated columns do not establish an allocation/performance
budget; that measurement belongs to the later performance milestones.

The unit staging runner omits CLI/render/metrics/director modules that the unit
files do not use. Discovery otherwise recompiles those untested modules in
every child. Concurrent detector gates exceeded the process wrappers when
multiple ASan CLI/discovery workers exhausted available instance memory. The
final demo gates run one configuration at a time with CTest `-j1`. Final
timeouts allow 180 seconds per smoke process,
60 seconds per unit isolate and 480 seconds for the unit command. Bad-argument
CTest coverage uses six representative OS-exit cases per mode. Additional
Release exploratory checks exercised all 16 original argument cases successfully.
These Debug configurations perform frequent full collections and do not meet
the design's aspirational under-five-second detector smoke target. This is
recorded as a performance limitation, not an engine correctness failure.
