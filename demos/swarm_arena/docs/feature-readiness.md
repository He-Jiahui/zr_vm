# Feature readiness — GD-C1 required probes pass

All required GD-C1 probes pass in Release, ASan/UBSan, and forced full GC with
ASan/UBSan after merging engine `d01387a9`. The same statuses hold in
all three configurations. Work stops at GD-C1 as requested. GD-C2 and game
features have not started.

The engine acceptance note is
[GD-C1 engine blockers](../../../tests/acceptance/2026-10-19-swarm-gd-c1-engine-blockers.md).
It records the root causes, semantics, regression coverage, and engine gates.
The P08 ownership-drop fix `c97113cd` is retained unchanged. P04 (`89e92cff`),
P09 (`8d575068`), P05 (`2a7b1b3e`, `3c0b025a`), and P14 (`3d69cee7`) retain and
complete the original work in progress.

## Inventory and parity scope

[cases.json](../probes/cases.json) defines 31 cases and 62 mode checks.
[status.json](../probes/status.json) records per-configuration results and CLI
hashes. Each configuration passes **38/38 required mode checks** and
**45/62 successful mode checks overall**. The remaining 17 mode checks reproduce
the documented optional failures. All **64/64 demo CTests** pass, including P22
and the required-feature gate. CTest uses `WILL_FAIL` for the exact optional
diagnostics; those probes retain feature status FAIL. Detector reports,
unexpected failures, and timeouts cannot satisfy these expected failures.

P22 now follows this task's required-probe gate: it requires successful
interpreter/source-removed binary parity for **all 18 required P01–P20 case
pairs**, including variants, with a matching nonempty CLI hash. It reports the
nine optional pairs without parity separately. The original aggregate required
every P01–P20 main case, including optional features; this explicit scope change
allows the required gate to pass while retaining those optional failures.
Matching failure messages never count as successful feature execution. P21's
embedded-host case passes separately in both modes and is checked by the gate.

R/P/L follow design §5. The two optional P14 import cases remain separate from
its required plugin search paths.

