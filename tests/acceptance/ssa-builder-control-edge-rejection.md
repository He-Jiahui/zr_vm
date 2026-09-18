# SSA 01.02: fail-closed semantic control edges

## Failure and success fixtures

An independent `ssa_builder_control_edges` target supplies dynamic CFG edges
with exception, cleanup, return, suspend and resume kinds. Previously the
builder silently copied the first such edge as a normal successor; the MSVC
red run failed with `builder lowered a semantic control edge as an ordinary
successor`. It now reports `UNSUPPORTED`, function token 42, source block 1,
highest currently representable edge kind and the actual kind. A throwing
block with a real constant/throw pair reports the source instruction and
source-map ID 2. Caller-owned output is unchanged on failure. A normal
dynamic edge still builds and retains its destination.

The directly interpreted Oracle does not currently implement the `INVOKE`
exceptional transfer, nor is a cleanup/resume edge lowered into a distinct
ExecIR control operation here. These edges must not be silently treated as
ordinary successors. This is deliberately a temporary rejection gate, not
completion of exceptional/suspension semantics.

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

Explicit exceptional-result availability, handler/cleanup entry, suspend
resume maps, loop phi insertion, and source-language parity are still
outstanding. The unrelated dirty `test_ssa_construction.c` was not edited.

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
