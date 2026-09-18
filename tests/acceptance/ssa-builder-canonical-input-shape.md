# SSA 01.02: canonical fact array shape preflight

## RED and boundary fixtures

The independent `ssa_builder_cfg` target exercises five malformed semantic
array shapes: nonempty values or instructions without backing storage,
nonempty operand IDs without backing storage, an instruction element width
different from its declared type, and an instruction length beyond its
capacity. Before preflight was added, the first case caused an MSVC access
violation (`0xC0000005`) rather than an ExecIR diagnostic.

Each case must now return `INVALID_RANGE` before dereferencing a fact,
leaving an existing caller-owned output function unchanged. Empty optional
value/operand pools remain legal in the existing CFG tests. This checks
array storage and metadata, not the contents of every canonical fact or
pruned SSA construction.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 directly compiled the focused builder/core fixture from the
  real sources with `-fsanitize=address,undefined -fno-omit-frame-pointer`.
  The D:-backed binary printed `ssa builder CFG PASS` (exit 0), with no
  sanitizer report. The D:-backed GCC CMake target could not finish its
  `VerifyGlobs.cmake` scan on the WSL `E:` mount and was stopped; no fresh
  GCC CTest result is claimed for this slice.
- WSL Clang 14 directly compiled the same focused sources with ASan/UBSan
  from the `D:` working directory. Its D:-backed binary printed
  `ssa builder CFG PASS` (exit 0), with no sanitizer report. This is a
  standalone focused binary, not a Clang CTest or full backend matrix.

## Remaining gates

Per-instruction operand bounds, semantic terminator placement, exception-
edge result availability, OOM injection, and cross-backend parity require
separate acceptance. The unrelated user-modified SSA construction fixture
was not staged or edited.
