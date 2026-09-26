# SSA 07.02: shared C/LLVM lowering parity

## Scope

`SZrAotIrEmitResult.loweringHash` exposes the target-independent shared
operation classification. C and LLVM retain distinct target contract hashes,
but must report the same source hash, lowering hash, native count, and runtime
bridge count for one validated AOTIR module. The backend adapter preserves the
same lowering hash when it converts either the shared lowering result or a
target emission result into backend facts.

## Evidence

`tests/parser/test_ssa_c_llvm_lowering.c` asserts typed scalar, control, call,
async and runtime-bridge classification, failure diagnostics for malformed CFG
and effects, policy rejection, relocation rejection, equal C/LLVM source and
lowering hashes, and intentionally different target contract hashes. Strict
GCC 11.4, Clang 14, and MSVC x64 `/W4 /WX /std:c11` builds and executions pass.
`tests/parser/test_ssa_aot_backend_adapters.c` additionally asserts that the
shared-lowering and LLVM adapter paths expose the same lowering hash.

No native source, LLVM module, executable artifact, GC root publication or
exception runtime is claimed by this descriptor-only parity slice.

## Acceptance

Accepted as the target-independent lowering gate. Full operation-family code
generation and executable C/LLVM parity remain open under 07.02.
