# AOT M1 / 03-S2a SemIR Static C Type Annotations

## Scope

- Slice: static C type annotation foundation from `03-S2` in `docs/plans/aot/03-instruction-set-refactor.md`.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: record one AOT-facing static C type for each SemIR type-table entry and preserve that annotation through binary `.zro` roundtrip.

## Completed Items

- Added `EZrStaticCType` in `zr_vm_common/include/zr_vm_common/zr_type_conf.h`.
- Extended `SZrFunctionTypedTypeRef` and `SZrIoFunctionTypedTypeRef` with `staticCType` and `staticCTypeId`.
- Annotated SemIR type-table entries while compiling type refs in `compiler_semir.c`.
- Mapped primitive scalars, bool, GC references, native pointer/data, and inline struct layout ids to stable static C type categories.
- Serialized the annotations in `writer.c` and loaded them through `io.c` / `io_runtime.c` behind binary patch `ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES`.
- Added `tests/parser/test_semir_static_c_types.c`.
- Registered the focused target and CTest entry `semir_static_c_types`.

## RED / GREEN Evidence

- RED: `zr_vm_semir_static_c_types_test` failed to build before implementation because `SZrFunctionTypedTypeRef` did not expose `staticCType` / `staticCTypeId` and `ZR_STATIC_C_TYPE_*` did not exist.
- GREEN: the focused test now compiles a mixed scalar/reference/struct fixture, verifies SemIR static C annotations, writes `.zro`, loads it back to runtime, and verifies the same annotations after roundtrip.

## Tests

Focused validation:

```text
zr_vm_semir_static_c_types_test
zr_vm_semir_pipeline_test
ctest -R "semir_static_c_types"
```

Observed results:

- SemIR static C types: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.
- Focused CTest filter `semir_static_c_types`: 1 test / 0 failures.

## Status

- Status: 03-S2a complete; 03-S2 remains partially open.
- The conflict-triggered deopt path required by the full `03-S2` acceptance has not been implemented yet.
- M1 remains partially complete. Typed-block generic arithmetic opcode rejection is still open before moving to broad scalar pure-C lowering.
