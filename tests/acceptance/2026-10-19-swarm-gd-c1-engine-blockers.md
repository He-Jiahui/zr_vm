# Swarm Arena GD-C1: engine blockers

Scope: `perf/interpreter-20261017`, continuing from merge `7abb5496` and the
P08 fix `c97113cd`. The earlier branch history is retained. No opcode, bytecode
version, or public ABI change is required. GD-C2 and game features are outside
this change.

## Reproduction and WIP disposition

The original demo runner and `probes/cases.json` reproduce all cases with a
private project per mode. Binary runs compile a fresh entry `.zro`, remove the
copied sources, and require `executed_via=binary`.

| Probe | Original failure | WIP disposition / fix |
| --- | --- | --- |
| P04 | Union arguments failed at VM calls or returned the wrong value. | Kept the union carrier conversions and fixture; committed `89e92cff`. |
| P05 | Generic queue instance fields were missing; then caller out-arguments were not updated. | Kept instance initializers, completed managed argument passing, and added coverage; committed `2a7b1b3e`, with readonly compatibility in `3c0b025a`. |
| P09 | Function field calls reported an argument count mismatch; callable arrays could use the wrong caller coordinates. | Kept signature inference and resolved VM dispatch; refined generic callable parsing; committed `8d575068`. |
| P14 | The native module was not linked yet in a fresh binary process. | Kept declared-provider discovery and added an executable plugin fixture; committed `3d69cee7`. |
| P08 | Ownership release retained a stale stack pointer after destructor stack growth. | Existing `c97113cd` was left unchanged and revalidated. |

## Causes and semantics

* **P04:** an inline union's complete value lives in frame bytes while an
  ordinary call/array/generic boundary needs its carrier object. The shared
  frame conversion routines now handle union layouts in both directions,
  preserving the active variant and its payload. The carrier stays rooted
  while its members are constructed.
* **P05 initializers:** class constructors did not emit instance field
  initializer expressions. They now run in declaration order, with `this`
  bound to the instance, after the super-constructor call and before the
  constructor body. Each construction evaluates its own initializers.
* **P05 arguments:** ordinary calls copied an `out`/`ref` local as a value.
  Calls now pass managed references to caller locals, fields, or elements;
  parameter loads and stores dereference that storage. Forwarding an argument
  forwards its original target. Two references to one local observe each
  other's writes. Semantic IR represents the parameter storage as a writable
  reference while retaining the declared value type for source checking.
  Mutable reference parameters use value slots for their carriers. Existing
  `ref readonly` and `scoped ref readonly` aggregate parameters retain borrowed
  inline storage.
* **P09:** field lookup used method argument rules for fields whose declared
  type was a function. Type inference and lowering now use the field's
  function signature. Generic type-name parsing also recognizes callable
  arrows and parameter-list commas. When a generic call resolves to a VM
  function, dispatch uses the prepared resolved path with the original caller
  frame and argument slots; inline values need not have a populated value
  mirror. Captured state remains shared between field and array calls.
* **P14:** binary linking can precede execution of the source imports that
  register a descriptor plugin. On a module mismatch, linking discovers the
  function's declared static-import providers and retries the normal
  module-hash/token/signature resolver. Existing binding checks still decide
  whether a discovered provider matches.

## Regression coverage

Twelve `swarm_*` CTests exercise six fixtures in interpreter and source-removed
binary modes:

* `destructor_stack_growth`: the existing P08 stack-growth regression.
* `union_call_storage`: direct/inline union arguments, arrays, generic class
  round trips, no-payload variants, and a dynamic string payload after full GC.
* `class_field_initializers`: generic queue capacity/wrap/out behavior,
  declaration-order initializers visible in the constructor body, and distinct
  per-instance arrays.
* `out_ref_arguments`: ordinary and forwarded out calls, ref read/write,
  two arguments targeting one local, stack growth and full GC during a call,
  generic string output, and field/element targets.
* `callable_field`: escaping shared captures called through a field and an
  array, including after full GC.
* `binary_plugin_binding`: an executable descriptor plugin discovered from
  project/native in a fresh process, with repeated calls checking the result.

