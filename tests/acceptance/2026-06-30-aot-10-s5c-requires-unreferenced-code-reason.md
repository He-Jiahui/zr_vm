# AOT 10-S5C / 12-S5C RequiresUnreferencedCode Reason Text

时间：2026-06-30 12:22:15 +08:00

## Scope

- `@requires_unreferenced_code("reason")` 的首个 metadata carrier reason 文本透传。
- 影响层：AOT C annotation warning scanner、生成 C trim diagnostic marker、focused parser/AOT tests。

## Baseline

- 10-S5B / 12-S5B 已能在 retained caller 静态调用 `requiresUnreferencedCode: true` callee 时输出
  `trim_warnings.annotationCount` 和逐条 `trim_warning.annotation[] reason=requires-unreferenced-code`。
- 旧输出没有用户 reason 文本；计划中的 `@requires_unreferenced_code("reason")` 仍缺少字符串诊断载体。
- 仓库仍有大量与本切片无关的既有未提交变更，本记录只接受本轮触达的 annotation warning 路径。

## Test Inventory

- Focused subsystem：`tests/parser/test_aot_c_reflection_annotation_preserve.c`
  - `requiresUnreferencedCode: true` + `requiresUnreferencedCodeReason: "uses \"name\" lookup"` 输出 1 条 warning。
  - marker 追加 `message="uses \"name\" lookup"`，双引号按 C 注释 marker 规则转义。
  - bool-only callee 保持旧 marker，无 `message=`。
  - unannotated static callee 保持 `trim_warnings.annotationCount = 0`。
- Adjacency：
  - `aot_reachability`
  - `aot_c_code_stripping`
  - `aot_c_reflection_annotation_preserve`
  - AOT C call shared-library smoke
  - AOT C dynamic deopt bridge smoke
  - AOT C source contracts

## Tooling Evidence

- WSL gcc was used for RED/GREEN and focused lower-layer validation.
- WSL clang was used to catch compiler portability warnings in the same AOT backend path.
- Windows MSVC Debug was used as compatibility smoke after Linux validation.

Commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_reflection_annotation_preserve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_reflection_annotation_preserve_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc --output-on-failure -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 8 && ctest --test-dir build-wsl-clang --output-on-failure -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build-msvc --config Debug --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test --parallel 8
ctest --test-dir build-msvc -C Debug --output-on-failure -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve"
.\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
```

## Results

- RED: after adding the reason-text fixture, WSL gcc built the focused target, then failed exactly on the new
  `message="uses \"name\" lookup"` assertion. Existing four annotation preserve tests still passed.
- GREEN: after production changes, WSL gcc focused annotation preserve passed 5/0.
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

## Acceptance Decision

Accepted for 10-S5C / 12-S5C.

The generated trim annotation warning now carries the optional user reason string with escaping, while preserving the old marker shape when no reason string exists. Runtime fallback diagnostics, warning counts, and static call target resolution behavior remain unchanged.

Remaining work: `@dynamically_accessed` dataflow, `@dynamic_dependency`, warning suppression/promotion, unannotated reflection warnings, cross-module annotation roots, type/member DESCRIPTION promotion, and complete trim analyzer / metadata sweep.
