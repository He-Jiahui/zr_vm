# AOT 10-S3D / 11-S2D Method Binding Reflection Carrier

## 状态

- 完成时间：2026-06-30 01:54:46 +08:00
- 状态：public `ResolveToken()` method binding carrier 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。

## 完成项目

- `SZrReflectionResolvedToken` 新增 `methodFunctionIndex`、`methodInfo`、`methodFunctionPointer` 和 `methodInvoker`。
- `ZrCore_Reflection_ResolveToken()` 对普通 MethodDef/MethodRef token 继续返回 method record/signature identity。
- 当 attached metadata runtime 存在 11-S2D code-registration method binding 时，public carrier 会复制 MethodInfo、entry thunk 和 invoker。
- 当没有 AOT binding 时，`ResolveToken()` 仍成功返回 method record，binding 字段保持空。
- `tests/module/test_reflection_token_resolve.c` 覆盖有 AOT binding 与无 AOT binding 的 MethodDef token 解析。

## RED/GREEN

- RED：reflection token resolve 测试先要求 public carrier 暴露 method binding 字段，WSL gcc 编译失败，提示 `SZrReflectionResolvedToken` 缺少对应成员。
- GREEN：补齐 carrier 字段与 `reflection_token_resolve.c` 的 11-S2D binding view 消费后，有/无 binding 两种路径均通过。

## 验证

- WSL gcc：
  - `zr_vm_reflection_token_resolve_test` 5/0。
  - `zr_vm_metadata_runtime_method_binding_test` 2/0。
  - `zr_vm_metadata_runtime_query_test` 24/0。
  - CTest `reflection_token_resolve|metadata_runtime_method_binding` 2/2。
- WSL clang：
  - `zr_vm_reflection_token_resolve_test` 5/0。
  - `zr_vm_metadata_runtime_method_binding_test` 2/0。
  - `zr_vm_metadata_runtime_query_test` 24/0。
- Windows MSVC Debug:
  - `zr_vm_reflection_token_resolve_test` 5/0。
  - `zr_vm_metadata_runtime_method_binding_test` 2/0。
  - `zr_vm_metadata_runtime_query_test` 24/0。
  - CTest `reflection_token_resolve|metadata_runtime_method_binding` 2/2。

## 未完成

- 不声明 public method reflection object 已物化。
- 不声明 `Method.Invoke` 参数/返回 marshaling、MethodSpec runtime instance binding、cross-module token rewrite、trim diagnostics 或完整 10-S2/10-S3/11-S2 关闭。
