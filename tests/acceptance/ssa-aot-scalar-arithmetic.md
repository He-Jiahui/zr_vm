---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - tests/parser/test_ssa_aot_scalar_arithmetic.c
  - tests/cmake/ssa-aot-scalar-arithmetic-tests.cmake
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
plan_sources:
  - .codex/plans/20261004-ssa-aot-scalar-arithmetic.md
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
tests:
  - tests/parser/test_ssa_aot_scalar_arithmetic.c
  - tests/acceptance/ssa-aot-scalar-arithmetic.md
doc_type: testing-guide
status: finite-emitter-and-native-parity-verified-registration-and-commit-pending
---

# SSA 07.02 finite scalar arithmetic acceptance

Scope: twelve positive two-CONSTANT ADD/SUB fixtures in both C and LLVM, four
checked overflow fixtures, nine malformed fixtures, exact output capacity and
buffer reuse. Design source: `.codex/plans/20261004-ssa-aot-scalar-arithmetic.md`.
Implementation details: `docs/instruction-generation/aotir-scalar-arithmetic.md`.

## RED evidence supplied by root

The actual six-translation-unit Clang+LLD test exited naturally with status 1
and twelve valid arithmetic positives rejected by both emitters. The compile
job was empty at receipt and required no cleanup actions.

Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-aot-red-aot-v6/Root-receipt.json`

SHA256:
`090ecc6aea3d18f3da64e7d9642b96779e58abbc5b886ed05f603e2c66102aab`

## Fresh GREEN emitter execution

Root compiled the actual seven translation units with Windows Clang 19.1.5 and
LLD, enabling UBSan with no recovery. All 25 fixture groups completed: twelve
positive emissions, four overflow rejections and nine guard rejections. The raw
log contains twelve ordered PASS lines and
`RESULT positives=12 overflow=4 rejected=9 failures=0`. Compile, link and fixture
runtime steps exited naturally with status 0, zero active/terminated owned Job
members, closed/reaped handles and no cleanup actions or errors. Whole invocation:
4.5780654 seconds.

Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-aot-red-green-v10/Root-receipt.json`.
SHA256: `050f7b546f58d4c8df35d024f73bfef4f4250b59e8abb71171e82a2ef1ce2044`.
Its recorded native-parity-pending flag describes that emitter-stage receipt;
the subsequent independent parity gate below supplies the separate evidence.

## Generated C and LLVM native parity

The fixture generated 36 inputs: twelve C functions, twelve LLVM IR functions
and twelve independent expected-return C runners. Root genuinely compiled,
linked and ran all twelve C and twelve LLVM executables. Every runner compares
the function's UInt64 return bits against its fixture's independent expected
constant; success is exit status 0. All 24 runtime logs are empty and all 84
recorded parity build/runtime steps exited naturally with status 0, empty owned
Jobs, closed/reaped handles and no actions or errors. Whole invocation:
20.1167385 seconds.

Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/aot-native-parity-v11/Root-receipt.json`.
SHA256: `8bd3a74b463b0758d402103d589f9d3434d988e1a51a9b62183a26ec890fd7b2`.

The twelve cases are add_positive, sub_order, add_negative, sub_negative,
add_mixed, sub_mixed, max_zero, min_zero, min_add_one, max_sub_one, branch_add and
branch_sub. Expected bit patterns include negative results, INT64_MIN,
INT64_MAX and adjacent representable values. Each case has both a C and LLVM
native executable. Actual compile flags establish UBSan for generated C and C
runners. LLVM IR was compiled as IR and executed natively; no frontend UBSan
instrumentation of the emitted IR is claimed. No ASan validation is claimed.

## Independent current artifact audit

A separate reviewer read the actual receipts, raw logs, generated inputs,
objects, executables, source/header/tool/resource files and expected-bit runners.
All 1,295 receipt pin records across 475 unique paths match current SHA256 and
length. The reviewer parsed all 31 actual Make dependency files (seven emitter,
twelve generated C and twelve runner files) and resolved every input against the
receipt pins. All 24 parity executables have AMD64 PE headers. All 93 recorded
emitter/parity steps satisfy natural zero exit and empty aggregate Job records.
No compiler, runtime, Job helper, network or Git command was run by this reviewer.

The two emitter hooks preserve foreign bytes. Removing only the added helper
include and conditional arithmetic dispatch reconstructs the exact prior hashes:
C `2e76f5d812940e68e548d085bfd00651cee1b33ee93ce197fae5317cb06bc4ce`;
LLVM `8645f263429e935f8b5df53958cff271e40d20f33eb0d6114c4cba1f1c4bc91a`.
This validates preservation of the existing comments and surrounding code.

Independent report:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-aot-scalar-current-v1.json`.
SHA256: `a94da734db904453ce97d92dab0469b69e602abf37653c1ba0626544e84cf7f2`.

## Integration and remaining scope

The isolated `ssa-aot-scalar-arithmetic-tests.cmake` fragment includes the new
helper translation unit. The actual parent `tests/CMakeLists.txt` did not yet
include that fragment at this audit. Parent registration and production source
glob integration remain separate checks; no genuine registered CTest or full
repository build is credited by these direct execution receipts. Full SSA 07.02
and all 47 leaves remain open. Earlier failed receipts retain their original
status; none is promoted by these fresh successful invocations.

Products remain under the task's permitted E build/tmp/report roots. The user
added `.git` write configuration and authorized the exact October 3 index-lock
cleanup. Root's actual removal still returned AccessDenied; the lock and index
remain preserved. Commit is pending effective filesystem permission, not another
user approval.