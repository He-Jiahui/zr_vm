# AOT 11-S6B Function Binding Compatibility Scan

时间：2026-06-28 00:56:57 +08:00

状态：11-S6 支撑子切片完成；完整 11-S6 仍进行中。dynamic 模块加载拒绝、typed 调用边界
deopt、跨模块 token resolve 集成和端到端 ABI drift 注入仍待后续。

## Scope

- 新增 `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()`，扫描
  `SZrFunction.moduleMetadataBindings` 并返回首个不兼容 binding、对应 ref record 与 report。
- 影响层：core runtime metadata binding compatibility、module-focused tests、AOT 11 metadata docs。

## Baseline

- 11-S6A 只有单个 `SZrMetadataTokenBinding` predicate；调用方需要自己遍历 function binding 表并寻找
  module ref record。
- 已知仓库基线：WSL gcc/clang 构建仍会在既有 execution computed-goto 路径输出扩展警告；
  MSVC Debug 构建仍会在既有 execution/object/reflection 代码输出警告。

## Test Inventory

- Unit/focused subsystem：`tests/module/test_metadata_runtime_binding_compatibility.c`。
- Boundary cases：空 `moduleMetadataBindings` 返回 compatible，并清空 `outBinding` / `outRefRecord`。
- Negative cases：第二个 binding 的 signature hash 漂移应返回首个不兼容 binding；module metadata
  token record 上的 version range 应触发 module version mismatch。
- Integration/project cases：focused CTest `metadata_runtime_binding_compatibility`。

## Tooling Evidence

- WSL gcc:
  - `cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_binding_compatibility_test -j 2`
  - `./build-wsl-gcc/bin/zr_vm_metadata_runtime_binding_compatibility_test`
  - `ctest --test-dir build-wsl-gcc -R metadata_runtime_binding_compatibility --output-on-failure`
- WSL clang:
  - `cmake --build build-wsl-clang --target zr_vm_metadata_runtime_binding_compatibility_test -j 2`
  - `./build-wsl-clang/bin/zr_vm_metadata_runtime_binding_compatibility_test`
  - `ctest --test-dir build-wsl-clang -R metadata_runtime_binding_compatibility --output-on-failure`
- Windows MSVC Debug:
  - `cmake --build E:\Git\zr_vm\build-msvc --config Debug --target zr_vm_metadata_runtime_binding_compatibility_test -- /m:2`
  - `E:\Git\zr_vm\build-msvc\bin\Debug\zr_vm_metadata_runtime_binding_compatibility_test.exe`
  - `ctest --test-dir E:\Git\zr_vm\build-msvc -C Debug -R metadata_runtime_binding_compatibility --output-on-failure`

## Results

- WSL gcc：direct test 15/0；CTest `metadata_runtime_binding_compatibility` 1/1。
- WSL clang：direct test 15/0；CTest `metadata_runtime_binding_compatibility` 1/1。
- Windows MSVC Debug：direct test 15/0；CTest `metadata_runtime_binding_compatibility` 1/1。
- 修复：function scan 先查 function-local token record，再查 module token record，保证 AssemblyRef/module-ref
  的 version range 可用于漂移判定。

## Acceptance Decision

Accepted for 11-S6B support scope. The function-level compatibility scan is available and tested across the focused
toolchain matrix. Full 11-S6 runtime loader reject, typed-boundary deopt, cross-module token resolve integration, and
end-to-end no-crash ABI drift injection remain open.
