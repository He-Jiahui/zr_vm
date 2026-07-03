# AOT 11-S7ZSJ / 12-S7ZZZA FieldDef-Owned Orphan Metadata Sweep

Date: 2026-07-03 05:04:05 +08:00

Status: complete for this sub-slice. The broader AOT 07-12 goal remains active.

## Scope

- Affected layers: emitted `.zrp` metadata pruning, TypeDef/FieldDef layout compaction, metadata pools, and member-token
  remap sidecars.
- FieldDef rows are now retained only when their owner TypeDef survives metadata pruning.
- Orphan FieldDef rows no longer keep an otherwise unreachable TypeDef alive.
- Retained TypeDef rows now have their field ranges rewritten to compacted FieldDef token space.

## Baseline

- TypeDef retention previously treated any FieldDef `ownerTypeToken` reference as a root, so an unreachable type could
  survive only because it owned a field row.
- FieldDef rows were copied wholesale into the pruned `.zrp` blob, which kept orphan fields, field strings,
  field signatures, constants, and member-token remap entries reachable after their owner type had been removed.
- TypeDef method and generic ranges were already adjusted after pruning, but field ranges did not yet have the same
  owner-retained compaction path.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
  - drops an orphan TypeDef that owns only a FieldDef row
  - retains the live TypeDef, its live MethodDef, and its live FieldDef
  - verifies compacted `.zrp` blob size, TypeDef/MethodDef/FieldDef counts, compacted string pool, TypeDef field range,
    and retained FieldDef `MEMBER_DEF` token
- Adjacent focused regression set:
  - `aot_c_zrp_metadata_pruning`
  - `aot_c_zrp_metadata_pool_pruning`
  - `aot_c_zrp_metadata_typedef_pruning`
  - `aot_c_zrp_metadata_typespec_pruning`

## Tooling Evidence

- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test -j 4`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pool_pruning_test`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_typedef_pruning_test`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_typespec_pruning_test`
- WSL clang:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_zrp_metadata_pruning_test -j 4`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_zrp_metadata_pruning_test`
- WSL clang:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_zrp_metadata_pool_pruning_test zr_vm_aot_c_zrp_metadata_typedef_pruning_test zr_vm_aot_c_zrp_metadata_typespec_pruning_test -j 2`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_zrp_metadata_pool_pruning_test`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_zrp_metadata_typedef_pruning_test`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_zrp_metadata_typespec_pruning_test`
- Windows MSVC Debug, Visual Studio environment:
  `cmake --build build-msvc --target zr_vm_aot_c_zrp_metadata_pruning_test --config Debug -j 4`
- Windows MSVC Debug:
  `build-msvc\bin\Debug\zr_vm_aot_c_zrp_metadata_pruning_test.exe`

## Results

- RED: the new orphan TypeDef-with-FieldDef fixture failed on WSL GCC with `Expected 576 Was 692`, proving the old
  pruning path retained the dead type/field metadata.
- Fix: TypeDef retention no longer treats FieldDef owner references as roots; FieldDefs are counted and copied only
  when their owner TypeDef survives; TypeDef field ranges are rewritten to compacted FieldDef token space.
- Fix: string pool, signature pool, constant-pool remap, and member-token remap construction now use the retained
  FieldDef set instead of the raw FieldDef table.
- GREEN: WSL GCC `aot_c_zrp_metadata_pruning` passed 16/0; adjacent pool, typedef, and typespec pruning tests passed
  8/0, 2/0, and 2/0.
- GREEN: WSL clang `aot_c_zrp_metadata_pruning` passed 16/0; adjacent pool, typedef, and typespec pruning tests passed
  8/0, 2/0, and 2/0.
- GREEN: Windows MSVC Debug `aot_c_zrp_metadata_pruning` passed 16/0.

## Acceptance Decision

- Accepted for this sub-slice: owner-TypeDef-based FieldDef orphan rows are swept from emitted `.zrp` metadata, and the
  associated pools, TypeDef field ranges, and member-token remap sidecar are compacted with the retained rows.
- Remaining work: full metadata sweep/pruning is not claimed complete. The full trim analyzer, annotation/promotion
  policy, broader ABI drift/deopt coverage, and remaining metadata edge cases stay open for later slices.
