# SSA 01.02: fail-closed semantic control edges

## Failure and success fixtures

An independent `ssa_builder_control_edges` target supplies dynamic CFG edges
with exception, cleanup, return, suspend and resume kinds. At the original
fail-closed checkpoint the
builder silently copied the first such edge as a normal successor; the MSVC
red run failed with `builder lowered a semantic control edge as an ordinary
successor`. It now reports `UNSUPPORTED`, function token 42, source block 1,
highest then-representable edge kind and the actual kind. A throwing
block with a real constant/throw pair reports the source instruction and
source-map ID 2. Caller-owned output is unchanged on failure. A normal
dynamic edge still builds and retains its destination.

The directly interpreted Oracle does not currently implement the `INVOKE`
exceptional transfer. Cleanup branches gained a bounded representation in the
2026-09-19 follow-up below; resume and multi-way cleanup dispatch still have no
distinct ExecIR control operation. Typed edges must not be silently treated as
ordinary successors. The remaining rejection is deliberately a temporary gate,
not completion of exceptional/suspension semantics.

## Inline-row follow-up

The legacy inline successor row carries no edge kind, but its block may
declare `RETURN`, `THROW`, `SUSPEND`, `CLEANUP_DISPATCH` or `EXIT` as
`terminatorKind`. With a nonempty row the builder previously accepted all
five as ordinary successors; the focused MSVC red run failed with
`builder silently published a typed control edge in an inline row`. They
now fail with `UNSUPPORTED`, the source block, declared terminator kind,
and final semantic instruction ID if present. A typed terminator with no
successors is not rejected by this particular guard. The adjacent MSVC
SSA CTest selection passed 10/10 after the fix.

WSL was not available for this follow-up: both the GCC compile attempt and
the subsequent `wsl.exe -e bash -lc 'printf ...'` startup probe failed at
`Wsl/Service/CreateInstance/CreateVm/0x800705b4` before invoking a Linux
compiler. The GCC/Clang successes in the next section refer to the prior
dynamic-edge revision, **not** to this new inline-row guard. WSL later
recovered: GCC 11.4 recompiled the inline-row fixture with
`-fsanitize=address,undefined` and printed `ssa builder control edges PASS`
(exit 0, no sanitizer report); Clang 14 recompiled without sanitizers and
printed the same PASS (exit 0). Full CTest on Linux remains outstanding.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): adjacent SSA CTest selection passed
  10/10 after the fix, including the independent builder CFG and dominance
  targets.
- WSL GCC 11.4 compiled the focused real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed fixture printed
  `ssa builder control edges PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 compiled the same sources without sanitizers; its D:-backed
  fixture printed `ssa builder control edges PASS` (exit 0). Full CTest on
  GCC/Clang and Clang sanitizer coverage are not claimed.

## Remaining gates

Executable handler payload projection, pending-state cleanup dispatch, suspend
resume maps, and source-language `try/finally` parity are still outstanding.
The unrelated dirty `test_ssa_construction.c` was not edited.

## Typed call exception-edge follow-up (2026-09-18)

The prior fail-closed guard rejected even a source-located typed call with
explicit normal/exception edges; the new test first exited 1 on MSVC with
`FAIL: typed call normal/exception edges were not preserved as INVOKE`.
The builder now lowers exactly a final `CALL_TYPED`/virtual/dynamic/meta call
with distinct, ordered normal and exception destinations to `INVOKE`, marks
the handler block exceptional, and retains both successor occurrences. A
handler reading the call result fails the core SSA verifier with
`EXCEPTION_EDGE` on its own return instruction; reversed edge kinds still
fail with `UNSUPPORTED` at the call site, preserving caller output. Bare
throw/cleanup/suspend/return/resume edges still fail closed.

MSVC 19.44 rebuilt four adjacent builder targets and
`ctest --test-dir D:/zr-ssa-verify-871bc234 -R
'^ssa_builder_(control_edges|cfg|dominance|fact_identity)$'
--output-on-failure --no-tests=error` reported 4/4 passed. WSL GCC 11.4
built the focused builder fixture with ASan/UBSan and its standalone
`/mnt/d/zr-ssa-verify-871bc234/ssa_builder_control_gcc_asan` exited 0 with
`ssa builder control edges PASS`, without a sanitizer report. This confirms
a synthetic canonical CFG fixture, **not**
source-level exception-CFG production, effect-token construction, Oracle
exception transfer, or four-backend acceptance.

## Earlier throwing operation follow-up (2026-09-18)

An additional RED case placed a second `CALL_TYPED` earlier in the same
source block; it exited 1 with `FAIL: earlier throwing call used the last
call's exception edge without a split`. The builder now rejects any earlier
operation whose ExecIR schema declares may-throw or may-suspend, reporting
`UNSUPPORTED` at that operation's source ID and preserving existing output.
The producer must split at each throwing site. This guard is limited to
known schema effects; it does not establish complete canonical effect facts.
After this guard, the same MSVC builder CTest selection passed 4/4 and the
focused WSL GCC ASan/UBSan executable printed
`ssa builder control edges PASS` (exit 0, no sanitizer report).

## Typed-call block splitting follow-up (2026-09-19)

