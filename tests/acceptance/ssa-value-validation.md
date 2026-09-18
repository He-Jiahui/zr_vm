# SSA 01.02: conservative operand validation slice

## Scope and observed failure

`ZrParser_ExecIr_BuildSsa` checked operand value IDs but used each instruction's
`operands.start` and `operands.count` without checking their bounds against
the function's *logical* operand pool length. The new independent fixture
placed a valid defined ID in allocated but logically unused backing storage,
then referenced that ID with an invalid instruction range. On MSVC before the
fix, `ctest --test-dir D:/zr-ssa-verify-871bc234 -R '^ssa_value_validation$'
--output-on-failure --no-tests=error` failed 0/1 with
`FAIL: SSA pass accepted operands beyond the logical side-pool end`.

## Verified cases

- Defined value and an in-range operand are accepted.
- An operand range outside the logical side pool is rejected even when
  allocated backing storage contains a valid value at that location.
- A `UINT32_MAX` range start cannot wrap to a small valid index.
- A nonempty operand pool with null backing storage is rejected without a
  dereference.
- Unknown opcode produces `UNKNOWN_OPCODE` with function token and one-based
  instruction location; per-instruction invalid ranges carry the same stable
  identities.

## Execution evidence (2026-09-17)

The MSVC standalone target `zr_vm_ssa_value_validation_test` rebuilt and
linked through the VSDevCmd wrapper in `D:/zr-ssa-verify-871bc234`. After the
fix and additional boundary cases, `ctest --test-dir
D:/zr-ssa-verify-871bc234 -R
'^ssa_(value_validation|builder_cfg|dominator_cfg|core_model|effects_verifier|pass_manager_scalar)$'
--output-on-failure --no-tests=error` reported **6/6 passed**. The existing
compiler D9025 `/W3`-overridden-by-`/W4` warning was nonfatal.

WSL `uname -r` still failed before entering Linux with host error
`HCS 0x800705aa`; this change has no claimed GCC/Clang or sanitizer result.
E: lacked free capacity for the full repository build, and pre-existing user
edits to the `ssa_construction` integration test were not staged or changed.

## Acceptance boundary

Only parser SSA input-range and diagnostic correctness are accepted here.
Dominance-aware value renaming, pruned phi insertion, exceptional definitions,
full 01.02 lowering, and four-backend parity remain open. This focused test
does not replace the core structural verifier or full SSA milestone gate.
