# AOT M1.5 / 07-S5 F64 Comparison Expression Return

时间：2026-06-24 05:50:48 +08:00

状态：子切片完成；07-S5 expression direct-return scalarization 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 bool/u64/f64 typed-local return 与 int/u64/f64 direct expression return 已转为 typed return helper 的基础上，补齐 `float` 比较产生 `bool` 后直接返回：
  - `var left: float = 2.5;`
  - `var right: float = 1.25;`
  - `return left > right;`
- 目标生成物必须同时满足：
  - `SZrAotSignature.returnType` 非空，`hasReturnValue=1`，signature type row 使用 bool 对应 `baseType/staticCType=1`。
  - 返回边界调用 `ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_bN)`。
  - `LOGICAL_*_FLOAT` 比较可被 scalar-local 分析识别为读取两个 f64 source、写入 bool destination。
  - 生成 C 通过 `zr_aot_scalar_exec_f64_compare` 写 `zr_aot_bN`，不回退到 `zr_aot_compare_exec_float` 或 generic frame-slot return。
- 本切片不处理 full typed ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridge ABI、完整 07-S5 acceptance 或性能计数器。

## Baseline

- 上一 expression direct-return 子切片覆盖了：
  - i64 comparison -> bool direct return。
  - u64 arithmetic -> u64 direct return。
  - f64 arithmetic -> f64 direct return。
- 但 f64 comparison -> bool 没有进入 result-opcode bool destination coverage，也没有 scalar f64 compare writer；生成物仍表现为：
  - MethodInfo signature 缺少 return type。
  - 比较块使用 `zr_aot_compare_exec_float` 和 `SZrTypeValue *zr_aot_destination`。
  - return 边界走 generic `ZrLibrary_AotRuntime_Return(state, &frame, ...)`。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 增加 `f64_bool_expr` case。
  - 断言 bool signature row、`ReturnBool`，并禁止 generic frame-slot return。
- Source contract: `tests/parser/test_aot_c_source_contracts.c`
  - 锁定 `LOGICAL_*_FLOAT` result opcode -> bool scalar-local destination。
  - 锁定 f64 local consumer 对 float compare source reads 的识别。
  - 锁定 `backend_aot_write_c_scalar_f64_compare(...)` 与 `zr_aot_b%u = (TZrBool)(zr_aot_f%u %s zr_aot_f%u);` 生成形态。
- Related regression: method-info signature、return contracts、frame setup contracts、source contracts、typed scalar generated/shared-library smoke。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_f64_bool_expr_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature test 1/1 失败，`f64_bool_expr` 缺失 `.returnType = &zr_aot_signature_0_types[0],`。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_f64_bool_expr_green_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature 1/0。

Related build and regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_f64_bool_expr_related_build.log'
```

结果：相关目标构建通过。

```text
./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test
./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test
```

结果：method-info signature 1/0、return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0。

Generated f64 bool expression grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "returnType = &zr_aot_signature_0_types\[0\]|hasReturnValue = \(TZrUInt8\)1u|baseType = \(TZrUInt16\)1u|staticCType = \(TZrUInt16\)1u|ReturnBool\(state, zr_aot_b|ZrLibrary_AotRuntime_Return\(state, &frame|SZrTypeValue \*zr_aot_destination|zr_aot_scalar_exec_f64_compare|zr_aot_compare_exec_float" build-wsl-gcc/tests_generated/aot_c_method_info_signature/f64_bool_expr/main.c'
```

结果：命中 bool signature row、return type、`hasReturnValue=1`、`zr_aot_scalar_exec_f64_compare` 与 `ReturnBool(state, zr_aot_b2)`；未命中 generic frame-slot return、`SZrTypeValue *zr_aot_destination` 或 `zr_aot_compare_exec_float`。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

## Results

- `backend_aot_c_scalar_locals.c` now treats `LOGICAL_EQUAL_FLOAT`, `LOGICAL_NOT_EQUAL_FLOAT`, `LOGICAL_LESS_FLOAT`, `LOGICAL_LESS_EQUAL_FLOAT`, `LOGICAL_GREATER_FLOAT`, and `LOGICAL_GREATER_EQUAL_FLOAT` as bool result opcodes.
- The f64 scalar-local consumer proof now recognizes float comparison instructions as reading f64 left/right slots while writing a bool destination slot.
- `backend_aot_c_scalar_semir.c` now has a scalar f64 compare route parallel to the existing i64/u64 compare routes.
- Generated `f64_bool_expr` C now keeps the comparison result in `zr_aot_b2` and returns via `ZrLibrary_AotRuntime_ReturnBool`.
- 规模：`backend_aot_c_scalar_locals.c` 3032 physical lines；`backend_aot_c_scalar_semir.c` 842 physical lines；`test_aot_c_method_info_signature.c` 181 physical lines；`test_aot_c_source_contracts.c` 2130 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 f64 comparison expression-return scalarization slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
