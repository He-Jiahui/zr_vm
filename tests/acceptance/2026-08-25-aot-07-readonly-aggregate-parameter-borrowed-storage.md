# 2026-08-25 AOT 07 Readonly Aggregate Parameter Borrowed Storage

## Scope

This A7.2P sub-milestone projects source-declared inline aggregate `in`, `ref readonly`, and
`scoped ref readonly` parameters into borrowed callee frame storage. It also isolates ordinary free-call windows and
cross-checks canonical parameter role/type identity against physical frame storage before code stripping.

## Baseline And RED

The TDD RED source was frozen at committed `HEAD=6dea202` plus the exact A7.2P implementation/test overlay. Final GCC
composition validation included committed semantic changes through `6a0e8a9`; commits through `2b135b2` did not touch
the controlled A7.2P files. The expanded
focused RED reported 27 tests with two failures: a readonly aggregate call reused a staging slot later observed as a
scalar VALUE argument, and a readonly aggregate parameter physically downgraded to VALUE still passed ExecIR frame
validation. Review-strengthened RED also reported 27/2: identical readonly physical signatures still allocated slots
18 and 27, and a partial frame table could omit the canonical readonly row. The unreachable malformed-owner writer
test already demonstrated that validation occurred before trim. Final review added a mixed `in int + in Snapshot`
RED: the second identical call used scalar slot 39 instead of 33 because the scalar `in` parameter had acquired an
aggregate marker. Requiring both readonly source form and resolved inline TypeLayout closed that 27/1 failure.

## Test Inventory

- Covers source `in`, `ref readonly`, and `scoped ref readonly` aggregate parameters plus a readonly aggregate at
  nonzero parameter slot 1.
- Executes readonly -> scalar -> readonly one-argument calls and proves the readonly and scalar call windows have
  different physical slots and `INLINE_STRUCT` versus VALUE layouts; matching readonly signatures reuse one inactive
  window while nested active windows remain unavailable.
- Executes `inspectOffset(identity(1), snapshot)` and proves the nested scalar argument remains VALUE in a slot distinct
  from the outer readonly aggregate argument.
- Repeats `inspectOffset(prefix: in int, value: in Snapshot)` and proves the `in int` prefix remains VALUE while both
  scalar and aggregate argument slots are reused by the exact mixed signature.
- Executes `inspectIn(init Snapshot(2))` and proves the struct constructor receiver uses a fresh high-water slot distinct
  from the outer readonly argument. Before the fix this case produced three focused failures because the constructor's
  implicit `targetSlot - 1` callable overwrote the outer callable.
- Requires callee `ALIAS | INDIRECT_ALIAS | BORROWED_ALIAS` frame rows, valid TypeLayout identity, interpreter result
  47, private Unix ExecIR frame/module builds, and nonempty `.zro`, generated C, and generated LLVM artifacts.
- Rejects missing borrowed flags, physical VALUE downgrade, and known VALUE role carrying borrowed storage.
- Rejects zero/partial frame tables that omit a canonical readonly aggregate parameter row or disguise it as a
  non-parameter VALUE row; a private helper contract proves non-window slots cannot acquire the readonly marker.
- Mutates the unreachable `inspectUnused(in Snapshot)` owner and requires both C and LLVM writers with stripping
  enabled to fail before creating an output artifact.
- Guards tests that call private parser symbols on Windows while retaining public physical-frame, writer, interpreter,
  and artifact coverage there.

## Tooling Evidence

- WSL frozen source: `/home/hejiahui/codex-validation/zr_vm-a72p-ebb738a-red-r1`
- Windows isolated source: `E:\codex-validation\zr_vm-a72p-msvc-ea7684a-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228 x64 Debug
- The Windows tree contains baseline `ea7684a` plus only the A7.2P hunks in the concurrently edited metadata file;
  the seven controlled files had no committed baseline difference between `ea7684a` and `6dea202`.
- Final independent review: no Critical, Important, or Moderate findings; direct struct-init callable isolation,
  mixed scalar/aggregate markers, window reuse, and reverse pre-strip validation are closed.

## Results

- WSL GCC passes generic typed-call 27/0, compiler integration 127/0, SemIR 13/0, AOT C stripping 37/0, and AOT LLVM
  stripping 3/0. Receiver boundary reports the same three established failures in 28 tests and no new failure.
- Windows MSVC passes generic typed-call with 27 total, zero failures, and six expected private/Unix-only ignores;
  SemIR passes 12/0, AOT C stripping 37/0, and AOT LLVM stripping 3/0.
- Clang's final pass compiles the changed call compiler and focused-test objects with only two pre-existing unused-static
  warnings in the production file; earlier validation compiled the remaining changed objects. The final executable link
  hits the repository's known static archive/C11 inline unresolved `ZrCore_*` references, so no Clang runtime-pass claim
  is made.
- The new fixture's interpreter path and artifact writers run, but generated C/LLVM execution is not claimed in this
  slice.

## Acceptance Decision

Accepted at `2026-08-26 00:47:37 +08:00` as AOT 07 A7.2P readonly aggregate parameter borrowed storage and the corresponding
AOT 12 pre-strip owner-validation slice. Tail/receiver/member/imported/spread callsites, original caller Place and
provenance, writable ref/out and writeback, aggregate return/destination, spill/address-taken slots, GC/debug maps,
full A7.2, and the broader AOT 07-12 goal remain active.
