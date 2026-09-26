# SSA 07.01: AOTIR constant-pool retention

## Scope

The shared AOTIR module now carries a typed constant pool with flags and raw
bits. Projection descriptors copy the supplied constant records into their
owned module view; AOTIR validation rejects missing pools and zero type tokens,
out-of-range `CONSTANT` instruction pool indices, missing pools referenced by a
`CONSTANT` instruction, and non-matching instruction type tokens. Semantic
hashing covers every constant record. Schema version 5 marks the contract
change.

Callers can use `ZrParser_ExecIr_LowerAotWithConstants` to deep-copy module
constant rows into the owned projection before descriptor conversion. C/LLVM
constant consumers still need to replace legacy opcode/pool decoding, so no
native artifact is claimed.

## Evidence

`tests/parser/test_ssa_aot_projection_descriptor.c` carries one typed constant
through the projection descriptor, checks its copied payload and hash impact,
and rejects a zero type token. `tests/parser/test_ssa_aotir_state_map.c` now
exercises the constant-aware AOT lowering entry point and asserts the copied
projection record is non-aliased. The strict GCC descriptor and AOTIR contract
fixtures passed; the contract fixture also rejects an out-of-range `CONSTANT`
pool index, a missing pool, and a mismatched instruction type token. Modified
AOTIR, projection, and descriptor sources compile with
`-std=c11 -Wall -Wextra -Werror -pedantic`.

This host has no `clang` or MSVC toolchain on `PATH`, and MinGW GCC 4.8 does
not support sanitizer flags. Configured WSL/MSVC matrix checks remain pending.

## Acceptance

Accepted as the shared typed constant-pool schema, population, and descriptor
handoff gate. Legacy decoder removal, C/LLVM lowering, and
executable AOT parity remain open.
