# AOT 11-S6A Runtime Binding Compatibility

时间：2026-06-27 09:04:36 +08:00

状态：11-S6 支撑子切片完成；完整 11-S6 仍进行中。dynamic 模块加载拒绝、typed 调用边界
deopt、跨模块 token resolve 集成和端到端 ABI drift 注入仍待后续。

## 完成项目

- 新增 `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility()` 作为运行期 ABI 漂移统一判定入口。
- 新增 `EZrMetadataRuntimeBindingCompatibilityStatus`，区分 compatible、invalid argument、module
  version mismatch、module signature hash mismatch、metadata token mismatch、signature token mismatch、
  signature hash mismatch、layout version mismatch、layout hash mismatch。
- 新增 `SZrMetadataRuntimeBindingCompatibilityReport`，保留 expected/actual token、signature hash、
  module signature hash、layout version/hash 和 module version range 指针。
- 版本区间语义与现有 import signature guard 保持一致：actual/min/max 均为合法三段 semver 时要求
  `actual >= min && actual < max`；缺失或旧格式版本按兼容处理。
- 新增 focused module test target 和 CTest：`metadata_runtime_binding_compatibility`。

## RED/GREEN

- RED：`tests/module/test_metadata_runtime_binding_compatibility.c` 引用缺失的 status/report/API 后，
  WSL gcc 构建失败。
- GREEN：实现公共头声明和独立实现文件后，focused 测试覆盖 12 个分支：
  compatible、version mismatch、legacy version compatible、module signature mismatch、metadata token
  mismatch、signature token mismatch、signature hash mismatch、layout version mismatch、layout hash
  mismatch、missing layout side mismatch、AssemblyRef->Module 合法映射、null binding invalid argument。

## 验证

- WSL gcc：`zr_vm_metadata_runtime_binding_compatibility_test` 12/0；CTest
  `metadata_runtime_binding_compatibility` 1/1。
- WSL clang：`zr_vm_metadata_runtime_binding_compatibility_test` 12/0；CTest
  `metadata_runtime_binding_compatibility` 1/1。
- Windows MSVC Debug：`zr_vm_metadata_runtime_binding_compatibility_test.exe` 12/0；CTest
  `metadata_runtime_binding_compatibility` 1/1。

## 备注

本切片只提供可复用 predicate/report 层，不声明 dynamic loader reject、typed boundary deopt、cross-module
token resolve 接线或无崩溃端到端漂移注入完成。
