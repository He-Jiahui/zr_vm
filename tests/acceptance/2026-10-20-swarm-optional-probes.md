# Swarm Arena GD-C1: optional probe corrections (2026-10-20)

Scope: `perf/interpreter-20261017`, continuing from `d01387a9`. The required
GD-C1 fixes and their regressions are retained. A fresh fetch confirms that
`origin/main` is already an ancestor; no additional main merge was needed.
GD-C2, game features, game host/gfx plugins, and zrgame runtime work have not
started.

## Disposition

Interpreter and source-removed binary results agree in Release, ASan/UBSan, and
forced full GC with ASan/UBSan.

| Probe | Interp | Binary | Commit / disposition |
| --- | --- | --- | --- |
| P11 | OK | OK | `b398492b`: preserve the GC domain identity of loaded embedded functions. |
| P02 | OK | OK | `5688215e`: preserve inline operands and typed destinations through struct arithmetic. |
| P07 | OK | OK | `4ccf219f`: match imported iteration contracts and run implicit instance initializers. |
| P17 | OK | OK | `6deb6a3e`: lower f32 casts through an explicit, serializable rounding helper. |
| P20 | OK | OK | `da7eaa51`: retain native scalar layouts and caller inline argument storage. |
| P18 | OK | OK | `1b1666f0`: recognize declared project features even when disabled. |
| P14 `#gfx` | OK | OK | `206290fd`: resolve manifest aliases before native import metadata lookup. |
| P19 | FAIL | FAIL | `import_load_unavailable`; runtime bytecode reflection remains deferred. |
| P14 `@swarmcore` | FAIL | FAIL | Unknown import alias; production v2 package dependency/export integration remains deferred. |

`8b1a45c9` separately fixes the result slot of a `typeid(T)` expression used as
an ordinary argument. This does not complete P19.

## Root causes and semantics

* **P11:** an embedded child function loaded from bytecode had no GC domain
  identity. The managed function anchor used to create a local Place could not
  identify it. Inline-function initialization now assigns the current domain,
  preserving pool `tryBorrow(out view)` behavior across binary loading.
* **P02:** mixed struct/scalar arithmetic lost the operator's declared result
  type. Generic SUB/MUL/NEG also forced their operands into ordinary value slots
  and did not materialize inline values. Arithmetic now uses the declared
  operator result type, materializes operands through the common frame path,
  roots each carrier before constructing the next, and writes the result to its
  typed destination. ADD shares that rooted path; unary negation records its
  result layout. The regression covers chained Vec2 add/sub/mul/neg and Vec2
  multiplied by a float.
* **P07:** ordinary native members inherited the zero-valued constructor meta
  tag, and contract comparison treated provider-local and import-qualified
  spellings as different types. Non-meta members now use the sentinel tag;
  resolved nominal prototypes determine signature compatibility, including
  provider identity. Native requirements with metadata tokens do not need
  nonexistent source-symbol relations. Classes with instance initializers and
  no explicit constructor get an executable default constructor, including a
  default super call where applicable. Initializers run separately for each
  instance. The generated constructor appears in runtime reflection; the
  metadata tests now include it and give initializer expressions executable
  parameter bindings.
* **P17:** `<f32>` fell through to TO_STRING rather than numeric conversion.
  Lowering now converts to a floating value and explicitly rounds through
  `TZrFloat32`. The helper has stable serialization ID 11, so a freshly loaded
  binary has the same precision behavior. Dynamic positive/negative casts and
  f32 field storage are covered.
* **P20:** its instruction-209 symptom was independent of P17. Inference treats
  `float` as double, while several storage paths used f32 widths. Compiler,
  frame, and typed native binding layouts now consistently use double for
  `float`, with `f32` retaining four-byte storage. Complete nongeneric native
  scalar structs retain compiler/runtime layout metadata. Their calls reserve
  dedicated argument slots so earlier scalar temporaries cannot change the
  physical layout. Direct native dispatch records the caller's argument frame
  and slot coordinates; the native context follows those coordinates after
  stack relocation. InlineArgumentView can read those caller argument slots.
  Incomplete/generic layouts keep their existing representation. The fixture
  checks exact float precision, fields, and repeated direct native calls;
  this is not a claim of higher-order native nominal-call support.
* **P18:** manifest parsing ignored the `build.features` declaration array.
  Declared features now default to disabled and are known to conditional
  compilation. The existing top-level boolean `features` map selects values.
  Disabled calls skip argument evaluation; enabled calls execute. This change
  does not add build-profile selection. P18 reports `traceCalls=0`.
* **P14 alias:** the v2 alias parser was disconnected from production import
  canonicalization. Root-project native and workspace aliases now use its
  parsed target, letting native metadata lookup and binary linking use the
  provider's real module name. Required environment and project plugin search
  paths retain successful fresh-process binary linking.
