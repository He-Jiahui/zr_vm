# Shared AOTIR contract

`zr_vm_core/include/zr_vm_core/aot_ir.h` defines the shared ABI projection
consumed by C and LLVM AOT backends.  AOTIR is not a second language IR: its
opcode, effect, CFG, state-map, signature, and frame identities originate in
ExecIR.  Backend-specific code may legalize alignment and calling convention,
but may not infer ownership, calls, or exceptions from quickened bytecode.
The target contract requires a nonzero target-triple hash and ABI hash in
addition to the supported ABI version, pointer size, and endianness; its
required capability mask may contain only known execution capabilities.

The public records contain pointers only as in-memory views over caller-owned
arrays.  Semantic references are numeric IDs and bounded ranges, so the
canonical `ZrCore_AotIr_HashModule` ignores host addresses and is stable for
identical input.  `ZrCore_AotIr_ValidateModule` checks schema/execution
contract versions, target ABI, IDs, ranges, opcode bounds, CFG terminator and
edge-target membership, state-map instruction membership, effect pairing, frame
layout, phi predecessor membership, nonzero state-map resume identity, and
module/function hash identity. Effect tokens must be present together or both
be absent. A
non-PHI instruction carrying phi incoming rows is rejected.
Block flags are likewise limited to the canonical entry/cold/cleanup/exception
bits. Frame byte size must cover the return area and be a multiple of the
declared frame alignment.

Block predecessor and successor ranges are views over the same numeric edge
pool; their bounds use that pool's count, so parallel edges remain representable
without treating the block count as an edge-capacity limit.

`ZrCore_AotIr_IsRelocationFree` rejects module or function relocation rows.
Unimplemented operation families should be reported by a lowering diagnostic;
they must not silently fall back to semantic decoding of `SZrInstruction`.

The focused fixture is
`tests/parser/test_ssa_aotir_contract.c`.  It exercises deterministic hashing,
contract validation (including state-map instruction references), and
relocation rejection.  CMake registers it as `ssa_aotir_contract`; run it with:

```text
ctest --test-dir build/ssa-gcc-debug -R '^ssa_aotir_contract$' \
  --output-on-failure --no-tests=error
```
