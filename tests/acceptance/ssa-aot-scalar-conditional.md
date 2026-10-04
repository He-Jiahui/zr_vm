# Finite AOT scalar conditional acceptance

Design: `.codex/plans/20261004-aot-scalar-conditional.md`.
Implementation: `docs/instruction-generation/aotir-scalar-conditional.md`.

Status 2026-10-04: finite descriptor emission and all 24 generated C/LLVM native
parity executions are accepted from current direct-workspace Root receipts.
Full 07.02 and the 47-leaf SSA milestone remain OPEN.

## Actual RED supplied by root

Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-aot-red-conditional-red-v25/Root-receipt.json`

SHA256: `ad7204eda8055eac42e013322e57281d7e7cbb9db27ccde3bc2d4845c618b175`.
Receipt size: 126176 bytes. Root reports whole-window elapsed time 4.2060909 s.

The current seven-TU UBSAN compile and link exited naturally with zero; fixture
execution exited naturally with one. All twelve positives passed ValidateModule
and RequireExecutableAbi independently. Both public emitters rejected all twelve
with status 14 (UNSUPPORTED), instruction site zero. Precondition/output failures
were zero and no guard rows ran. Job accounting was empty with no cleanup actions.
This establishes missing conditional emission rather than malformed input or a
missing symbol. It is a root-supplied actual result, not an implementation-agent
compiler execution.

The earlier pre-conditional copied-source preservation manifest is historical
only. Current acceptance uses actual workspace source/dependency pins and the
current original arithmetic fixture; removed copied inputs are not required.

## Current direct-workspace descriptor GREEN

Root receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/aot-direct-v29/Root-receipt.json`.
SHA256: `2ca49031d849d6e7327a3760f5d8fb180bb49d25801388e8ba03c40af8e11e37`.
Whole acceptance is true. Current workspace inputs were compiled with the
mandatory conditional helper TU; all project C TUs were UBSan instrumented.
The actual successful fixture summary is:

```text
RESULT positives=12 precondition_failures=0 feature_failures=0 output_failures=0 guards_executed=16 guard_failures=0 argument_checks=10 argument_failures=0
```

Twelve original positive names/precondition witnesses and independent expected
bits are retained. Sixteen exact rejection cases and ten emitter argument checks
are separate counters. Exact length+NUL succeeds; truncation clears reused output;
subsequent successful emission with optional diagnostic proves reuse. Conditional
and original arithmetic runtimes both naturally exited zero with empty job
accounting and no actions. The original arithmetic fixture summary was
`RESULT positives=12 overflow=4 rejected=9 failures=0` (25 cases total). Actual
workspace source/dependency hashes, compilation inputs, maps and products are
recorded in the receipt. This accepts descriptor emission and arithmetic
regression in the finite scope.

## Generated C/LLVM native parity

Root receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/conditional-native-parity-v30/Root-receipt.json`.
SHA256: `eb2845483eff931818cd5d6a8e22fd2317ac2db874b490d6a3631fa8cc5f47e7`;
194,213 bytes, 65.5449482 s whole window. Whole acceptance is true.

V29 generation produced twelve C sources, twelve LLVM IR sources and twelve
independent expected-bits runners. V30 compiled and ran the corresponding
24 products, with 84 accepted compile/link/runtime steps and 24 accepted parity
rows. Every runtime naturally exited zero, was reaped with empty job accounting
and required no cleanup action. Current generated-input hashes, actual compiler
dependencies and native products are pinned in the receipt; no project source
snapshot or copy was used. Generated C and the C runners were UBSan instrumented.
Emitted LLVM IR was compiled natively; it does not establish frontend UBSan
instrumentation for that IR.

Independent audit:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-aot-direct-v29-v30-current-v1.json`.
SHA256: `86e0b70de120874e4b85386efdabefd4b36a9493b5033d506aa41310664f0b34`.
Scoped acceptance is true. The audit checked 487 unique current pins twice with
no conflicts or mismatches, 13 V29 and 84 V30 natural-zero steps, 26 PE products,
45 object products and 33 actual dependency files. It independently adopts
descriptor emission, original arithmetic regression and the 24 native parity
rows. Current CMake fragments are outside the V29/V30 receipt context; that audit
does not establish formal CMake acceptance.

## Formal direct-checkout CMake gate

Root receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31/Root-receipt.json`.
SHA256: `17c71e728b1d84f679bfa7d2d60f2f46793a3c76ab6430782509d244b39e88e2`;
258,995 bytes. Whole acceptance is true. The actual source directory was
`E:/Git/zr_vm/tests/cmake/ssa-direct-validation`; project sources and headers
were read directly from `E:/Git/zr_vm`, without copied source inputs.

Configure, build, CTest, Ninja dependency inspection and Ninja command inspection
all naturally exited zero. All 138 project C compile edges used UBSan and
UNDEBUG. CTest passed exactly 9/9 configured finite tests with zero failures,
including `ssa_aot_scalar_conditional`, `ssa_aot_scalar_arithmetic` and the
original `ssa_aot_scalar_text`. The scalar-text target uses eight distinct TUs
after helper deduplication. This validates the finite local CMake configuration;
it does not establish full-repository acceptance, typed parser-producer
integration or full 07.02.

Independent audit:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-direct-cmake-v31-current-v1.json`.
SHA256: `506ad7a314b4b37af3a75af6b746e23298742f73b9079d8d9da9b1a4bb78802e`;
12,198 bytes. Scoped acceptance is true: all 40 checks passed, including current
pins, 138 valid dependency blocks, all 138 UBSan/UNDEBUG compile edges and exact
nine-test CTest agreement. This independently adopts the finite direct-checkout
CMake gate while retaining the broader acceptance limits above.

Preservation evidence from read-only byte reconstruction: removing only the new
conditional header and constantCount4 dispatch restores exact C prehook SHA
`8c773e462dbd928f7fc95c4e0b0c69c27312ce0ee23d3fa59f7a0ec5217a7d5a`
and LLVM prehook SHA
`356a040fdc1f1e7fbbddbecb9b87dd795a951cf6408194d0159701ceb3f88896`.
The existing arithmetic constantCount2 path and every foreign comment byte are
preserved. scalar_text.c, scalar_text_internal.h, scalar_arithmetic.c/.h and the
arithmetic test were not edited.

Only descriptor emission and standalone native parity are in scope. No public
bool ABI, current typed-producer integration, PHI or artifact registration is
claimed. The implementation agent executed no compiler/runtime/WSL/jobs or Git
writes. Linux, ASan and native32 are not established by these Windows runs.
Full 02.02, full 07.02 and the 47-leaf milestone remain OPEN. Root owns native
compile/runtime evidence and products under
`E:/cargo-targets/zr_vm/{build,tmp,reports}/ssa-20261004-01a0fe2b`.
