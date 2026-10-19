# Feature readiness — GD-C1 blocked after the P01 fix

P01 **PASSES** in interpreter and source-removed binary mode after the engine
fixes merged by `3fc8b406`. All probes P01–P22 have now been evaluated. Required
P04, P05, P09, P14 (binary) and P22 fail. **P08 also fails under ASan and forced
GC with a heap use-after-free**, despite passing Release. John's required-probe
stop rule gates GD-C2 and every later phase. No fallback, lane reduction, or
engine edit has been adopted in this batch.

## Inventory

These mode columns describe Release. ASan/UBSan and forced-GC checks reproduce
the same statuses, except P08, which fails in both modes under both sanitizer
configurations. OK requires assertions, the probe marker and the requested
execution-mode marker. Signed overflow observations in P16 instead require
the exact checked-arithmetic error. FAIL is a real feature failure, even when
CTest accepts its documented diagnostic through `WILL_FAIL`.

R/P/L follow design §5; P05's generic `out T` and P14's aliases/packages are
optional parts of otherwise required probes. The machine-readable inventory is
[status.json](../probes/status.json); [cases.json](../probes/cases.json) defines
the 31 reproducible cases (62 mode checks).

| Probe | Tier | Interp | Binary | Evidence / decision |
|---|---|---|---|---|
| P01 | R | OK | OK | PASS after merged engine fixes: explicit writeback, direct field write-through, copy isolation, nested constructor and 10k-loop checks; host access-cost probe also passes. |
| P02 | P | FAIL | FAIL | GET_MEMBER while evaluating struct operator expression; optional operator path unavailable. |
| P03 | R | OK | OK | Interface-typed field dispatch, virtual/override/super, properties and static counter all pass. |
| P04 | R | FAIL | FAIL | Required FAIL: Array<GameEvent>.add(GameEvent.Hit(4, 9)) raises KNOWN_VM_CALL before union/generic storage assertions. Stopped; no workaround adopted. |
| P05 | R | FAIL | FAIL | Required FAIL: generic RingQueue<int>.push raises GET_MEMBER: missing member 'size'. Stopped; no concrete replacement adopted. Generic out and wrap checks are not reached. |
| P06 | R | OK | OK | Array<float> for-in, C-style for with += and while return equal sums. |
| P07 | P | FAIL | FAIL | User Enumerator/Iterable implementation rejected as not implementing all abstract/interface members. Optional; no fallback selected. |
| P08 | R | OK | OK | Release scope/using and host program-end destructor checks pass. Required sanitizer FAIL: ASan and forced-GC report heap-use-after-free at value.h:152 / execution_dispatch.c:10019 after a destructor grows the VM stack. Stopped; no workaround adopted. |
| P09 | R | FAIL | FAIL | Required FAIL: function-typed field invocation this.callback(x) is rejected with Argument count mismatch. Stopped; escaping capture/array assertions not reached. |
| P10 | L | OK | OK | Job<int> construction, currentScheduler.schedule and result return 17. |
| P11 | L | OK | FAIL | Interp pool deliver/borrow/write/close/recycle passes. Source-removed binary repeatedly fails at tryBorrow(out view): PROPERTY_REF_CREATE_LOCAL: invalid local Place. Optional M3 is unavailable; no fallback selected. |
| P12 | R | OK | OK | GC stats, 500us budget, minor request and process.arguments pass. A request need not finish after one step; snapshot allocation can trigger a full GC in Debug, so last-request kind is not asserted. |
| P13 | R | OK | OK | native extern into absolute libzrgame_ffi.so path passes i64 and f64 checks. Relative/search resolution not needed for the selected absolute-path contract. |
| P14 | R | OK | FAIL | Required plugin loads via ZR_VM_NATIVE_PATH and project/native in interp; fresh binary compiles but fails to load the entry with source removed. Optional #gfx metadata and @swarmcore imports also fail. No workaround adopted. |
| P15 | P | OK | OK | Runtime template interpolation produces value=7. |
| P16 | R | OK | OK | Safe Park-Miller (10k steps), bounded mod-prime hash and wide uint pass. Separate runtime observations prove signed +/* overflow errors and uint addition wraps to zero. |
| P17 | P | FAIL | FAIL | f32 cast/field probe raises invalid operand for bytecode instruction 209. Size/precision unverified; no f32 claim. |
| P18 | L | FAIL | FAIL | Declared trace feature rejected as conditional.unknown_feature; profiles/comptime combined probe cannot complete. |
| P19 | L | FAIL | FAIL | Runtime import zr.reflection fails with import_load_unavailable; field/attribute enumeration unverified. |
| P20 | P | FAIL | FAIL | Valid native SpriteInstance descriptor and constructor compile. Interp raises invalid operand for bytecode instruction 209; source-removed binary cannot load entry. Native InlineArgumentView assertion not reached. |
| P21 | R | OK | OK | Host spike passes: entry side effects, canonical module key game, 10000 tick calls, expected exception, ResetThread and a succeeding call. Source compilation is disabled in binary host. |
| P22 | R | FAIL | FAIL | Aggregate FAIL: successful interp/source-removed binary parity for P01-P20 is blocked by P02/P04/P05/P07/P09/P11/P14/P17/P18/P19/P20. Matching failure diagnostics are not successful feature parity. |

P22 is an aggregate check of successful P01–P20 execution, not another simulation.
Matching failure messages do not count as successful binary feature parity.
Package sources are also removed after compilation. The source filenames in
binary exception stacks come from retained debug metadata.

## Required engine findings

### C01 — Nested union payload call in Array.add fails (P04)

[p04 main.zr](../probes/p04_union_storage/src/main.zr:19) compiles, but
`events.add(GameEvent.Hit(4, 9))` raises
`KNOWN_VM_CALL: invalid known VM callable` in both modes. The later array,
generic-box and switch assertions are not reached. This is a distinct remaining
call shape from P01's now-passing nested struct constructor. No split-construction
workaround has been adopted.

### C02 — Generic RingQueue loses a member (P05)

[p05 main.zr](../probes/p05_generic_queue/src/main.zr:15) constructs a
`RingQueue<int>(2, 0)`. Its first `push` raises
`GET_MEMBER: missing member 'size'` at `this.size`. Generic queue storage is R;
this failure occurs before the optional generic-out and concrete-out assertions.
No concrete queue replacement has been selected.

### C03 — Function-typed field call is rejected (P09)

[p09 main.zr](../probes/p09_closures/src/main.zr:6) declares
`callback: fn(int) -> int`, assigns it in the constructor and calls
`this.callback(x)`. Compilation reports `Argument count mismatch`. Both modes
fail compilation; the escaping shared-capture and array-call assertions remain
unverified. The source is retained as the required OOP EventBus blocker.

### C04 — Descriptor plugin cannot be consumed by source-removed binary (P14)

[p14 main.zr](../probes/p14_plugin_package/src/main.zr) imports the minimal
`zrgame.probe` descriptor and asserts `value() == 37`. Interpreter execution
passes via both `ZR_VM_NATIVE_PATH` and `<project>/native`. Compilation returns 0
and creates a fresh entry `.zro`, but after removal of source, binary execution
exits 1 with `failed to load project entry: main` for both search paths. The
provider contains only the value function; P20's native struct is in a separate
plugin so it cannot contaminate P14's contract. The loader's root cause is not
established. Optional native alias/package checks fail separately.

### C05 — Ownership release uses freed stack storage (P08, sanitizer)

[p08 main.zr](../probes/p08_lifetimes/src/main.zr) passes the Release checks for
scope cleanup, `@close`, and no duplicate destructor after full GC. In **both
interpreter and binary** runs, **both ASan and forced-GC** builds report:

```text
ERROR: AddressSanitizer: heap-use-after-free
ZrCore_Value_PrepareDestinationForOverwriteNoProfile  value.h:152
execution_prepare_destination_for_direct_store_no_profile
ZrCore_Execute  execution_dispatch.c:10019 (OWN_DROP)
```

The freed-by stack includes `stack_realloc_internal` → `ZrCore_Stack_GrowTo` →
VM destructor invocation through `ownership_resource_call_destructor`. This
supports a stale destination after destructor-induced stack growth; the complete
root cause has not been established. These tests stay **red**, without
`WILL_FAIL`; the harness refuses to accept any sanitizer error as a known
feature diagnostic. Program-end cleanup is independently observed after entry
return through the embedded host's `finished()` / `afterGC()` calls and passes.

P22 remains a required aggregate failure because the underlying features above
and optional probes do not all execute successfully from binary artifacts.

## Other findings (H/M)

- **H01, P02:** struct operator expression raises `GET_MEMBER`; no claim of
  working mixed Vec2/scalar operators.
- **H02, P11:** pool operations pass in interpreter mode; source-removed binary
  raises `PROPERTY_REF_CREATE_LOCAL: invalid local Place` at `tryBorrow(out view)`.
  This reproduced in three additional fresh Release compilations. Pool lane M3
  remains unavailable.
- **H03, P17/P20:** f32 lowering and native SpriteInstance construction hit
  invalid bytecode operand 209. InlineArgumentView's C assertion is not reached;
  mode B has not been selected. P20 binary also cannot load its entry.
- **M01, P07:** the user Enumerator/Iterable implementation is rejected as not
  implementing all interface members; custom iteration remains unverified.
- **M02, P18:** `trace` is rejected as undeclared despite the manifest's build
  feature/profile declarations; profile stripping/comptime contract is unverified.
- **M03, P19:** `zr.reflection` cannot load at runtime; field and runtime
  attribute enumeration are unverified.
- **M04, P14 extras:** `#gfx` import metadata and `@swarmcore` package resolution
  fail independently. The primary plugin test uses a direct module import.

## Reproduction and retained evidence

Build and run commands are in the [README](../README.md). Every individual case
retains a fresh project, commands, exit statuses, elapsed time, CLI SHA-256,
source-removal state, `compile.log`, `run.log`, and `result.json` under:

```text
build/demo-{release,asan,gcstress}/demos/swarm_arena/probe-results/<case>-<mode>/
```

Important paths: `p01-main-{interp,binary}`, `p01-nested-*`, `p04-main-*`,
`p05-main-*`, `p09-main-*`, `p14-main-binary`, `p14-project-binary`, and
`p08-main-*` in the sanitizer trees. `p22-result.json` lists every successful and
failed parity dependency. Exact CTest results and core inventory evidence are in
[verification.md](verification.md).

## Remaining v1 definition-of-done gaps (§8.5)

1. The Release suite remains red at P22 and the feature gate; sanitizer suites
   also expose P08's memory error. Game smoke coverage does not yet exist.
2. No DOD/OOP/Hybrid/C simulation, gameplay golden hashes, lane equivalence,
   three-run game determinism or CLI/host game parity exists. P21 is only a host
   API spike and cannot substitute for GD-C4.
3. All 22 probes have statuses and actionable evidence; required failures above
   must be resolved and rerun before GD-C2 onward.
4. No W0–W14 game baseline, capacity@60Hz, GC policy distributions, thresholds,
   software renderer or snapshot exists. C1 timings are exploratory only.
5. Demo OFF/ON core inventories are identical at **431** tests. The six-test
   increase over the old 425 baseline comes from the already-merged P01 fixes.
   This batch changes only `demos/swarm_arena/**`. The complete core suite has
   not been rerun; its six P01 regression checks pass.

GD-C2–C5, GD-H1–H6 and M-tier have not started. Resumption requires the remaining
required failures to be resolved or an explicit change from John to their scope.
