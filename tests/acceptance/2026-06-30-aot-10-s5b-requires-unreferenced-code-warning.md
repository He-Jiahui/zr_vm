# AOT 10-S5B / 12-S5B RequiresUnreferencedCode Static Call Warning

时间：2026-06-30 11:59:59 +08:00

## 状态

- 10-S5 / 12-S5 的 `@requires_unreferenced_code` 调用点 warning 子切片完成。
- 完整 `@dynamically_accessed` 数据流、`@dynamic_dependency`、warning 抑制/提升、用户 reason 字符串透传、未注解反射 warning 与完整 trim analyzer 仍未完成。

## 完成项目

- 复用现有 compile-time decorator metadata 作为计划级 `@requires_unreferenced_code` 的首个承载面。
- 新增 `backend_aot_c_annotation_warnings.{h,c}`，独立扫描 retained function table 中的静态 call 指令。
- `backend_aot_resolve_callable_slot_function_index_before_instruction(...)` 现在可把 `GET_SUB_FUNCTION` 写入的 callable slot 解析为原始 flat function index，使子函数静态调用可被 warning 扫描复用。
- AOT C emitter 在 `enableCodeStripping` 下输出 `trim_warnings.annotationCount`，并为静态调用到 `requiresUnreferencedCode: true` callee 的 retained caller 输出 `trim_warning.annotation[index] function=<flatIndex> instruction=<index> targetFunction=<flatIndex> reason=requires-unreferenced-code`。
- annotation warning 与既有 `trim_warnings.runtimeFallback*` 计数/原因掩码分离；未标注静态 callee 保持 annotation count 为 0。

## RED / GREEN

- RED：扩展 `zr_vm_aot_c_reflection_annotation_preserve_test` 后，带 `requiresUnreferencedCode: true` callee 的静态调用生成 C 缺少 `trim_warnings.annotationCount = 1` 与逐条 `trim_warning.annotation[0]` marker。
- GREEN：新增 annotation warning scanner 与 `GET_SUB_FUNCTION` callable provenance 后，标注 callee 输出 1 条 annotation warning；未标注 callee 输出 `trim_warnings.annotationCount = 0` 且没有逐条 annotation warning。

## 验证

- WSL gcc：`zr_vm_aot_c_reflection_annotation_preserve_test` 4/0。
- WSL gcc：CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3。
- WSL gcc：`zr_vm_aot_c_call_shared_library_smoke_test` 5/0，`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 7/0。
- WSL clang：重新配置后同组 CTest 3/3；call shared-library smoke 5/0；dynamic deopt bridge smoke 7/0。
- Windows MSVC Debug：重新配置后同组 CTest 3/3；call shared-library smoke 0 failures / 5 ignored；dynamic deopt bridge smoke 0 failures / 7 ignored。
- WSL gcc / clang：`zr_vm_aot_c_source_contracts_test` 22/0。
- 行数检查：`backend_aot_c_emitter.c` 964 行，`backend_aot_c_annotation_warnings.c` 187 行，`backend_aot_callable_provenance.c` 264 行，`test_aot_c_reflection_annotation_preserve.c` 356 行。

## 备注

该切片只覆盖 retained static caller -> annotated static callee 的 compile-time warning marker。它不声明动态反射数据流、跨模块 annotation、用户 reason 字符串格式、warning suppression policy、被裁剪目标恢复策略、runtime fallback warning 行为变更或完整 metadata sweep 完成。
