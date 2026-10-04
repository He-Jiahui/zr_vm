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
doc_type: module-detail
status: finite-emitter-and-native-parity-verified-registration-and-commit-pending
---

# Finite AOTIR i64 ADD and SUB text emission

## Entry and validation

The C and LLVM scalar APIs select the arithmetic helper after establishing an
empty output buffer and zero output length, when the module has two constants.
The existing one-constant template retains its own prepare path. The arithmetic
helper first calls `ZrCore_AotIr_RequireExecutableAbi`; successful descriptor
validation alone does not authorize emission. The callable ABI must explicitly
declare a no-argument i64 function.

The accepted function has one block with exactly CONSTANT, CONSTANT, ADD or SUB,
RETURN. A second accepted form has an entry block with one unconditional BRANCH
and the four instructions in its target block. Block ranges, terminators, entry
flags, the edge's predecessor and successor entries, and all instruction counts
must match those forms. The result and operand pools each have exactly three
entries. Constants define the first two result entries, arithmetic defines the
third, and RETURN consumes that third ValueId. The three IDs must be distinct
and nonzero; they need not be consecutive.

CONSTANT `layoutId` is a constant pool index. Each constant instruction reads
its own index, so reversed indices preserve the intended meaning. The two
indices must differ, ensuring both pool entries are used. Constant flags must be
zero, and constant and instruction type tokens must equal the explicit ABI
return token. Each arithmetic operand is resolved by its actual ValueId, in its
declared order, against the two definitions. The private plan copies IDs,
opcode and operand bit patterns and retains no module pointers.

The template rejects required capabilities, declared effects, nonzero module
flags, layout descriptors, runtime slots, phi and memory pools, GC and deopt
metadata, logical state maps, and map hashes. Every instruction must have zero
flags, effect tokens, phi/memory ranges, binding row, deopt ID and
`matchTypeToken`. Source annotations are descriptive and are not executable
metadata. Unsupported shapes return a diagnostic through the existing shared
failure helper. This finite slice does not implement general SSA lowering or
artifact registration.

## Overflow and signed formatting

The helper decodes signed bits using `-1 - (int64_t)(~bits)` for negative
patterns, avoiding implementation-defined unsigned-to-signed conversion.
For ADD it checks `b > 0 && a > INT64_MAX - b` or
`b < 0 && a < INT64_MIN - b`. For SUB it checks
`b > 0 && a < INT64_MIN + b` or `b < 0 && a > INT64_MAX + b`.
All evaluated intermediate values are representable. The helper never evaluates
the source signed arithmetic itself. Overflow returns UNSUPPORTED and identifies
the arithmetic instruction (102 in the fixture).

C source contains two `int64_t` input locals, an actual `+` or `-` expression in
a result local, and a return of that local. `INT64_MIN` has an explicit spelling;
other negative literals use unsigned magnitude and negated `INT64_C`.
A branch label is followed by an empty statement so subsequent declarations
remain valid C11. LLVM emits an actual `add i64` or `sub i64` instruction and
returns its SSA result. It emits neither `nsw` nor `nuw` flags. Decimal negative
LLVM operands also use unsigned magnitude. Emission does not fold the result
into a returned literal.

## Output and tests

Successful output length excludes NUL. Capacity equal to length plus one must
succeed; capacity equal to length must return INVALID_RANGE, clear the output,
and leave length zero. The tests reuse the buffers through these operations and
check overflow and malformed input failures against previously populated buffers.

The finite fixture has twelve positive cases, four signed overflow cases and
nine malformed cases. Positive modules are validated before calling either
emitter. Optional artifact output writes C, LLVM and one expected-bit runner per
case; executable parity requires compiling and executing both emitted forms.
The isolated CMake fragment includes the new helper translation unit. Current
acceptance evidence is recorded separately in the acceptance document; a source
change alone is not evidence of a passing executable test.

## Fresh finite execution evidence

Root freshly compiled the seven C translation units with Windows Clang 19.1.5,
LLD and UBSan. The fixture passed twelve positives, four overflow rejections and
nine guard rejections, reporting `RESULT positives=12 overflow=4 rejected=9 failures=0`.
It generated twelve C files, twelve LLVM IR files and twelve independent
expected-return runners. All twelve C and twelve LLVM products then compiled,
linked and executed successfully against the expected UInt64 return bits.

The emitter receipt is `llvm-aot-red-green-v10/Root-receipt.json`, SHA256
`050f7b546f58d4c8df35d024f73bfef4f4250b59e8abb71171e82a2ef1ce2044`;
the native parity receipt is `aot-native-parity-v11/Root-receipt.json`, SHA256
`8bd3a74b463b0758d402103d589f9d3434d988e1a51a9b62183a26ec890fd7b2`.
Both reside under `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b`.
An independent read-only audit verified 1,295 pin records across 475 unique paths,
31 actual dependency files, all 24 AMD64 PE parity executables and their empty
runtime logs. Its report is `independent-aot-scalar-current-v1.json`, SHA256
`a94da734db904453ce97d92dab0469b69e602abf37653c1ba0626544e84cf7f2`.

UBSan covers the emitter/fixture C code, generated C functions and C runners.
The emitted LLVM IR was compiled and executed natively; this does not claim
frontend UBSan instrumentation of that IR. Removing only the new helper include
and arithmetic dispatch from each emitter reproduces its exact preimage hash,
confirming preservation of the existing bytes and foreign comments.

The isolated fragment contains the helper translation unit. At this audit the
parent test file did not yet include that fragment; parent registration and
production source glob integration require their own checks. This finite result
does not close full SSA 07.02 or all 47 leaves. Root's attempted authorized index
lock removal still returned AccessDenied despite the user's writable-config
change; the lock and index remain preserved, and commit awaits effective write
permission rather than user approval. Full evidence is in the acceptance document.