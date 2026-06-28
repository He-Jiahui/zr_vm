# AOT 11-S6C Dynamic Loader Binding Reject

时间：2026-06-28 01:18:38 +08:00

状态：11-S6 dynamic AOT module-load reject 子切片完成；完整 11-S6 仍进行中。typed 调用边界
deopt、跨模块 token resolve 集成和更完整的端到端 ABI drift 注入仍待后续。

## Scope

- `zr_vm_library/src/zr_vm_library/aot_runtime.c` 在 AOT dynamic module load 期间，加载 embedded/zro metadata
  function、构建 function table、attach metadata runtime 后，扫描每个 function 的
  `moduleMetadataBindings`。
- 扫描复用 `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()`；首个不兼容 binding 会拒绝
  AOT module load，并在 runtime last-error 中写入状态名、function index、ref token、metadata/signature token、
  signature hash、module signature hash 和 layout version/hash。
- 新增 focused runtime-loader 测试 `tests/parser/test_aot_c_metadata_binding_loader.c`，直接生成 AOT C
  shared library，并通过 `ZrLibrary_AotRuntime_ExecuteEntry()` 触发 loader。

## RED

- RED 先注入一条 embedded `.zro` 会保留的 `SZrMetadataTokenBinding` signature hash drift。
- WSL gcc 直接运行 `zr_vm_aot_c_metadata_binding_loader_test` 失败：
  `Expected FALSE Was TRUE`，证明旧 loader 执行成功、没有拒绝 ABI drift。

## GREEN

- GREEN 后 loader 返回失败，`lastError` 包含 `AOT metadata binding compatibility failed`、
  `module 'main'` 与 `SIGNATURE_HASH_MISMATCH`。
- 既有正常 shared-library smoke 继续通过，证明空 binding/兼容路径未被新 gate 拦截。

## Tooling Evidence

- WSL gcc:
  - `cmake --build build-wsl-gcc --target zr_vm_aot_c_metadata_binding_loader_test -j 2`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_metadata_binding_loader_test`
  - `ctest --test-dir build-wsl-gcc -R aot_c_metadata_binding_loader --output-on-failure`
  - `cmake --build build-wsl-gcc --target zr_vm_aot_c_shared_library_smoke_test -j 2`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test`
- WSL clang:
  - `cmake -S . -B build-wsl-clang`
  - `cmake --build build-wsl-clang --target zr_vm_aot_c_metadata_binding_loader_test -j 2`
  - `./build-wsl-clang/bin/zr_vm_aot_c_metadata_binding_loader_test`
  - `ctest --test-dir build-wsl-clang -R aot_c_metadata_binding_loader --output-on-failure`
- Windows MSVC Debug:
  - `cmake -S E:\Git\zr_vm -B E:\Git\zr_vm\build-msvc`
  - `cmake --build E:\Git\zr_vm\build-msvc --config Debug --target zr_vm_aot_c_metadata_binding_loader_test -- /m:2`
  - `E:\Git\zr_vm\build-msvc\bin\Debug\zr_vm_aot_c_metadata_binding_loader_test.exe`
  - `ctest --test-dir E:\Git\zr_vm\build-msvc -C Debug -R aot_c_metadata_binding_loader --output-on-failure`

## Results

- WSL gcc：direct loader reject test 1/0；CTest `aot_c_metadata_binding_loader` 1/1；
  existing `zr_vm_aot_c_shared_library_smoke_test` 8/0。
- WSL clang：direct loader reject test 1/0；CTest `aot_c_metadata_binding_loader` 1/1。
- Windows MSVC Debug：target builds; direct test 0 failures / 1 ignored by the existing Unix shared-library branch;
  CTest `aot_c_metadata_binding_loader` 1/1。

## Acceptance Decision

Accepted for 11-S6C dynamic AOT module-load reject scope. The loader now rejects incompatible embedded/zro metadata
bindings before materializing reflection/prototypes or storing the runtime record. typed-boundary deopt, cross-module
token resolve integration, and broader no-crash ABI drift injection remain open 11-S6 work.
