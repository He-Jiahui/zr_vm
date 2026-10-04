---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_loops.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm_scalar.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h
  - tests/parser/test_ssa_licm_scalar_legality.c
  - tests/parser/test_ssa_licm_scalar_context.inc
  - tests/parser/test_ssa_loops_specialization.c
  - tests/cmake/ssa-licm-scalar-legality.cmake
  - tests/cmake/ssa-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_loops.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm_scalar.h
plan_sources:
  - .codex/plans/20261004-ssa-licm-legality.md
  - docs/plans/ssa/02-automatic-optimization/05-loops-specialization.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_licm_scalar_legality.c
  - tests/parser/test_ssa_licm_scalar_context.inc
  - tests/parser/test_ssa_loops_specialization.c
  - tests/acceptance/ssa-licm-scalar-legality.md
doc_type: module-detail
---

# Scalar LICM legality and explicit constant payloads

## Constant representation

`CONSTANT.layoutId` identifies a pool entry when the Core Oracle has a pool. A
slot numbered 1 may contain signed 2; treating that slot number as numeric 1
changes `9 * 2` into `9`. Root recorded this actual old-API counterexample: the
baseline returned 18 and the optimized function returned 9. Neither metadata
type tokens nor a function's pool slot numbers declare a scalar representation.

`OptimizeLoopsWithContextEx` and `StrengthReduceWithContextEx` take a read-only
`SZrExecIrOracleInput` view. The pass reads only `function`, `constants`, and
`constantCount`. The function pointer must identify the exact function being
optimized. Initial values, callbacks, user data, and runtime execution are never
used during optimization. The caller must supply the same semantic constants
that execution will use and keep the view valid throughout the pass.

The private scalar helper resolves CONSTANT definitions and bounded COPY chains.
Both operands must resolve to explicit SIGNED values or both to explicit
UNSIGNED values. Undefined, bool, float, absent pools, out-of-range slots, cyclic
copies, and mixed kinds supply no integer proof. Even the base of an identity
must have a known constant kind; a numeric-looking type token is insufficient.

## Checked arithmetic and identities

ADD, SUB, MUL and DIV must be independently nontrapping before speculative
movement. Signed bounds checks avoid evaluating host signed overflow; unsigned
bounds checks prevent overflow and subtraction underflow. DIV requires a nonzero
divisor and rejects signed `INT64_MIN / -1`. The arithmetic rules follow the
Core Oracle's checked 64-bit integer semantics.

Strength reduction accepts same-kind integer `x * 1`, `x + 0`, `x - 0` and
`0 + x` when the actual pool payloads prove the identity. The replacement COPY
retains the explicit integer kind. Floating identities are rejected, including
negative zero and integer/float mixes that could alter representation.

CONVERT remains in its original block. No conversion proof is supplied by this
API; the Oracle can reject NaN and out-of-range float-to-integer conversion.
The legacy APIs delegate with no context. They still hoist pure CONSTANT/COPY
instructions but do not infer arithmetic safety or identities from slot numbers.

## DIV effect chains

DIV's schema requires MAY_THROW and an effect chain even when these particular
operands prove it cannot trap. Its relocation therefore retains MAY_THROW and
rebuilds the effect chains with `SynthesizeCfgEffects`. Full Core verification
runs before the DIV move and after reconstruction.

This path is restricted to a whitelist of scalar arithmetic, comparison,
CONSTANT/COPY/CONVERT, branches and returns. It rejects memory tokens and memory
phis, calls and other observable instructions, non-DIV effects, additional flags,
deopt bindings, logical state maps, and GC maps. Existing effect-phi ranges are
retired from the shared incoming pool before synthesis, and value-phi incoming
starts are remapped while preserving their rows. Merely clearing effect ranges
would expose stale effect tokens as ordinary SSA values to structural
verification. Overlapping effect/value incoming ranges and nonempty instruction
phi ranges exclude the CFG from this narrow path. This restriction avoids
changing mixed call/memory/effect ordering or recovery-state semantics. An
allocation failure during reconstruction is reported through the pass failure
contract. DIV movement, pool compaction, synthesis, and full verification run on
a deep `CloneFunction` staging copy. The original graph is replaced only after
staging succeeds, keeping its address stable for the semantic context. A failed
DIV step leaves the original graph valid and unchanged by that step. Earlier
accepted pure moves retain the existing mutating-pass contract. Allocation
failure injection has not been executed; this is a source-level staging guarantee.

