# AOT 10-S3C / 11-S3 Method Signature Reflection Carrier

## 状态

- 完成时间：2026-06-30 00:31:26 +08:00
- 状态：10-S3 token 驱动方法签名 carrier 子切片完成；完整 10-S3/11-S3 仍未关闭。

## 完成项目

- `SZrReflectionResolvedToken` 新增 `methodSignatureToken`、`methodSignatureRecord` 与 `methodSignatureHash`。
- `ZrCore_Reflection_ResolveToken()` 解析普通 MethodDef/MethodRef token 时复用 `ZrCore_MetadataRuntime_ResolveSignatureRecord()` 填充方法签名身份。
- MethodSpec `SIGNATURE` token 继续走 `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`，并把 MethodSpec token/record/hash 作为 method signature identity 暴露。
- 该 carrier 为后续 token-driven `Invoke`、MethodInfo/Invoker 绑定和 public method reflection object 保留签名单一真相。

## RED/GREEN

- RED：`tests/module/test_reflection_token_resolve.c` 要求 MethodDef 和 MethodSpec resolved token 暴露 `methodSignatureToken`、`methodSignatureRecord`、`methodSignatureHash` 后，WSL gcc 编译失败，因为 `SZrReflectionResolvedToken` 尚无这些字段。
- GREEN：新增 carrier 字段并在 `reflection_token_resolve.c` 填充普通方法签名记录与 MethodSpec 签名身份后，同一测试通过。

## 验证

- WSL gcc：`zr_vm_metadata_runtime_query_test` 24/0，`zr_vm_reflection_token_resolve_test` 4/0，`zr_vm_metadata_runtime_typespec_layout_test` 14/0。
- WSL clang：同三项分别 24/0、4/0、14/0。
- Windows MSVC Debug：同三项分别 24/0、4/0、14/0。
- 备注：gcc/clang 的 computed-goto/unused warnings、MSVC 的 unreachable/unused warnings 为既有警告，本切片未扩大处理范围。

## 未完成

- 不声明 public method reflection object、token-driven `Invoke`、MethodInfo/function pointer 绑定、名表到 token 重写、trim warning/annotation flow、cross-module token rewrite 或完整 10-S3/11-S3 关闭。
