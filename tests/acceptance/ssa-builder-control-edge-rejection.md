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
dynamic-edge revision, **not** to this new inline-row guard; cross-compiler
revalidation remains outstanding.

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