The earlier fail-closed result is superseded for call-shaped operations. A RED
fixture changed the first operation in a two-call block to `CALL_TYPED` and
failed with `FAIL: throwing calls were not split into ordered INVOKE blocks`.
The builder now transactionally normalizes that canonical block before SSA:
the first call terminates a new block with normal continuation to the second
call and the original exception destination; the final call retains the
original normal/exception destinations. All old block targets are remapped,
predecessors are rebuilt, and both call results keep normal-edge-only
availability. Non-call may-throw/may-suspend operations still fail at their
own source ID because the current `INVOKE` schema cannot encode them safely.

Observed debug evidence on 2026-09-19:

- MSVC 19.44.35228 passed the adjacent builder/value selection 7/7:
  `ssa_builder_cfg`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, `ssa_builder_fact_identity`,
  `ssa_place_eligibility`, `ssa_place_promotion`, and
  `ssa_value_validation`.
- WSL GCC 11.4.0 and WSL Clang 14.0.0 each rebuilt and passed
  `ssa_builder_control_edges` through CTest. The existing missing-braces
  warnings in `exec_ir_build.c` were unchanged; the new normalizer emitted no
  compiler warnings.

This covers synthetic canonical typed-call splitting. Source-language
try/finally production, cleanup dispatch, optional-call short circuit,
effect-token construction, Oracle exception transfer, and the full 01.02 gate
remain open.

## Cleanup branch foundation follow-up (2026-09-19)

A focused RED fixture built `entry -> cleanup -> continuation` with two explicit
`ZR_PARSER_CFG_EDGE_CLEANUP` rows. MSVC first failed with
`builder did not preserve the cleanup branch region`, confirming the old
fail-closed guard rejected the first edge. The builder now accepts only a
cleanup edge that is the sole successor of an operand-free semantic `BRANCH`
and has a cleanup block at either endpoint. It preserves the cleanup block flag,
both successor rows, both predecessor rows, and passes the resulting function
through structural plus SSA verification. A paired negative fixture changes the
only cleanup block into a statement block and receives source-located
`UNSUPPORTED` at block 1/instruction 1 without publishing output. A second
negative fixture changes the cleanup block terminator to `CLEANUP_DISPATCH` and
is rejected at block 2/instruction 2 because no pending-control selector exists.

The control-edge validation and typed-invoke predicate moved from the 1086-line
`exec_ir_build.c` into the dedicated 329-line
`exec_ir_build_control_edges.c`; the main builder is now 888 lines. The new
module is compiled by every focused builder target. MSVC 19.44.35228, WSL GCC
11.4.0, and WSL Clang 14.0.0 each rebuilt and passed the same seven-test
selection: builder CFG, dominance, control edges, fact identity, iterator
invokes, Place eligibility, and Place promotion. GCC and Clang retained only
the pre-existing missing-braces warnings in `exec_ir_build.c`; the new module
emitted none. A fresh GCC ASan+UBSan build at
`/home/hejiahui/codex-validation/zr-vm-ssa-cleanup-asan-phase74` also passed the
focused control-edge test with leak detection and halt-on-error enabled. The
production parser target compiled the new module as MSVC static and GCC/Clang
shared libraries. The final adjacent SSA selection passed 11/11 on all three
toolchains: core model, effects verifier, dominators, builder CFG/control
edges/iterator invokes, Place eligibility/promotion, value validation, Oracle
projections, and scalar pass manager.

This is a representation-layer foundation, not source `try/finally` completion.
`CLEANUP_DISPATCH`, pending return/throw/break/continue state, cleanup effects,
source production, Oracle/ExecBC/AOT execution, and the plan's interrupted
assignment case remain open.

## Explicit cleanup dispatch follow-up (2026-09-19)

The builder now admits the previously rejected multi-successor cleanup shape
only when the cleanup block ends in a SemanticIR `SWITCH` that consumes exactly
one SSA selector. Its dynamic successors must be one or more ordered
`SWITCH_CASE` entries followed by one terminal `SWITCH_DEFAULT`. Lowering
preserves the selector as the ExecIR `SWITCH` operand, both successor order and
predecessor multiplicity, and the cleanup block flag; core structural plus SSA
verification then proves selector availability and dominance.

The focused RED first failed with
`builder did not preserve explicit cleanup dispatch successors`. Paired
negative fixtures reject a missing selector and a default-before-case edge list
with source-located `UNSUPPORTED` diagnostics and no published output. The old
operand-free cleanup branch, cleanup-outside-region, selector-free dispatch,
and inline typed-control rejection fixtures remain in the original control-edge
test. The dispatch fixtures moved into
`tests/parser/test_ssa_builder_cleanup_dispatch.c` so the general control-edge
test stays below the repository's large-file threshold.

This is an IR representation contract only. The source compiler does not yet
create pending normal/return/throw/break/continue discriminators or abrupt
payload state, exceptional entry into `finally`, cleanup effect tokens, or the
interrupted-assignment fixture.

MSVC 19.44.35228, WSL GCC 11.4.0, and WSL Clang 14.0.0 each rebuilt and passed
the focused control-edge plus cleanup-dispatch tests. The adjacent SSA matrix
passed 13/13 on all three toolchains. A fresh WSL-native GCC ASan+UBSan build at
`/home/hejiahui/codex-validation/zr-vm-ssa-cleanup-dispatch-gcc-asan-phase76`
passed the dispatch test five consecutive times with leak detection and
halt-on-error enabled. Clang 14 ASan was excluded from the success claim after
intermittent, report-free startup segfaults reproduced in both this binary and
the previously passing phase-75 source-cleanup binary. Wiki validation passed
for 116 Markdown files, 115 manifest pages, and 644 local links; its unit tests
passed 5/5.
