# AOT 12-S7J Runtime Fallback Warning Source Line

- 时间：2026-06-24 21:04:24 +08:00
- 状态：12-S7 子切片完成；完整 12-S7 仍未关闭
- 完成项目：hybrid runtime fallback trim warning marker 新增 `sourceLine=<debugLine>` 字段，来源为匹配 ExecIR instruction 的 `debugLine`
- RED：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 要求 `sourceLine=41` 后，dynamic call 与 dynamic value-access warning marker 断言失败
- GREEN：`backend_aot_c_runtime_fallback.c` 输出 `sourceLine` 后，dynamic call、dynamic member get、dynamic index get warning marker 均通过
- 验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0；`zr_vm_aot_c_generic_call_typed_test` 6/0；`zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_code_stripping_test` 3/0
- 备注：本切片只传播已有 ExecIR debug line；列号/range、跨调用链依赖路径、warning 抑制、注解数据流、zrp 元数据裁剪与 release 符号剥离仍待后续。
