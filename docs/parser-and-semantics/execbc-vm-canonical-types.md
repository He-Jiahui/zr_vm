---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_execbc_vm.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_execbc_vm_canonical_types.inc
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/acceptance/ssa-source-execbc-vm.md
doc_type: module-detail
---

# ExecBC VM Canonical Types

Source-built ExecIR uses semantic canonical IDs for scalar types. Those IDs
are local to the semantic context and are not `EZrValueType` values. The
canonical materializer entry point takes that owning context explicitly and
resolves each type through `ZrParser_CanonicalType_Find`.

Executable values and instruction targets accept primitive bool and i64
nodes. Narrow integer, reference, owner, nominal, unknown, and missing-context
inputs fail before a Core function is allocated. No type is inferred from the
numeric ID. Constant pool tokens use the same canonical identity as their
instruction result.

An unreferenced constant pool entry may also describe canonical primitive
NULL, with flags and bits both zero. The adapter resolves its copied descriptor
to `ZR_VALUE_TYPE_NULL` and preserves the complete input pool and its indices.
This descriptor is distinct from the Oracle's undefined value kind. Every
CONSTANT reference to that NULL entry is rejected with `UNSUPPORTED` at its
instruction/source location; NULL value annotations, instruction targets,
match tokens and nullable wrappers remain unsupported. Malformed NULL flags or
bits produce `INVALID_VALUE`; a 64-bit payload exceeding the diagnostic's
32-bit actual field is reported as `UINT32_MAX`.

The ordinary VM planner emits only referenced scalar constants. A valid
two-entry projection containing INT64 42 and unused NULL therefore remains
unchanged while its Core function has one constant and returns 42. The adapter
does not implement NULL consumption or relax memory, effect, ownership,
CONVERT, or STORE validation.

The canonical type array must be initialized, have the correct element size,
and have a non-overflowing capacity with `length <= capacity` and storage for
nonempty contents. The adapter rejects malformed descriptors before lookup.

The adapter copies the instruction, slot-value, and constant arrays into a
temporary borrowed view, translates their type fields, and calls the normal
materializer. `COMPARE.typeToken` is a comparison-kind tag and stays unchanged;
its `matchTypeToken` is resolved as a type. CFG, source/PC maps, phi schedules,
memory/effect fields and GC/deopt metadata stay intact and pass the ordinary
materializer's validation. The original projection remains unchanged on both
success and failure. Only the three copied arrays are freed by this adapter.

Successful function rooting and emission ownership follow the normal
materializer contract. The focused tests deliberately reserve IDs before
interning primitive types, execute the emitted function through Core, and
check unknown/narrow/mismatched/missing inputs plus byte-for-byte preservation
of all copied input arrays. The two existing registered adapter tests also
exercise two successful NULL/baseline configurations and eleven strict
rejections, with exact diagnostics and empty outputs. Real source branch tests
resolve against the compiler's own semantic context.

The 2026-09-30 root MSVC test-first run accepted an invalid type-array
descriptor in `canonical-type-shape-red-direct.log`. After array validity and
capacity checks were added, `ssa_exec_ir_execbc_vm` passed all 18 Unity cases
in `current-scalar-shape-fixtures-ctest.log`, including eight negative adapter
inputs with empty output and unchanged projection arrays. The same selection
passed both real source branch cases. Broader GCC/Clang and full semantic
matrix results are maintained in the source acceptance record.

The 2026-10-02 UTC (2026-10-03 Asia/Shanghai) unused-NULL preparation observed
the unchanged adapter reject a legal unused pool descriptor. A D-only copied
adapter passed all three focused tests and executed Core VM return 42. Root's
formal MSVC build then succeeded; the first registered CTest timed out with no
stdout after 41.61 seconds and an unlocalized cause. The unchanged executable
passed a direct 27-case run and the exact registered retry, 1/1 in 0.57 seconds.
This establishes the recorded MSVC native adapter scope while preserving the
unexplained timeout. GCC/Clang, sanitizer and full source-loop VM gates remain
open. Evidence and remaining gates are in
[the focused acceptance record](../../tests/acceptance/2026-10-03-ssa-canonical-unused-null-pool.md).