Generic hoisting also rejects any function with a logical state-map pointer, GC
map pointer, or nonzero GC-map count. Pure CONSTANT/COPY movement changes later
instruction IDs too; existing source-map/binding-row remapping does not update
all logical checkpoints and GC map sites. These unsupported borrowed maps and
the associated instruction order are preserved. Complete map-aware hoisting
requires owned/COW map updates plus freshness/liveness checks and remains open.

All existing loop, SSA, preheader, source-map and use-order gates remain. The
loop analysis's immediate interpretation of header constants is separate work:
this repair never relies on that interpretation as proof that trapping
arithmetic may move. The regression uses nonzero pool slot 2 containing false
to check actual zero-trip behavior.

## Validation boundary

The focused executable first checks the old function-only pooled-2 case and then
executes 29 semantic cases before and after optimization. It checks returned
value kind and payload bits, event kind/source/operand order, arithmetic error
codes and source/block locations, actual instruction placement, strength counts,
and full Core verification. Cases cover positive signed/unsigned identities,
safe integer ADD/SUB/MUL/DIV, absent or mismatched contexts, pooled nonidentities,
division traps and overflows, zero-trip traps, floating negative zero, mixed
kinds, undefined values, and conversion failure domains. Semantic failures
explicitly return failure rather than depending on assertion abort codes.
The two added mapped cases borrow stack-owned logical/GC maps and assert no
instruction/block/map/site mutation, with actual Oracle equivalence. The logical
checkpoint is derived from Core ownership/liveness analysis and validated before
and after the pass. Stack side tables are detached before freeing the function.

The existing loops suite keeps its positive strength fixture with explicit
signed pooled constants; its CONSTANT hoist fixtures are unchanged. The isolated
CMake fragment reuses that suite's support sources and adds Core Oracle sources.
Root added the central fragment inclusion. Broader CMake configuration and the
repository-wide build remain outside this finite gate. The actual V31 direct
CMake subproject configured the owned registration fragments, compiled their
current checkout sources, and ran both named CTest targets successfully. The
existing loops executable also passed the earlier V20 regression below.

Root's first fresh build passed compilation and the first four semantic-context
cases, then full verification rejected safe DIV with INVALID_VALUE (expected
valueCount 6, actual incoming 0). Source inspection identified stale effect-phi
rows losing their effect classification. The incoming-pool compaction repair
was subsequently validated by the V16 run described below.
Root's next run passed the DIV staging/compaction positives and both mapped
negatives, with 28 of 29 semantic cases passing. The undefined-base fixture had
incorrectly expected ARITHMETIC_ERROR: Core's operand collection rejects
UNDEFINED with INVALID_VALUE before arithmetic evaluation. Its named expected
diagnostic now matches that primary-source behavior, retaining exact before/after
code, source/block, event, no-transform, and outcome checks. Production was
unchanged by this fixture repair.

### Historical focused acceptance: V16

