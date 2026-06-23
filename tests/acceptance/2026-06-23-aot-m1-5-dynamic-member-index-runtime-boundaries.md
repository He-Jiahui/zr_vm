# 2026-06-23 AOT M1.5 07-S5 dynamic member/index runtime boundaries

## 状态

- 时间：2026-06-23 13:49:07 +08:00
- 状态：子切片完成；07-S5 / M1.5 / 07 仍部分完成；08-12 未开始
- 范围：把 AOT 生成器中已知的动态 member/index 访问 opcode 从统一 unsupported stub 改为显式 runtime boundary helper。

## 完成项目

- `backend_aot_c_function_body.c` 将 `GET_MEMBER`、`GET_MEMBER_SLOT`、`GET_BY_INDEX`、`SET_MEMBER`、`SET_MEMBER_SLOT`、`SET_BY_INDEX` 分别路由到专用 writer。
- `backend_aot_c_lowering_values.c` 新增六个直接 writer，生成 `ZrLibrary_AotRuntime_GetMember`、`SetMember`、`GetMemberSlot`、`SetMemberSlot`、`GetByIndex`、`SetByIndex` 调用。
- 生成 C 保留显式 `zr_aot_value_dynamic_*_boundary` 标记，用于锁定这些路径仍是 runtime boundary，而不是内联 VM 帧重建。
- `test_aot_c_global_contracts.c` 反转合同：要求具体 writer、runtime header 声明、function-body 分发，并拒绝这些六个 opcode 继续走 `UnsupportedDynamicValueAccess`。
- `test_aot_c_global_shared_library_smoke.c` 执行六个动态 member/index 访问路径，检查 slot 顺序和 runtime helper 调用文本。
- `test_aot_c_typed_call_contracts.c` 抽出共享读取/断言 helper 到 `tests/parser/aot_c_typed_call_contract_support.h`，让合同文件回到 893 physical / 872 non-empty lines。

## RED / GREEN

- RED：全局合同在 7 个测试中 1 个失败，缺少 `backend_aot_write_c_direct_get_member(FILE *file,`，证明旧实现仍只暴露 unsupported 动态访问边界。
- GREEN： focused WSL GCC debug 构建和测试通过：
  - `zr_vm_aot_c_global_contracts_test`：7/0
  - `zr_vm_aot_c_global_shared_library_smoke_test`：9/0
- 支撑拆分验证：`zr_vm_aot_c_typed_call_contracts_test` 保持 4/0。

## 验证

Broader focused WSL GCC AOT group passed:

- contracts：source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0
- smokes：shared-library 8/0, call 5/0, typed direct-call 5/0, bool 25/0, u64 22/0, f64 18/0, global 9/0, logical 4/0, power 1/0

## 文件规模

- `backend_aot_c_lowering_values.c`：989 physical / 889 non-empty
- `backend_aot_c_function_body.c`：2085 physical / 2037 non-empty
- `backend_aot_c_emitter.h`：805 physical / 800 non-empty
- `test_aot_c_global_contracts.c`：670 physical / 620 non-empty
- `test_aot_c_global_shared_library_smoke.c`：1206 physical / 1048 non-empty
- `test_aot_c_typed_call_contracts.c`：893 physical / 872 non-empty
- `aot_c_typed_call_contract_support.h`：97 physical / 78 non-empty

## 未完成

- 这只完成动态 member/index 六个具体 opcode 的 runtime boundary helper。
- inline structs、`in` / `out` writeback、deopt/dynamic bridges、general typed-return ABI、完整 07-S5 验收和 08-12 仍未完成。
