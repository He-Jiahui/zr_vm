# 2026-07-02 AOT 11-S7ZK / 12-S7 Provider AOT Load Request

## 时间

- 2026-07-02 06:13:21 +08:00

## 状态

- 完成。
- 范围限定为 standalone provider import 的 AOT load-request 规划层：把 raw import specifier 和 provider location discovery 结果转换成运行期后续可消费的 backend、canonical module key、descriptor module name、provider source/binary/intermediate path、provider AOT library path 或 `.zrm` archive entry。
- 不声明 provider runtime dynamic loading、multi-version selection、export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

## RED

- `tests/library/test_project_import_resolver.c` 新增 `.zrp` provider AOT load request 断言。
- 旧代码没有 `SZrLibrary_ProjectImportProviderAotLoadRequest` 和 `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()`，WSL GCC focused build 编译失败。

## GREEN

- `zr_vm_library/include/zr_vm_library/project.h` 新增 `SZrLibrary_ProjectImportProviderAotLoadRequest` 和 public API。
- `zr_vm_library/src/zr_vm_library/project/project_import_provider_location.c` 复用 provider location API，生成：
  - `$alias@version/module` canonical module key。
  - descriptor-local module name，例如 `ops/sum`。
  - `.zrp` provider 的 source/binary/intermediate path 和 `bin/aot_c/lib/zrvm_aot_<sanitized-module>.<ext>` / LLVM backend root。
  - `.zrm` provider 的 archive/entry view，并保持 libraryPath 为空，避免把 archive entry 错当作 filesystem dynamic library。
- `tests/library/test_project_import_resolver.c` 覆盖 `.zrp` provider 的 C backend load request 与 `.zrm` provider 的 LLVM backend request/entry mirror。

## 验证

- WSL GCC: `cmake --build build-wsl-gcc --target zr_vm_project_import_resolver_test -j 2 && ./build-wsl-gcc/bin/zr_vm_project_import_resolver_test`
  - 9 Tests / 0 Failures / 0 Ignored。
- WSL clang: `cmake --build build-wsl-clang --target zr_vm_project_import_resolver_test -j 2 && ./build-wsl-clang/bin/zr_vm_project_import_resolver_test`
  - 9 Tests / 0 Failures / 0 Ignored。
- Windows MSVC Debug: `cmake --build build-msvc --target zr_vm_project_import_resolver_test --config Debug -j 2 && .\build-msvc\bin\Debug\zr_vm_project_import_resolver_test.exe`
  - 9 Tests / 0 Failures / 0 Ignored。
- 该 focused target 当前未作为独立 CTest 注册。

## 后续缺口

- provider runtime dynamic loading。
- multi-version provider selection。
- export metadata attach。
- 完整 metadata sweep/pruning。
- full trim analyzer。
- 更完整的 runtime ABI drift/deopt closure。