| Probe | Tier | Interp | Binary | Evidence / decision |
| --- | --- | --- | --- | --- |
| P01 | R | OK | OK | Struct-array explicit writeback, direct field writes, copy isolation, nested constructors, and the 10k-loop pass; the host access-cost case also passes. |
| P02 | P | FAIL | FAIL | Struct operator evaluation reaches GET_MEMBER with an unsuitable receiver; optional operator path remains unavailable. |
| P03 | R | OK | OK | Interface-typed field dispatch, virtual/override/super, properties and static counter all pass. |
| P04 | R | OK | OK | Union variants and payloads survive calls, arrays, generic storage, and full GC; fixed by 89e92cff. |
| P05 | R | OK | OK | Generic queue instance initializers, capacity/wrap behavior, and caller out-argument updates pass; fixed by 2a7b1b3e with readonly compatibility in 3c0b025a. |
| P06 | R | OK | OK | Array<float> for-in, C-style for with += and while return equal sums. |
| P07 | P | FAIL | FAIL | User Enumerator/Iterable implementation rejected as not implementing all abstract/interface members. Optional; no fallback selected. |
| P08 | R | OK | OK | Scope/using cleanup and host program-end destructor checks pass in all configurations; c97113cd re-reads operands after destructor stack growth. |
| P09 | R | OK | OK | Function-typed fields and arrays call escaping shared captures correctly, including after full GC; fixed by 8d575068. |
| P10 | L | OK | OK | Job<int> construction, currentScheduler.schedule and result return 17. |
| P11 | L | OK | FAIL | Pool deliver/borrow/write/close/recycle passes in interp; binary tryBorrow(out view) still reports PROPERTY_REF_CREATE_LOCAL: invalid local Place. |
| P12 | R | OK | OK | GC stats, 500us budget, minor request and process.arguments pass. A request need not finish after one step; snapshot allocation can trigger a full GC in Debug, so last-request kind is not asserted. |
| P13 | R | OK | OK | native extern into absolute libzrgame_ffi.so path passes i64 and f64 checks. Relative/search resolution not needed for the selected absolute-path contract. |
| P14 | R | OK | OK | Descriptor plugins link in fresh source-removed binary processes via both ZR_VM_NATIVE_PATH and project/native; fixed by 3d69cee7. Optional #gfx metadata and @swarmcore package imports still fail. |
| P15 | P | OK | OK | Runtime template interpolation produces value=7. |
| P16 | R | OK | OK | Safe Park-Miller (10k steps), bounded mod-prime hash and wide uint pass. Separate runtime observations prove signed +/* overflow errors and uint addition wraps to zero. |
| P17 | P | FAIL | FAIL | f32 cast/field lowering reports invalid operand for bytecode instruction 209; size/precision remain unverified. |
| P18 | L | FAIL | FAIL | Declared trace feature rejected as conditional.unknown_feature; profiles/comptime combined probe cannot complete. |
| P19 | L | FAIL | FAIL | Runtime import zr.reflection fails with import_load_unavailable; field/attribute enumeration unverified. |
| P20 | P | FAIL | FAIL | Native inline-struct construction reports invalid operand for bytecode instruction 209 in both modes; binary linking now succeeds. InlineArgumentView assertion remains unreached. |
| P21 | R | OK | OK | Host spike passes: entry side effects, canonical module key game, 10000 tick calls, expected exception, ResetThread and a succeeding call. Source compilation is disabled in binary host. |
| P22 | R | OK | OK | Required-case parity aggregate passes all 18 required P01-P20 case pairs; nine optional pairs without successful parity are reported separately. P21 is gated independently. |

An additional exploratory case with two distinct callable signatures in separate
`Array` specializations in one module still reports an incompatible callable-type
error. This broader generic-type limitation is recorded in the engine acceptance
note and is outside the required P09 case.

## Validation

| Gate | Release | ASan/UBSan | Forced full GC + ASan/UBSan |
| --- | --- | --- | --- |
| Engine core CTests | 499/499 | 499/499 | 112/112 selected |
| Engine swarm regressions, interp + binary | 12/12 | 12/12 | 12/12 |
| Demo CTests | 64/64 | 64/64 | 64/64 |
| Required mode checks | 38/38 | 38/38 | 38/38 |
| Successful mode checks, all cases | 45/62 | 45/62 | 45/62 |
| P22 required parity pairs | 18/18 | 18/18 | 18/18 |

The Release engine performance gate also passes 1/1 for
`c,zr_interp,zr_binary`; this is not a new performance comparison. The forced-GC
engine gate retains the previous 100-test selection and adds all 12 swarm tests;
it is not a complete forced-GC language-suite run. Leak detection remains
disabled with `ASAN_OPTIONS=detect_leaks=0`; `UBSAN_OPTIONS=halt_on_error=1` is
unchanged. No detector findings occur in the final runs.

Every required binary case compiles a fresh entry artifact, removes the copied sources,
and requires binary execution evidence. Native fixtures and the embedded host
are rebuilt against each configuration's current engine. Raw probe logs and private
probe projects are retained only under `/tmp/zr-swarm-demo-{release,asan,gcstress}-probes`.
Local build `probe-results` and CTest `Testing/Temporary` paths point to `/tmp`
through symlinks. Exact commands and CTest totals are in [verification.md](verification.md).

## Remaining v1 definition-of-done gaps (§8.5)

The optional failures above remain outside the required engine gate. No fallback
or game lane has been selected. DOD/OOP/Hybrid/C simulations, gameplay hashes,
lane equivalence, game determinism, CLI/host game parity, W0–W14 baselines,
capacity@60Hz, renderers, screenshots, and frame/GC distributions remain future
work. P21 is an API spike rather than GD-C4 gameplay coverage. GD-C2–C5, GD-H1–H6,
and M-tier implementation have not started.
