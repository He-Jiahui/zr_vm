# 2026-07-02 AOT 11-S7ZL / 12-S7 Provider AOT Runtime Load Request

## 时间

- 2026-07-02 06:41:32 +08:00

## 状态

- 完成。
- 范围限定为 strict AOT module loader 消费上一切片的 provider AOT load request：canonical provider import 会走 provider `.zrp` 的 source/binary/library path，descriptor validation 使用 provider-local module name，runtime diagnostic 暴露 provider AOT library path。
- 不声明 provider 动态库成功加载端到端、multi-version selection、export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

## RED

- 新增 `tests/library/test_project_import_aot_provider_runtime.c` 和 focused target `zr_vm_project_import_aot_provider_runtime_test`。
- 旧 runtime 对 `$mathLocal@2.1.0/ops/sum` 仍按消费者工程路径解析 AOT artifacts；WSL GCC direct test 编译通过但失败：
  - `test_project_import_aot_provider_runtime.c:132` `lastError` 为 null，未报告 `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.*` provider library path。

## GREEN

- `zr_vm_library/src/zr_vm_library/aot_runtime.c` 的 `aot_runtime_prepare_record()` 在 canonical `$alias@version/module` 导入上调用 `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()`。
- `.zrp` provider request 现在驱动 runtime source path、binary `.zro` path、backend-specific dynamic library path，并把 descriptor module-name validation 切换为 provider-local `descriptorModuleName`。
- `.zrm` provider request 在 runtime 侧 fail-closed 为 “archive entries are not dynamic libraries”，避免把 archive entry 错当作 filesystem dynamic library。
- runtime record/cache 仍以 canonical module key 命名，保持 caller import identity 不变。

## 验证

- WSL GCC: `cmake --build build-wsl-gcc --target zr_vm_project_import_aot_provider_runtime_test -j 2 && ./build-wsl-gcc/bin/zr_vm_project_import_aot_provider_runtime_test`
  - 1 Test / 0 Failures / 0 Ignored。
- WSL clang: `cmake -S . -B build-wsl-clang && cmake --build build-wsl-clang --target zr_vm_project_import_aot_provider_runtime_test -j 2 && ./build-wsl-clang/bin/zr_vm_project_import_aot_provider_runtime_test`
  - 1 Test / 0 Failures / 0 Ignored。
- Windows MSVC Debug: `cmake -S . -B build-msvc && cmake --build build-msvc --target zr_vm_project_import_aot_provider_runtime_test --config Debug -j 2 && .\build-msvc\bin\Debug\zr_vm_project_import_aot_provider_runtime_test.exe`
  - 1 Test / 0 Failures / 0 Ignored。
- WSL GCC existing resolver guard: `zr_vm_project_import_resolver_test`
  - 9 Tests / 0 Failures / 0 Ignored。
- Additional exploratory regression check: WSL GCC `zr_vm_project_import_canonicalization_test` currently fails outside this slice at `test_project_compile_applies_dependency_import_version_range_to_assembly_ref` (`assemblyRef` null, 35 Tests / 1 Failure). This was not fixed in this slice to avoid touching unrelated parser/import metadata work in the dirty worktree.
- This focused target is currently direct-run only and not registered as an individual CTest suite entry.

## 后续缺口

- provider dynamic library success fixture and end-to-end provider execution。
- multi-version provider selection。
- export metadata attach。
- complete metadata sweep/pruning。
- full trim analyzer。
- broader runtime ABI drift/deopt closure。
