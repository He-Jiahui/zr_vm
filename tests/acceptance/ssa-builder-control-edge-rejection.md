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
