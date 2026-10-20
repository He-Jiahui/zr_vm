# Feature readiness — GD-C1 optional probe corrections

All required GD-C1 probes and seven corrected optional paths pass in Release,
ASan/UBSan, and forced full GC with ASan/UBSan after merging engine
`ed10137f`. Only P19 runtime reflection and P14's package import
remain deferred. GD-C2 and game features have not started.

The engine [optional-probe acceptance note](../../../tests/acceptance/2026-10-20-swarm-optional-probes.md)
records causes, semantics, regression coverage, and final engine gates. The
[required-blocker acceptance note](../../../tests/acceptance/2026-10-19-swarm-gd-c1-engine-blockers.md)
and its P04/P05/P08/P09/P14 fixes remain intact.

## Inventory and parity scope

[cases.json](../probes/cases.json) defines 31 cases and 62 mode checks.
[status.json](../probes/status.json) records results, fix commits, CLI and runtime
library hashes, and optional P14 variant statuses. Every configuration passes **38/38 required
checks**, **58/62 successful checks overall**, and **64/64 demo CTests**.
The four remaining failing mode checks reproduce the exact P19/package
diagnostics. CTest uses WILL_FAIL only for those checks; feature status stays
FAIL. Detector reports, unexpected failures, and timeouts cannot satisfy them.

P22 gates all **18 required P01–P20 pairs**, including variants, with successful
interpreter/source-removed binary execution and matching nonempty CLI hashes.
It reports optional parity separately: 10 optional pairs pass, while P19 and
the package pair fail. P21's embedded-host case is checked separately by the
feature gate. Matching failures are not successful feature execution.

R/P/L follow design §5. P14's optional alias and package cases remain separate
from its required environment/project plugin search paths. The following
statuses hold in all three configurations.

