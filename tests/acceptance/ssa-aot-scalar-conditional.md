# Finite AOT scalar conditional acceptance

Design: `.codex/plans/20261004-aot-scalar-conditional.md`.
Implementation: `docs/instruction-generation/aotir-scalar-conditional.md`.

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

Root's pre-conditional eight-file arithmetic preservation manifest:
`E:/cargo-targets/zr_vm/tmp/ssa-20261004-01a0fe2b/aot-arithmetic-25-before-conditional/manifest.json`.
SHA256: `93ad09ef44029b525301423d79c209116f3158296ac44928f870e8ace9fabea5`.

## Prepared GREEN inputs; execution pending root

The helper adds the mandatory eighth TU to the isolated conditional target.
Expected successful fixture summary:

```text
RESULT positives=12 precondition_failures=0 feature_failures=0 output_failures=0 guards_executed=16 guard_failures=0 argument_checks=10 argument_failures=0
```

Twelve original positive names/precondition witnesses and independent expected
bits are retained. Sixteen exact rejection cases and ten emitter argument checks
are separate counters. Exact length+NUL succeeds; truncation clears reused output;
subsequent successful emission with optional diagnostic proves reuse. Successful
artifact generation writes 36 input files for 24 C/LLVM compile-and-run products.
Native parity and UBSAN require fresh root receipts before a GREEN claim.

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
writes. Root owns all subsequent compile/runtime evidence and products under
`E:/cargo-targets/zr_vm/{build,tmp,reports}/ssa-20261004-01a0fe2b`.
