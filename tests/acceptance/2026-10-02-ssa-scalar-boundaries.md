# SSA Publication and Scalar Boundary Validation

## Scope

This focused continuation covers GVN dominance, unsupported pass-manager
analysis rollback, module StateMap identity publication, empty StateMap
handling in scalar VM materialization, and literal ADD/SUB source branches.
It does not accept an entire SSA milestone or claim loop-phi, general effects,
nonempty StateMap recovery, AOT execution, or the default compiler's migration.

The source snapshot was taken at HEAD
`8ad6ded85d3a1879bd85a3722fe1e5a4c277b5c4` with the remaining changes in the
shared worktree. Raw source SHA256 values are preserved in
`D:/tmp/zr_vm/ssa-control/scalar-boundary-source-snapshot.json` (receipt SHA256
`c4a145bb4bbb9bbf5fea2ca6a0139b775746fbee6631a7befaa8c9e7b9454340`).
Foreign source, index, jobs, generated files, and submodules were preserved.

## Actual Results

| Slice | Evidence | Result |
| --- | --- | --- |
| GVN dominating reuse and sibling rejection | Native current-source standalone target, `ssa-scalar-validation/native/zr_vm_ssa_gvn_range_test/{build,run}.log` | MSVC build 0, run 0; committed `52aee9a9` |
| Unsupported LOOPS/LIVENESS pipeline rollback | Native current-source standalone target, `ssa-scalar-validation/native/zr_vm_ssa_pass_manager_scalar_test/{build,run}.log` | MSVC build 0, run 0; committed `8ad6ded8` |
| Published StateMap token/signature/generation | Exact builder target source list, `ssa-scalar-validation/native/zr_vm_ssa_builder_cfg_test/{build,run}.log` | Test before production fix: build 0, run 1; after fix: build 0, run 0 (`ssa builder CFG PASS`); committed `e0d82456` |
| Empty-map scalar VM boundary | Fresh current-source native CMake target, `ssa-control/empty-map-final-{build,test}.log` | Build 0; CTest `ssa_exec_ir_execbc_vm` 1/1 passed; 22 Unity cases; committed `2e64113d` |
| Source ADD/SUB and division rejection | Fresh current-source native CMake target, `ssa-control/source-arithmetic-scoped-{build,test,last-test}.log` | Build 0; CTest `ssa_source_execbc_vm` 1/1 passed; 5 tests, 0 failures; committed `4cc6d506` |

All log paths in this table are relative to `D:/tmp/zr_vm`.

## Commands and Environment

Windows MSVC 19.44.35228.0 was bootstrapped with the installed
`E:/Visual Studio/Common7/Tools/VsDevCmd.bat`. Compiler temporary files were
placed below each D-only build directory. Focused standalone commands:

```powershell
python D:/tmp/zr_vm/ssa-control/run_native_scalar_validation.py zr_vm_ssa_gvn_range_test zr_vm_ssa_pass_manager_scalar_test
python D:/tmp/zr_vm/ssa-control/run_native_scalar_validation.py zr_vm_ssa_builder_cfg_test
```

`run_native_source_fresh.py` configured
`D:/tmp/zr_vm/ssa-source-native-current` with Ninja, Debug, static libraries,
tests enabled, and network/debug/thread libraries, CLI, language server and
Rust bindings disabled. The source VM target built successfully in 888 steps.
The final incremental commands were:

```powershell
python D:/tmp/zr_vm/ssa-control/run_native_existing.py zr_vm_ssa_exec_ir_execbc_vm_test ssa_exec_ir_execbc_vm empty-map-final
python D:/tmp/zr_vm/ssa-control/run_native_existing.py zr_vm_ssa_source_execbc_vm_test ssa_source_execbc_vm source-arithmetic-scoped
```

## RED Evidence and Limits

The original source test execution failed because the materializer rejected
every attached StateMap and conditional-arm preflight rejected binary syntax.
The empty-map regression separately produced 21 tests with one failure in
`ssa-control/empty-map-red-direct.log` before the production fix. Nonempty maps
and incorrect identity were already rejected. The final extra negative case
also rejects side-pool data when there are no entries.

The final ADD/SUB preflight is limited to conditional-arm returns in
`compiler_semantic_cfg_arithmetic.c`. The shared linear-expression predicate
used by loops and finally is unchanged. The source target was rebuilt and
passed again after this scope adjustment.

The first empty-map CTest attempt timed out under host load; its direct run
provided the useful RED. The later final CTest run passed. No GCC/Clang,
sanitizer, or full SSA matrix result is claimed for these new slices.

An early WSL `pgrep` command was parsed by its default shell as a pipeline and
accidentally launched the existing in-source Ninja/CMake regeneration in E:.
This live job was preserved under the coordination constraint. Subsequent WSL
commands used `-e`; deliberate validation outputs were confined to D:. No
capture START, tree hold, release, or network proof is implied by these checks.