Root's V16 fresh fixture compilation, link, actual runtime, and repeat runtime
all exited 0. All 29 semantic-context cases passed, along with the separate
legacy pooled-2 equality regression (baseline 18, optimized 18, reduced count
0). Both runtime logs are identical with SHA256
`868cbfee1460996ae97807e1f0c00b8251e9d1f14023443e1deee34d3228e397`.
The authoritative receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-licm-green-v16/Root-receipt.json`,
SHA256 `e5da76c7767d80a9d628dcd74de8729610e6c63306ddf14248745c5fe5a395ab`.
Its total elapsed time is 6.1750171 seconds.

This Windows clang-cl gate instruments the fresh fixture and five individually
accepted current Parser objects with UBSan, linking 19 plain Core objects.
Root confirmed current actual dependency files, source/tool/resource identities,
and compiled/linked products before and after execution. This is not whole-Core
UBSan or ASan coverage, Linux validation, allocation-failure injection, full
02.05 acceptance, recurrence optimization, or map-aware hoisting acceptance.
The changed existing loops fixture also passed V20. V31 subsequently verified
the finite direct CMake configuration and registration described below. See the focused
[acceptance record](../../tests/acceptance/ssa-licm-scalar-legality.md).

### Historical existing loops/profile regression: V20

Root V20 compiled the current existing loops fixture and profile source with
UBSan, reused five individually accepted current UBSan Parser objects and 19
plain Core objects, linked, and ran the eight named assertion tests. All steps
exited naturally with code 0; the runtime log is empty and contains no sanitizer
diagnostics. The positive context strength fixture, pure CONSTANT hoist, typed
binding-row remapping, zero-trip trapping block, loop forest/trip analysis,
profile key/hash tests, and specialization budget/cooldown all passed.

Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-loops-regression-v20/Root-receipt.json`,
SHA256 `0c1f6d6d40cad060c0f141c9c8fa285be292103712d59e3d81494f7517add157`,
137833 bytes; whole elapsed 6.7047965 seconds. The independent read-only audit
`licm-owned-loops-v20-audit.json` in the same report task directory has SHA256
`cda0c16caaa231ebecbf2a705029bf8995adcdb77b6198df0bd0921a5dbc2c66`.
It matched all 590 pinned records across 315 current files, parsed all 26 actual
dependency files and their 232 unique dependencies, checked the source's eight
main test calls and enabled assertions, and confirmed natural/reaped/empty/no-
intervention lifecycle evidence. V19 stopped on an incorrect controller report
path before any compilation; it was not a fixture/runtime failure.

### Current direct CMake acceptance: V31

The direct subproject at `tests/cmake/ssa-direct-validation/CMakeLists.txt`
configured nine finite SSA targets against the actual checkout. Every source
and header was read directly from `E:/Git/zr_vm`; no source snapshot was used.
Native Clang plus LLD freshly compiled all 138 translation-unit edges with
UBSan, nonrecovering sanitizer errors, frame pointers, and `/UNDEBUG`. The LICM
target contains 26 translation units and the existing loops target contains 21;
their Core support objects were freshly instrumented too. This supersedes the
earlier V16/V20 limitation of linking plain Core support for these finite targets.

All nine named CTest targets passed, including `ssa_licm_scalar_legality` and
`ssa_loops_specialization`. LICM retained the separate pooled-value regression
(`baseline=18`, `optimized=18`, `reduced=0`) and all 29 semantic-context cases;
the existing loops suite retained its eight tests. The CTest log reports zero
failed tests and no sanitizer diagnostic. Its real test time was 31.72 seconds;
the complete recorded configure/build/test/evidence workflow took
525.470823399999 seconds.

The authoritative Root receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31/Root-receipt.json`,
258995 bytes, SHA256
`17c71e728b1d84f679bfa7d2d60f2f46793a3c76ab6430782509d244b39e88e2`.
Its acceptance flag is true. The independent read-only audit
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-direct-cmake-v31-current-v1.json`
is 12198 bytes, SHA256
`506ad7a314b4b37af3a75af6b746e23298742f73b9079d8d9da9b1a4bb78802e`.
All 40 checks passed. They cover current file/tool/resource identities, the
actual source and header dependency graph, all compiler flags, nine UBSan
links, exact CTest names, enabled assertions or explicit failure returns, and
natural completion without process intervention. These two file hashes and
lengths were rechecked when updating this document; no native jobs were rerun.

This is acceptance of the finite direct CMake integration and scalar legality
slice. Full 02.05 and all 47 SSA leaves remain open. Whole-pass rollback,
allocation-failure injection, general induction/strength reduction, map-aware
hoisting, and compiler semantic-pool/context integration remain open; there is
currently no LICM callsite in the pass manager. DIV staging protects one DIV
step and retains earlier accepted pure moves; it does not establish whole-pass
rollback. Linux, ASan, native32, complete repository builds, real typed AOT
producer integration, and full milestone acceptance are not established by V31.