* **Type identity arguments:** lowering emitted the correct type identity but
  did not publish its result slot. Ordinary argument lowering therefore copied
  the preceding callable slot. The expression now publishes its own slot. The
  regression checks the received identity marker and canonical name, so a
  non-null callable cannot satisfy it. The strengthened fixture passes both
  modes in all three configurations.

No opcode or bytecode-version change was needed. The f32 helper identifier and
native argument-source entry point are additive.

## Regression coverage and validation

Eight new fixtures, each registered in interpreter and source-removed binary
mode, bring `swarm_*` coverage from 12 to **28 CTests**:
`pool_view_out`, `struct_operators`, `user_iterable`, `f32_cast`,
`native_inline_arguments`, `declared_features`, `native_alias`, and
`typeid_argument`. Existing union, out/ref, callable, initializer, destructor,
and native-provider regressions remain included.

| Gate | Release | ASan/UBSan | Forced full GC + ASan/UBSan |
| --- | --- | --- | --- |
| Core CTest inventory, final per-test outcomes | 515/515 | 515/515 | 129/129 selected |
| All `swarm_*` CTests | 28/28 | 28/28 | 28/28 |
| Required demo mode checks | 38/38 | 38/38 | 38/38 |
| Successful demo mode checks, all cases | 58/62 | 58/62 | 58/62 |

Release completed the full core suite in 58.21 s. The first detector run
completed all 515 tests in 949.03 s: 513 passed, and the two metadata expectation
tests described above failed. Their targeted final rerun passes **2/2** in
630.40 s, completing successful final outcomes for the full 515-test inventory.
No engine code changed after that full detector run. No detector findings
occurred in these final validation runs. The forced-GC selection passes in
25.62 s; it retains the previous 100 tests, adds all 28 Swarm tests, and adds
the initializer hygiene metadata test. It is not a full forced-GC language
suite. The Release performance-report gate passes **1/1** for
`c,zr_interp,zr_binary` (464.18 s); this is not a new performance comparison.

All 31 demo cases were run through the demo's `run_probe.py` and `cases.json`
against each engine CLI. Successful binary checks require copied sources to
be removed and `executed_via=binary`. Existing embedded-host and native fixture
binaries use the matching engine libraries through `LD_LIBRARY_PATH` for this
engine-side gate. The demo integration separately rebuilds those executables
and libraries after merging this branch. Only four optional mode checks retain
FAIL status: P19 and the P14 package case, in each mode. An exact expected
diagnostic is not successful feature execution.

The existing build trees and knobs are unchanged:
`build/r2-merged-release`, `build/r2-asan`, and `build/r2-merged-gcstress`, with
`ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`, and the existing
`ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST` definition in the GC tree. This does not
claim leak-clean shutdown. New raw detector, forced-GC, build, and probe logs
are only under `/tmp/zr-swarm-*`; CTest Temporary paths point there through
local symlinks.

## Deferred findings

* **P19:** the runtime import route supports registered metadata runtimes, but
  bytecode entry modules do not publish the equivalent reflection service.
  Opening that route alone is insufficient: TypeId constants need authenticated
  rehydration after deserialization, and source TypeId resolution needs the
  declaration's fields/attributes rather than a generic type literal.
  Exploratory bridge changes were removed; the original unavailable-module
  diagnostic remains stable. Only the independent argument-slot correction
  was retained and tested.
* **P14 package:** the v2 manifest stores dependencies and explicit exports,
  but production module keys/provider loading still use the legacy dependency
  package tables. Completing package loading requires connecting these models,
  export lookup, and source-removed binary provider selection. The native alias
  correction does not pretend to implement that package contract.
* The previously recorded distinct callable signatures in multiple Array
  specializations and higher-order native nominal signatures remain outside
  these regressions. No game lane or fallback is selected.

## Commands and evidence

Core and performance commands follow the previous acceptance note:

```sh
export PATH="$PWD/build/local-tools/usr/bin:$PWD/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/build/local-tools/usr/lib/x86_64-linux-gnu"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core
ctest --test-dir build/r2-merged-release -j6 -E '^performance_report$' --output-on-failure
ctest --test-dir build/r2-asan -j6 -E '^performance_report$' --output-on-failure
ctest --test-dir build/r2-asan -j2 -R '^(language_pipeline|macro_syntax_hygiene_field_results)$' --output-on-failure
ctest --test-dir build/r2-merged-gcstress -j4 --tests-from-file /tmp/zr-swarm-opt-gc-tests.txt --no-tests=error --output-on-failure
ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary ctest --test-dir build/r2-merged-release -R '^performance_report$' --output-on-failure
```

Final core logs use `/tmp/zr-swarm-opt-{release-core-final2,asan-core-final,asan-core-rerun,gc-core-final2}.log`.
The performance log is `/tmp/zr-swarm-opt-release-performance.log`.
Per-case commands, CLI hashes, source-removal state, and runtime results are in
`/tmp/zr-swarm-opt-engine-{release,asan,gcstress}-probes/`.
