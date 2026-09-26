# SSA 07.01: owned projection to shared AOTIR descriptor

## Scope

`ZrParser_AotIrProjection_BuildDescriptor` bridges an owned AOT projection to
the shared `SZrAotIrModule` contract. It copies representation-dependent
instruction, block, phi, frame-slot and source-map records, while borrowing
the projection's operand/result/successor/memory/state-map pools for the
duration of the returned descriptor. `FreeDescriptor` releases only copied
records. The module remains descriptor-only and emits no native artifact.

## Test-first evidence

The new fixture first failed to link because the bridge API was absent. After
implementation, strict GCC 11.4 and Clang 14 builds and runs pass, GCC
ASan/UBSan passes without a report, and MSVC x64 `/W4 /WX /std:c11` builds and
runs pass. The fixture asserts the copied typed instruction and frame slot,
owner identity, self-rebased `module.functions` pointer, shared module
validation, and cleanup.

## Lifetime contract

The caller must keep the owned projection alive while using the descriptor,
because operand/result/successor/memory and logical-state-map pools are borrowed.
The descriptor owns its converted record arrays and can be freed independently
of the projection only after all consumers stop reading it. No host pointer is
included in the AOTIR hash.

## Acceptance

The direct cross-toolchain slice is accepted. The CMake target
`ssa_aot_projection_descriptor` is registered; a full post-schema-v4 CMake
rebuild is pending because concurrent workspace glob churn previously expanded
the build and was stopped. C/LLVM artifact emission remains open.
