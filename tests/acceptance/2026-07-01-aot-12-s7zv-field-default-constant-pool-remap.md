# AOT 12-S7ZV / 11-S7 FieldDef Default Constant Pool Remap

- Completion time: 2026-07-01 14:51:47 +08:00
- Status: completed support sub-slice for FieldDef default-value constant-pool slice retention and remap.

## Scope

This slice extends `.zrp` data metadata to version 3 so `SZrZrpMetadataFieldDefRow` carries
`defaultValueConstantPoolOffset` and `defaultValueConstantPoolLength`. Emitted zrp metadata pruning now keeps
FieldDef default-value referenced constant-pool slices, rewrites retained FieldDef offsets into the compacted
constant pool, and still drops orphan constant-pool payload.

This does not close cross-module export-token publication/rewrite, complete metadata sweep/pruning, complete trim
analyzer, annotation-driven warning policy, or runtime ABI drift deopt coverage.

## RED

- `tests/module/test_zrp_metadata_format.c` expected `SZrZrpMetadataFieldDefRow` default-value constant-pool fields.
- `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c` expected retained FieldDef default-value constant-pool slices
  to be remapped while unrelated bytes were dropped.
- The focused MSVC build failed because `defaultValueConstantPoolOffset` and
  `defaultValueConstantPoolLength` were not members of `SZrZrpMetadataFieldDefRow`.

## GREEN

- `ZR_ZRP_METADATA_VERSION` is now 3.
- `SZrZrpMetadataFieldDefRow` validates `defaultValueConstantPoolOffset/defaultValueConstantPoolLength` against the
  constant pool section.
- `backend_aot_c_zrp_metadata_constant_pool.{h,c}` owns retained constant-pool slice collection, compacted copy,
  identity checks, and FieldDef default-value offset rewrites.
- `backend_aot_c_zrp_metadata_prune.c` builds the constant-pool remap with the existing token/signature/string remaps,
  writes the compacted constant pool, and skips raw constant-pool section copying.
- CLI zrp metadata dump/version tests now expect metadata version 3.

## Validation

- Windows MSVC Debug direct runs passed:
  - `zr_vm_zrp_metadata_format_test.exe` 12/0
  - `zr_vm_aot_c_zrp_metadata_pool_pruning_test.exe` 5/0
  - `zr_vm_aot_c_zrp_metadata_pruning_test.exe` 5/0
  - `zr_vm_aot_c_source_contracts_test.exe` 24/0
  - `zr_vm_aot_c_code_stripping_test.exe` 10/0
  - `zr_vm_aot_c_zrp_metadata_size_deltas_test.exe` 2/0
  - `zr_vm_cli_zrp_metadata_dump_test.exe`
  - `zr_vm_cli_args_test.exe`
- WSL GCC and WSL Clang built the same focused targets and passed the same direct test set.
- Focused CTest selection passed 7/7 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- `git diff --check` returned exit code 0; only existing line-ending conversion warnings were printed.
