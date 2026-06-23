# AOT M1.5 / 07-S5 MethodInfo Scalar Return Signatures

时间：2026-06-24 04:38:40 +08:00

状态：子切片完成；07-S5 MethodInfo/signature boundary metadata 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在上一切片已支持 script-level i64 direct return signature 的基础上，补齐 script-level typed-local bool/u64/f64 direct return 的 `SZrAotMethodInfo.signature` 返回元数据。
- 影响层：AOT C emitter、scalar-local return proof、generated-product MethodInfo/signature fixture、frame/return source contracts、CTest registration。
- 本切片只覆盖已有 scalar-local proof 能证明的脚本级 typed local return。表达式直接返回若未形成 scalar local、inline structs、完整 typed ABI、`in`/`out` writeback、deopt/dynamic bridge ABI、性能/SZrValue 计数不在本切片内。

## Baseline

- i64 脚本级 direct return 已能在 callable return metadata 缺失时由 emitter 扫描 `FUNCTION_RETURN` 并推导 signature row。
- bool/u64/f64 的 existing typed-return proof 仍绑定 `function->hasCallableReturnType`，适合 typed callable/native-to-VM 直返，但无法用于脚本入口的 inferred MethodInfo signature。
- 因此脚本级 bool/u64/f64 typed-local return 仍生成 `.returnType = ZR_NULL` 和 `.hasReturnValue = 0`。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 生成 bool/u64/f64 三个脚本入口。
  - 每个脚本先写 typed local，再 `return result;`，匹配 scalar-local direct-return proof 模型。
  - 断言 `.returnType = &zr_aot_signature_0_types[0]`、`.hasReturnValue = 1`，以及 bool/u64/f64 对应 `baseType/staticCType` row。
- CMake/CTest: `tests/CMakeLists.txt`
  - 注册 `zr_vm_aot_c_method_info_signature_test` 和 `aot_c_method_info_signature`。
- Source contracts:
  - `test_aot_c_frame_setup_contracts.c` 锁定 emitter 的通用 scalar return 推断和 bool/u64/f64 helper。
  - `test_aot_c_return_contracts.c` 锁定 typed callable direct-return proof 仍要求 callable return metadata，同时 script signature inference 走无 callable metadata 的 proof helper。
- Related regression: method-info signature、frame setup、source、return、typed scalar。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：focused method-info signature test 1/1 失败，失败点为缺失 `    .returnType = &zr_aot_signature_0_types[0],`。初始 bool fixture 直接返回表达式时仍走 value-frame path，随后收窄为 typed-local return fixture，以覆盖本切片的 scalar-local proof 范围。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_method_info_signature_green_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature 1/0。

Related regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_method_info_signature_related_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test'
```

结果：method-info signature 1/0、frame setup contracts 1/0、source contracts 19/0、return contracts 1/0、typed scalar 1/0。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

Generated signature grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && echo CASE:bool && grep -nE -- "static const SZrAotSignature zr_aot_signature_0|\.returnType = &zr_aot_signature_0_types\[0\]|\.hasReturnValue = \(TZrUInt8\)1u|\.baseType = \(TZrUInt16\)1u|\.staticCType = \(TZrUInt16\)1u" build-wsl-gcc/tests_generated/aot_c_method_info_signature/bool/main.c && echo CASE:u64 && grep -nE -- "static const SZrAotSignature zr_aot_signature_0|\.returnType = &zr_aot_signature_0_types\[0\]|\.hasReturnValue = \(TZrUInt8\)1u|\.baseType = \(TZrUInt16\)9u|\.staticCType = \(TZrUInt16\)9u" build-wsl-gcc/tests_generated/aot_c_method_info_signature/u64/main.c && echo CASE:f64 && grep -nE -- "static const SZrAotSignature zr_aot_signature_0|\.returnType = &zr_aot_signature_0_types\[0\]|\.hasReturnValue = \(TZrUInt8\)1u|\.baseType = \(TZrUInt16\)11u|\.staticCType = \(TZrUInt16\)11u" build-wsl-gcc/tests_generated/aot_c_method_info_signature/f64/main.c'
```

结果：bool 命中 `baseType/staticCType=1`，u64 命中 `9`，f64 命中 `11`，三者均命中 return pointer 与 `hasReturnValue=1`。

## Results

- `backend_aot_c_emitter.c` 现在使用通用 scalar return 推断器，按 i64/bool/u64/f64 顺序扫描所有 `FUNCTION_RETURN`，只有所有返回都能由同一标量 proof 证明时才合成 return signature。
- `backend_aot_c_scalar_locals.c` 新增共享 `can_return_kind_local` proof，direct typed callable return 继续要求 callable return metadata；script signature inference 使用不要求 callable metadata 的 bool/u64/f64 proof helper。
- `tests/parser/test_aot_c_method_info_signature.c` 新增 bool/u64/f64 MethodInfo signature 生成物 fixture，并通过 CTest 注册。
- 规模：`backend_aot_c_emitter.c` 631 physical lines；`backend_aot_c_scalar_locals.c` 2942 physical lines；`backend_aot_c_scalar_locals.h` 75 physical lines；new method-info signature test 140 physical lines；frame setup contract 362 physical lines；return contract 382 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 MethodInfo scalar return signature slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, expression-direct-return scalarization outside current proof scope, complete 07-S5 acceptance, performance/SZrValue runtime counters, and stages 08-12.
