# AOT 11-S2D / 10-S2 Method Token Binding View

## 状态

- 完成时间：2026-06-30 01:38:26 +08:00
- 状态：runtime 内部 method token 到 MethodInfo/function pointer/invoker binding view 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。

## 完成项目

- `metadata_runtime.h` 新增 `SZrMetadataRuntimeMethodBindingView`。
- 新增 `ZrCore_MetadataRuntime_ReadMethodBindingView(runtime, methodToken, outView)`。
- 新增 `metadata_runtime_method_binding.c`，保持 method binding 逻辑独立于已很大的 metadata runtime 主文件。
- 该入口从 attached `SZrMetadataRuntime` 的 code registration 读取 `methodTokens[]`、`methodInfos[]` 和 `functionPointers[]`。
- 只接受唯一 local `MEMBER_DEF` token，并返回 `functionIndex`、`SZrAotMethodInfo`、entry thunk 和 `methodInfo->invoker`。
- 缺失表、重复 token、非 method token、unknown token、MethodInfo slot 不一致、缺 thunk 或缺 invoker 都返回 false 并清空输出。
- 新增 `tests/module/test_metadata_runtime_method_binding.c` 和 CTest `metadata_runtime_method_binding`。

## RED/GREEN

- RED：新增 focused 测试先要求 `SZrMetadataRuntimeMethodBindingView` 和 `ZrCore_MetadataRuntime_ReadMethodBindingView()`，WSL gcc 编译失败，提示缺少类型和函数声明。
- GREEN：补齐公共 view/API 与实现后，成功绑定和负向防御路径均通过。

## 验证

- WSL gcc：
  - `zr_vm_metadata_runtime_method_binding_test` 2/0。
  - `zr_vm_metadata_runtime_query_test` 24/0。
  - `zr_vm_reflection_token_resolve_test` 4/0。
  - CTest `metadata_runtime_method_binding` 1/1。
- WSL clang：
  - `zr_vm_metadata_runtime_method_binding_test` 2/0。
  - `zr_vm_metadata_runtime_query_test` 24/0。
  - `zr_vm_reflection_token_resolve_test` 4/0。
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_method_binding_test` 2/0。
  - `zr_vm_metadata_runtime_query_test` 24/0。
  - `zr_vm_reflection_token_resolve_test` 4/0。
  - CTest `metadata_runtime_method_binding` 1/1。

## 未完成

- 不声明 public `ZrCore_Reflection_ResolveToken()` 已暴露 MethodInfo/function pointer/invoker。
- 不声明 `Method.Invoke` 参数/返回 marshaling、public method reflection object、MethodSpec runtime instance binding、cross-module token rewrite、trim diagnostics 或完整 10-S2/10-S3/11-S2 关闭。
