# AOT 10-S5D / 12-S5D DynamicDependency Function Root

时间：2026-06-30 12:38:52 +08:00

## Scope

- `@dynamic_dependency` 的首个函数级 metadata carrier。
- 当前承载面：function decorator metadata `dynamicDependencyFunctionIndex: uint`。
- 影响层：AOT reachability annotation root collection、opt-in AOT C code stripping、generated-C trim diagnostics。

## Baseline

- 10-S5A / 12-S5A 已能把 `reflectable: true` function metadata 收集为 reflection annotation root。
- 旧 collector 不识别 dynamic dependency，因此无法通过注解直接保留一个 otherwise-unreachable target function。
- 本切片不新增语法，不处理按名/按 token 成员依赖，也不处理跨模块 dependency 绑定。

## Test Inventory

- Focused generated-C test：`tests/parser/test_aot_c_reflection_annotation_preserve.c`
  - root function metadata 写入 `dynamicDependencyFunctionIndex = 2`。
  - target child 本身不带 `reflectable` metadata。
  - opt-in code stripping 生成物必须包含 `code_stripping.annotationRoots = 1`、
    `code_stripping.annotationRoot[0] = 2` 和 `zr_aot_fn_2`。
  - 既有 unannotated prune 路径继续证明没有 annotation root 时 target 被裁剪。
- Focused reachability test：`tests/parser/test_aot_reachability.c`
  - 直接调用 `backend_aot_collect_reflection_annotation_roots(...)`。
  - 验证 `dynamicDependencyFunctionIndex` 被收集为单个 annotation root。

## Tooling Evidence

Commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_reflection_annotation_preserve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_reflection_annotation_preserve_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_reachability_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 8 && ctest --test-dir build-wsl-gcc --output-on-failure -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_reachability_test zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 8 && ctest --test-dir build-wsl-clang --output-on-failure -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build-msvc --config Debug --target zr_vm_aot_reachability_test zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test --parallel 8
ctest --test-dir build-msvc -C Debug --output-on-failure -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve"
.\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
git diff --check -- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c tests/parser/test_aot_reachability.c tests/parser/test_aot_c_reflection_annotation_preserve.c
```

## Results

- RED: generated-C fixture failed on the new dynamic dependency assertion because old root collection did not emit
  `annotationRoot[0] = 2` and did not keep `zr_aot_fn_2`.
- GREEN: WSL gcc focused annotation preserve passed 6/0.
- WSL gcc focused reachability passed 8/0.
- WSL gcc CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`: 3/3.
- WSL gcc call shared-library smoke: 5/0.
- WSL gcc dynamic deopt bridge smoke: 7/0.
- WSL gcc source contracts: 22/0.
- WSL clang same CTest group: 3/3.
- WSL clang call shared-library smoke: 5/0.
- WSL clang dynamic deopt bridge smoke: 7/0.
- WSL clang source contracts: 22/0.
- Windows MSVC Debug same CTest group: 3/3.
- Windows MSVC Debug call shared-library smoke: 0 failures / 5 ignored.
- Windows MSVC Debug dynamic deopt bridge smoke: 0 failures / 7 ignored.
- Windows MSVC Debug source contracts: 22/0.
- `git diff --check`: exit 0, LF/CRLF warnings only.

## Acceptance Decision

Accepted for 10-S5D / 12-S5D.

The AOT annotation root path now supports a direct function-index dynamic dependency carrier and preserves the target
through opt-in code stripping. Remaining work: `@dynamically_accessed` dataflow, token/name member dynamic dependencies,
cross-module dependency binding, warning suppression/promotion, unannotated reflection warnings, type/member
DESCRIPTION promotion, and complete trim analyzer / metadata sweep.
