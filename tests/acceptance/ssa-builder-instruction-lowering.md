# SSA 01.02: builder instruction ranges and branch edge attachment

## RED and fixture

The independent `ssa_builder_cfg` target now builds two semantic CFG blocks:
the entry defines a constant and branches to a return block, which consumes
that value. On MSVC, the first fixture incorrectly omitted the return value
and received `INVALID_RANGE` at instruction 2; that was an invalid test
input, not evidence of the builder defect. With a valid constant result and
return operand, the target failed at
`FAIL: builder shifted instruction ranges or detached branch successor`.

## Required boundary

- Block instruction ranges are `[0, 2)` and `[2, 3)` in the actual output
  pool, with one-based terminator IDs 2 and 3.
- The entry branch references the same one-edge successor range as its block.
  The emitted function passes `ZR_EXEC_IR_VERIFY_STRUCTURE` after assigning
  the module-owned function ID in the standalone fixture.
- Existing empty-block CFG tests, invalid-target diagnostics and source
  function ownership remain unchanged.

## Observed validation

- MSVC (D:-backed focused build via VSDevCmd): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 (D:-backed Debug build): `ctest -R '^ssa_builder_cfg$'`
  passed 1/1. GCC reported pre-existing `-Wmissing-braces` warnings in the
  builder's range initializers; compile and link succeeded.
- A direct WSL Clang 14 ASan/UBSan command progressed through compilation,
  but its linker remained blocked on the WSL `E:` mount (`p9_client_rpc`)
  for over two minutes and was stopped. No Clang test or sanitizer result is
  claimed for this builder slice.

## Remaining gates

This checks output-pool indexing and normal branch edge identity, not the
planned pruned phi insertion, promotable-place selection, exception-edge
definition availability, source short-circuit behavior or four-backend
semantic parity. The user-modified `test_ssa_construction.c` and aggregate
acceptance file were left untouched.
