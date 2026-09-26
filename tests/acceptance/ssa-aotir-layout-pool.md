# SSA 07.01: AOTIR layout-pool retention

## Scope

The shared AOTIR module now carries typed layout records (`id`, type token,
size, alignment, and layout hash). Projection descriptors copy the supplied
records into their owned module view; AOTIR validates alignment, nonzero
identity/hash, duplicate IDs, and semantic hashes. Schema version 6 marks the
contract change.

`ZrParser_ExecIr_LowerAotWithConstantsAndLayouts` deep-copies layout rows into
the owned projection before the existing AOT move. This preserves `layoutId`
references without making the emitter infer inline representation from legacy
opcodes.

## Evidence

`tests/parser/test_ssa_aot_projection_descriptor.c` carries one layout record
through descriptor validation and checks the copied payload. The same fixture
rejects a deopt aggregate whose `layoutId` is absent from the module pool or
whose type token disagrees with the referenced layout. The existing state-map
projection fixture exercises the layout-aware lowering entry point and asserts
the projection copy. Strict GCC descriptor, AOTIR contract, and modified
projection/lowering object checks passed.

This host has no `clang` or MSVC toolchain on `PATH`, and MinGW GCC 4.8 does
not support sanitizer flags. Configured WSL/MSVC matrix checks remain pending.

## Acceptance

Accepted as the shared layout-pool schema and projection handoff gate. Inline
field/return lowering, root relocation consumers, C/LLVM code generation, and
executable AOT parity remain open.
