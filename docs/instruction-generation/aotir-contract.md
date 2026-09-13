# Shared AOTIR contract

`zr_vm_core/include/zr_vm_core/aot_ir.h` defines the shared ABI projection
consumed by C and LLVM AOT backends.  AOTIR is not a second language IR: its
opcode, effect, CFG, state-map, signature, and frame identities originate in
ExecIR.  Backend-specific code may legalize alignment and calling convention,
but may not infer ownership, calls, or exceptions from quickened bytecode.

The public records contain pointers only as in-memory views over caller-owned
arrays.  Semantic references are numeric IDs and bounded ranges, so the
canonical `ZrCore_AotIr_HashModule` ignores host addresses and is stable for
identical input.  `ZrCore_AotIr_ValidateModule` checks schema/execution
contract versions, target ABI, IDs, ranges, opcode bounds, CFG terminator
membership, effect pairing, frame layout, and module/function hash identity.

`ZrCore_AotIr_IsRelocationFree` rejects module or function relocation rows.
Unimplemented operation families should be reported by a lowering diagnostic;
they must not silently fall back to semantic decoding of `SZrInstruction`.

The focused fixture is
`tests/parser/test_ssa_aotir_contract.c`.  It exercises deterministic hashing,
contract validation, and relocation rejection.  The source is intentionally
not added to shared CMake by this slice; temporary validation can compile it
with:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
  -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include \
  zr_vm_core/src/zr_vm_core/aot_ir.c \
  tests/parser/test_ssa_aotir_contract.c
```
