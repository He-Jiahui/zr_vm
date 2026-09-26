# SSA 07.01: AOTIR constant-pool retention

## Scope

The shared AOTIR module now carries a typed constant pool with flags and raw
bits. Projection descriptors copy the supplied constant records into their
owned module view; AOTIR validation rejects missing pools and zero type tokens,
and semantic hashing covers every constant record. Schema version 5 marks the
contract change.

This is a descriptor/schema slice. The production ExecIR module-to-projection
constant population path and C/LLVM constant consumers still need to replace
legacy opcode/pool decoding, so no native artifact is claimed.

## Evidence

`tests/parser/test_ssa_aot_projection_descriptor.c` carries one typed constant
through the projection descriptor, checks its copied payload and hash impact,
and rejects a zero type token. The strict GCC descriptor and AOTIR contract
fixtures passed; modified AOTIR, projection, and descriptor sources compile
with `-std=c11 -Wall -Wextra -Werror -pedantic`.

This host has no `clang` or MSVC toolchain on `PATH`, and MinGW GCC 4.8 does
not support sanitizer flags. Configured WSL/MSVC matrix checks remain pending.

## Acceptance

Accepted as the shared typed constant-pool schema and descriptor handoff gate.
ExecIR module population, legacy decoder removal, C/LLVM lowering, and
executable AOT parity remain open.
