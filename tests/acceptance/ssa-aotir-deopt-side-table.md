# SSA 07.01: AOTIR deopt side-table retention

## Scope

ExecBC and AOT projections now own deopt state rows, reconstruction value IDs,
aggregate recipes, and aggregate-field records. The projection-to-AOTIR
descriptor borrows those pools, and AOTIR validates storage shape, ranges,
nonzero recipe identities, value/aggregate references, and state-local
aggregate edges. Semantic hashing covers every deopt row and pool record.

This slice retains and validates the logical recipe data only. It does not
materialize a runtime frame, publish native deopt metadata, or claim executable
C/LLVM reconstruction.

## Evidence

`tests/parser/test_ssa_aot_projection_descriptor.c` carries one deopt state,
aggregate, field, and reconstruction value through the descriptor, validates
the shared AOTIR module, and checks that removing the recipe changes its hash.
The strict GCC fixture passed; modified AOTIR, projection, lifetime, and
descriptor sources compile with `-std=c11 -Wall -Wextra -Werror -pedantic`.

This host has no `clang` or MSVC toolchain on `PATH`, and MinGW GCC 4.8 does
not support sanitizer flags. Configured WSL/MSVC matrix checks remain pending.

## Acceptance

Accepted as the deopt side-table retention and validation gate. Runtime frame
reconstruction, native deopt publication, C/LLVM code generation, and
executable AOT parity remain open.