| Probe | Tier | Interp | Binary | Evidence / decision |
| --- | --- | --- | --- | --- |
| P01 | R | OK | OK | Struct-array explicit writeback, direct field writes, copy isolation, nested constructors, and the 10k-loop pass; the host access-cost case also passes. |
| P02 | P | OK | OK | Vec2 add/sub/mul/neg and mixed Vec2 * float preserve inline operands and typed results; fixed by 5688215e. |
| P03 | R | OK | OK | Interface-typed field dispatch, virtual/override/super, properties and static counter all pass. |
| P04 | R | OK | OK | Union variants and payloads survive calls, arrays, generic storage, and full GC; fixed by 89e92cff. |
| P05 | R | OK | OK | Generic queue instance initializers, capacity/wrap behavior, and caller out-argument updates pass; fixed by 2a7b1b3e with readonly compatibility in 3c0b025a. |
| P06 | R | OK | OK | Array<float> for-in, C-style for with += and while return equal sums. |
| P07 | P | OK | OK | User Enumerator/Iterable contracts match imported nominal types; default constructors run independent instance initializers; fixed by 4ccf219f. |
| P08 | R | OK | OK | Scope/using cleanup and host program-end destructor checks pass in all configurations; c97113cd re-reads operands after destructor stack growth. |
| P09 | R | OK | OK | Function-typed fields and arrays call escaping shared captures correctly, including after full GC; fixed by 8d575068. |
| P10 | L | OK | OK | Job<int> construction, currentScheduler.schedule and result return 17. |
| P11 | L | OK | OK | Pool deliver/borrow/write/close/recycle and tryBorrow(out view) pass in both modes; embedded function GC domain identity fixed by b398492b. |
| P12 | R | OK | OK | GC stats, 500us budget, minor request and process.arguments pass. A request need not finish after one step; snapshot allocation can trigger a full GC in Debug, so last-request kind is not asserted. |
| P13 | R | OK | OK | native extern into absolute libzrgame_ffi.so path passes i64 and f64 checks. Relative/search resolution not needed for the selected absolute-path contract. |
| P14 | R | OK | OK | Descriptor plugins pass both required search paths and optional #gfx metadata lookup in both modes; alias fixed by 206290fd. @swarmcore remains deferred because v2 package dependencies/exports are disconnected from production provider loading. |
| P15 | P | OK | OK | Runtime template interpolation produces value=7. |
| P16 | R | OK | OK | Safe Park-Miller (10k steps), bounded mod-prime hash and wide uint pass. Separate runtime observations prove signed +/* overflow errors and uint addition wraps to zero. |
| P17 | P | OK | OK | Explicit f32 casts and field precision pass, including dynamic positive/negative rounding; serializable conversion fixed by 6deb6a3e. |
| P18 | L | OK | OK | Declared trace feature is recognized while disabled; traceCalls=0, disabled argument skipping, enabled feature calls, and comptime checks pass; fixed by 1b1666f0. Build-profile selection is not added. |
| P19 | L | FAIL | FAIL | Runtime zr.reflection import still reports import_load_unavailable. Bytecode TypeId rehydration and declaration field/attribute resolution remain deferred. The independent ordinary typeid argument-slot fix is 8b1a45c9. |
| P20 | P | OK | OK | Native scalar struct construction, float precision, and InlineArgumentView pass in both modes; caller argument coordinates and native layout metadata fixed by da7eaa51. |
| P21 | R | OK | OK | Host spike passes: entry side effects, canonical module key game, 10000 tick calls, expected exception, ResetThread and a succeeding call. Source compilation is disabled in binary host. |
| P22 | R | OK | OK | Required-case parity passes all 18 required P01-P20 pairs; only P19 and the optional package pair lack successful parity. P21 is gated independently. |

## Validation

| Gate | Release | ASan/UBSan | Forced full GC + ASan/UBSan |
| --- | --- | --- | --- |
| Engine core CTest inventory, final per-test outcomes | 515/515 | 515/515 | 129/129 selected |
| Engine swarm regressions, interp + binary | 28/28 | 28/28 | 28/28 |
| Demo CTests | 64/64 | 64/64 | 64/64 |
| Required mode checks | 38/38 | 38/38 | 38/38 |
| Successful mode checks, all cases | 58/62 | 58/62 | 58/62 |
| P22 required parity pairs | 18/18 | 18/18 | 18/18 |

The Release engine performance gate passes 1/1 for
`c,zr_interp,zr_binary`. The detector core result combines the full inventory
run with a successful rerun of two adjusted metadata expectations; the engine
acceptance note records the exact counts and times. The forced-GC selection
contains the previous 100 tests, all 28 swarm tests, and the initializer hygiene
metadata test. It is not a full forced-GC language-suite run.
`ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1` are unchanged.
No detector findings occur in the final runs.

Every successful binary probe compiles a fresh artifact, removes copied sources,
and requires binary execution evidence. Existing native fixtures and the
embedded host were rebuilt against each configuration's current engine.
Raw evidence and CTest Temporary output stay under `/tmp/zr-swarm-*` through
local build-directory symlinks. Exact commands, hashes, and times are recorded
in [verification.md](verification.md).

## Deferred findings and v1 definition-of-done gaps (§8.5)

P19 needs runtime bytecode reflection publication, authenticated TypeId
rehydration after binary loading, and declaration field/attribute lookup.
The independent ordinary `typeid` argument-slot correction is included, but
runtime `zr.reflection` remains unavailable. P14's package case needs production
integration between v2 manifest dependencies/exports and module/provider
loading. Both retain their original diagnostics and expected-failure checks.
The previously recorded distinct callable signatures in multiple Array
specializations and higher-order native nominal calls remain outside these
regressions.

No game lane or fallback is selected. DOD/OOP/Hybrid/C simulations, gameplay
hashes, lane equivalence, game determinism, CLI/host gameplay parity, W0–W14
baselines, capacity@60Hz, renderers, screenshots, and frame/GC distributions
remain future work. P21 is an API spike. GD-C2–C5, GD-H1–H6, and M-tier
implementation have not started.