The existing readonly aggregate AOT/interpreter test caught a compatibility
regression during development. The final implementation retains its borrowed
storage contract, and that test passes in all three configurations.

## Validation

The three existing build trees were rebuilt at the final engine revision.

| Gate | Release | ASan/UBSan | Forced full GC + ASan/UBSan |
| --- | --- | --- | --- |
| Core CTest suite | 499/499 | 499/499 | 112/112 selected |
| All `swarm_*` CTests | 12/12 | 12/12 | 12/12 |
| Required demo cases, both modes | 38/38 | 38/38 | 38/38 |
| All demo cases, both modes | 45/62 | 45/62 | 45/62 |

The Release core run took 72.85 s; the ASan/UBSan run took 986.49 s with no
detector findings. The Release performance-report gate also passes
**1/1** for `c,zr_interp,zr_binary` (196.40 s). The forced-GC selection took
28.76 s. These gates do not claim a new performance comparison.

The forced-GC selection retains the existing 100 tests from
`build/p01-gcstress-tests.txt` and adds every `swarm_*` test. It does not claim a
complete forced-GC language-suite run. The detector settings remain
`ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1`; the existing
forced-GC tree additionally has `-DZR_DEBUG_GARBAGE_COLLECT_MEM_TEST`.

The 62 demo checks cover 31 cases, including P01 variants, P08 host cleanup,
P14's two required search paths, P16 overflow observations, and P21's embedded
host checks. Native fixtures and the host were compiled against the current
engine headers/libraries for each configuration.

P22's demo integration follows this task's required-probe gate: successful
interpreter/source-removed binary parity is required for the required cases in
P01–P20; optional parity failures remain listed separately. Matching failures
are not counted as successful feature execution.

## Deferred optional findings

These original optional failures have the same statuses in all three builds:

| Probe | Interpreter / binary | Diagnosis |
| --- | --- | --- |
| P02 | FAIL / FAIL | Struct operator evaluation reaches GET_MEMBER with an unsuitable receiver. |
| P07 | FAIL / FAIL | Custom iteration is rejected by abstract/interface member matching. |
| P11 | OK / FAIL | Binary `tryBorrow(out view)` cannot create the local Place for the pool view. |
| P17 | FAIL / FAIL | f32 cast/field lowering reaches invalid operand 209. |
| P18 | FAIL / FAIL | The declared `trace` feature is not recognized by conditional compilation. |
| P19 | FAIL / FAIL | Runtime module `zr.reflection` is unavailable. |
| P20 | FAIL / FAIL | Native inline-struct construction reaches invalid operand 209; binary linking now succeeds. |
| P14 aliases/package | FAIL / FAIL | `#gfx` metadata and `@swarmcore` import resolution remain unavailable. |

An additional exploratory case with `Array<fn(int) -> int>` and
`Array<fn(int, int) -> int>` in one module is rejected with an incompatible
callable-type diagnostic. Distinct callable signatures in multiple generic
specializations remain outside the P09 shared-capture/field/array regression.
The minimal reproducer and result are under `/tmp/zr-swarm-callable-signatures*`.

No game lane or fallback is selected by these engine changes.

## Commands and evidence

All new raw detector, forced-GC, and probe evidence is under `/tmp/zr-swarm-*`.
CTest temporary output is redirected there through local build-directory
symlinks. Probe `result.json` files retain commands, source-removal state,
execution-mode markers, and CLI hashes.

```bash
export PATH="$PWD/build/local-tools/usr/bin:$PWD/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/build/local-tools/usr/lib/x86_64-linux-gnu"
cmake --build build/r2-merged-release -j4
cmake --build build/r2-asan -j4
cmake --build build/r2-merged-gcstress -j4
ZR_VM_TEST_TIER=core ctest --test-dir build/r2-merged-release -j6 \
  -E '^performance_report$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/r2-asan -j6 -E '^performance_report$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/r2-merged-gcstress -j4 \
  --tests-from-file /tmp/zr-swarm-gcstress-tests.txt --no-tests=error --output-on-failure
ZR_VM_TEST_TIER=core ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary \
  ctest --test-dir build/r2-merged-release -R '^performance_report$' --output-on-failure
```
