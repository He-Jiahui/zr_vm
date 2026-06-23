# AOT M1.5 / 07-S5 MethodInfo Typed Return Signature

时间：2026-06-24 04:00:05 +08:00

状态：子切片完成；07-S5 MethodInfo/signature boundary metadata 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 让 script-level typed scalar i64 return 在 generated `SZrAotMethodInfo.signature` 中暴露非空 return metadata。
- 影响层：AOT C emitter、AOT generated-product contract、frame setup source contract。
- 本切片只覆盖已能证明 direct-return 的 i64 scalar local；bool/u64/f64 无 callable return metadata 的脚本级 return、inline structs、完整 typed ABI、`in`/`out` writeback、deopt/dynamic bridge ABI 和性能/SZrValue 计数不在本切片内。

## Baseline

- `tests/parser/test_aot_c_typed_scalar.c` 已要求 `static const SZrAotMethodInfo zr_aot_method_info_0` 和 `SZrAotSignature` 存在。
- 但 generated C 对该脚本级 i64 return 仍生成 `.returnType = ZR_NULL` 和 `.hasReturnValue = 0`，因为 emitter 只从 `function->hasCallableReturnType` 推导 signature return type。
- 现有 typed return lowering 已能通过 `backend_aot_c_scalar_locals_can_direct_return_i64_local(...)` 证明返回值是 i64 scalar local。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_typed_scalar.c`
  - 断言 `.returnType = &zr_aot_signature_0_types[0]`
  - 断言 `.hasReturnValue = (TZrUInt8)1u`
  - 断言 return signature type row 为 i64：`baseType=5`、`staticCType=5`
- Source contract: `tests/parser/test_aot_c_frame_setup_contracts.c`
  - 锁定 emitter 通过 `functionIr` 推导 i64 direct-return signature。
  - 锁定 `FUNCTION_RETURN` scan 和 `backend_aot_c_scalar_locals_can_direct_return_i64_local(...)` proof。
- Related regression: source contracts、frame setup contracts、return contracts、typed scalar generated/shared-library smoke。
- Boundary grep: 07 §9 environment token grep 与 07 §9 double-write/SZrValue token grep。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_signature_return_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test'
```

结果：`zr_vm_aot_c_typed_scalar_test` 1 项中 1 项失败，失败点为缺失 `    .returnType = &zr_aot_signature_0_types[0],`。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_signature_return_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test'
```

结果：typed scalar 1/0。

Related regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test >/tmp/zr_aot_signature_related_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test'
```

结果：frame setup contracts 1/0、source contracts 19/0、return contracts 1/0、typed scalar 1/0。

Boundary grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && if grep -nE -- "zr_aot_begin_instruction|frame\.slotBase|state->stackTop|programCounter|publishAllInstructions|Debug_Hook" build-wsl-gcc/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c; then exit 1; fi'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && if grep -nE -- "SZrTypeValue|->ownershipKind|ZR_VALUE_FAST_SET|ZrCore_Ownership_ReleaseValue" build-wsl-gcc/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c; then exit 1; fi'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE -- "static const SZrAotSignature zr_aot_signature_0|\.returnType = &zr_aot_signature_0_types\[0\]|\.hasReturnValue = \(TZrUInt8\)1u|\.baseType = \(TZrUInt16\)5u|\.staticCType = \(TZrUInt16\)5u" build-wsl-gcc/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c'
```

结果：环境符号 grep 无输出；SZrValue/double-write grep 无输出；signature grep 命中 i64 return metadata。

## Results

- `backend_aot_c_emitter.c` 现在在 callable return metadata 缺失时，扫描 `FUNCTION_RETURN` 并使用 scalar-local direct-return proof 推导 i64 return signature。
- 所有 `FUNCTION_RETURN` 都必须满足 i64 direct-return proof 才推导 return metadata；混合或未知 return 保持 `ZR_NULL`。
- Generated typed scalar C 现在包含非空 return signature pointer、`hasReturnValue=1` 和 i64 signature row。
- 规模：`backend_aot_c_emitter.c` 550 physical lines；`test_aot_c_typed_scalar.c` 1101 physical lines；`test_aot_c_frame_setup_contracts.c` 344 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 metadata slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, bool/u64/f64 script-level inferred signatures, complete 07-S5 acceptance, performance/SZrValue runtime counters, and stages 08-12.
