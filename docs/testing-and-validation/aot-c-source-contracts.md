---
related_code:
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/aot_source_contract_match.h
  - tests/cmake/aot-contract-targets.cmake
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_legacy_int_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_conversion.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c
  - zr_vm_core/include/zr_vm_core/integer.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_binary.c
  - tests/core/test_execution_integer_boundaries.c
implementation_files:
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/aot_source_contract_match.h
plan_sources:
  - "user: 2026-10-16 Continue M1 Windows independent validation with Linux unpassed"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/parser/test_aot_c_source_contracts.c
  - tests/core/test_execution_integer_boundaries.c
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: testing-guide
---

# AOT C Source Contracts

The 26-case source suite checks lowering ownership, frame cleanup, helper
boundaries and forbidden obsolete paths. It reads the actual checkout through
`__FILE__`; required fragments tolerate whitespace differences, while forbidden
fragments use literal matching. These checks do not execute generated C and
cannot substitute for generated-product or runtime tests.

## Current Boundaries

The four numeric/conversion/logical module checks require real function
definitions, not a source file naming itself in a comment. Reading the exact
registered path already establishes module identity. All operation, conversion,
scalar synchronization and forbidden-helper assertions remain in place.

Signed-integer needles require checked `ZrCore_Integer_*` helper forms in
legacy INT, typed/scalar and signed load-fusion paths, including checked
add-then-modulo composition. Relevant obsolete raw signed expressions and
scalar format strings are forbidden in their respective modules. Unsigned and
floating-point expressions, instruction mappings, materialization, zero guards
and scalar synchronization assertions remain. Historical case names containing
"direct C expressions" do not imply unchecked or state-free signed arithmetic.

Instruction publication is required inside the non-null, in-bounds, nonzero
`callBindingInstructionMap` branch before known-call preparation. A blanket ban
on `backend_aot_write_c_begin_instruction` is obsolete for this branch. Ordinary
typed-scalar generated products still need their own no-observation checks;
this source suite does not grant those products runtime coverage.

## Evidence

Windows r10 ran the original suite: 26 cases, five failures. Four stopped at
self-filename assertions; the fifth rejected the existing known-call boundary
(introduced before M1). The focused contract update leaves production code and
the 26-case registration unchanged. Focused MSVC r1 then built normally
(PID13112, exit0) and ran all 26 original cases (PID10704, exit0, no failures
or ignores). Source4789, HEAD, six support/tool pins and the tested binary
matched before and after execution. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-aot-contract-repair-r1-receipt.json`
has SHA-256 `5A4D871670ABC1E07A90B5F97EE0A2BA502339768F31CF50945010591E326D2A`.
Independent source/inverse review found no P1/P2 issue. R10's full-matrix failure
and source drift remain failures; Linux and M1 acceptance remain open.

### Focused R35 Checked-Integer Contracts (2026-10-17)

The test-only repair in `tests/parser/test_aot_c_source_contracts.c` (SHA-256
`B0EAFB21F4BBE324D309EB329CA10158830212DCFE51E8E0B99EDDED2F0805E0`)
updates required checked-helper forms and forbids the obsolete unsafe signed
forms described above. Production code and the original 26-case registration
are unchanged. R35 ran `zr_vm_aot_c_source_contracts_test`: 26 PASS, 0 FAIL,
0 IGNORE; the separate `zr_vm_execution_integer_boundaries_test` ran its
original three cases: 3 PASS, 0 FAIL, 0 IGNORE. Both normal footers matched and
both native exits were 0; all seven direct children terminated naturally.

Strict closure passed: source4867 identity
`B5372281DD9B6D26C3171B43BC371D6AC53B5AC7D7B14BDA420ED2246F9D1BB4`, HEAD
`f3b7d3b1db3b032a74778e40da7b84b1fd7812e3`, registered support pins, cache
and both tested binaries remained stable. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-aot-integer-contract-green-r35-receipt.json`
has SHA-256 `777B929F60917350D7074148681444FAD2619C4B95C5C5911E95CB954C7BC800`.

This closes only the focused source-contract and original core-boundary gate.
The core cases do not execute generated AOT products. Full Windows, Linux and
M1 acceptance remain open. R31 selected19 documentation validation covers its
then-frozen inputs only; this revision requires fresh documentation validation.
