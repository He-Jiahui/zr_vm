# AOT M1.5 / 07-S2 i64 Direct Return Local-Only Slice

时间：2026-06-21 07:51:00 +08:00

状态：子切片完成；07-S2 部分完成；M1.5/07 部分完成；08-12 未开始。

完成项目：
- focused typed scalar 的两个 i64 return sites (`slot 23` 与 `slot 48`) 现在通过 `zr_aot_direct_return_i64_local` 直接把 `zr_aot_s23` / `zr_aot_s48` 写入 caller result value。
- `backend_aot_write_c_direct_return_i64_local()` 不再委托 frame-backed direct return，也不再生成 `zr_aot_result_slot = frame.slotBase + N` / `zr_aot_result_value` 读取路径。
- `backend_aot_c_scalar_locals_can_direct_return_i64_local()` 集中校验 i64 local direct-return 的保守条件，并允许 i64 result-skip proof 在可由 local direct return 消费的 `FUNCTION_RETURN` 边界停止。
- focused generated C 中 return 前的 `slot 23` / `slot 48` result materialization 被删除；返回块直接设置 `ZR_VALUE_TYPE_INT64` 和 `nativeInt64 = zr_aot_sN`。
- WSL shared parser rebuild 暴露了既有 untracked `cfg_throw_profile.c` 缺少 `cfg_node_throw_kind_mask()` 前置声明的 C11 编译错误；补最小 forward declaration 后验证链路可继续。

RED/GREEN：
- RED：`zr_vm_aot_c_typed_scalar_test` 先因缺少 `/* zr_aot_direct_return_i64_local */` 失败 1/1。
- GREEN：实现 i64 local direct-return emitter、return 边界 result-skip 证明和 focused generated-product 断言后，`zr_vm_aot_c_typed_scalar_test` 通过 1/0。

验证：
- `zr_vm_aot_c_typed_scalar_test`：1 test / 0 failures。
- 生成 C 检查：存在 `zr_aot_direct_return_i64_local`；存在 `nativeInt64 = zr_aot_s23` 和 `nativeInt64 = zr_aot_s48`；不存在 `zr_aot_result_slot = frame.slotBase + 23/48`；不存在 return 前 `frame.slotBase[23/48].value` result materialization。
- `zr_vm_aot_c_source_contracts_test`：19 tests / 0 failures。
- `ctest -R 'aot_c_typed_scalar' --output-on-failure`：1/1 passed。
- `git diff --check`：退出 0，仅报告仓库既有 LF/CRLF 提示。

剩余风险 / 未完成：
- 07-S2 仍未完成；reset-stack-null frame writes、prologue/frame setup、边界 local restoration、generic float copy/type checks 和完整 typed boundary marshaling 仍需后续切片收敛。
- `backend_aot_c_scalar_locals.c` 已超过 1800 行；后续最小拆分边界仍建议放在 scalar result-skip/liveness proof 模块。
