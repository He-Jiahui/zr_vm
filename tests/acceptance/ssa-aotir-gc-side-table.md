# SSA 07.01: AOTIR GC side-table retention

## Scope

ExecBC and AOT projections now own the validated GC map entries, live-slot and
inline-offset pools, and GC root IDs. The projection-to-AOTIR descriptor borrows
those complete side tables instead of retaining only `gcMapCount`; AOTIR
validation checks ranges, instruction sites, physical frame slots, and root
IDs, while semantic hashing includes every entry and pool value.

This is a descriptor-only retention slice. It does not publish GC roots to a
native artifact or claim executable C/LLVM generation. Deopt reconstruction
tables remain a separate follow-up slice.

## Evidence

`tests/parser/test_ssa_aot_projection_descriptor.c` builds a projection with a
GC safepoint, live physical slot, root range, and root value, validates the
descriptor through the backend adapter, and checks that removing the GC side
tables changes the AOTIR hash. Strict GCC compilation and execution passed.
The modified projection common/lifetime sources also compile with strict GCC
warnings enabled. This host has no `clang` or MSVC toolchain on `PATH`, and its
MinGW GCC 4.8 does not support sanitizer flags; those matrix checks are
pending in the configured WSL/MSVC build environments.

## Acceptance

Accepted as the GC side-table ownership and descriptor-retention gate. Native
root publication, deopt recipe retention, C/LLVM code generation, and
executable AOT parity remain open.
