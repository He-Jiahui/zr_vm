# AOT M1.5 / 07-S5 F64 Unary Expression Return

时间：2026-06-24 06:04:14 +08:00

状态：子切片完成；07-S5 expression direct-return scalarization 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 direct binary expression returns 与 f64 comparison -> bool direct return 已覆盖后，补齐 `float` 一元负号直接返回：
  - `var value: float = 1.25;`
  - `return -value;`
- 目标生成物必须同时满足：
  - `SZrAotSignature.returnType` 非空，`hasReturnValue=1`，signature type row 使用 f64 对应 `baseType/staticCType=11`。
  - 返回边界调用 `ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_fN)`。
  - `NEG_FLOAT` 结果槽被识别为 f64 scalar-local destination。
  - 生成 C 通过 scalar f64 unary path 写 `zr_aot_fN`，不回退到 `zr_aot_arith_exec_float_unary` 或 generic frame-slot return。
- 本切片不处理 signed unary direct return、full typed ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridge ABI、完整 07-S5 acceptance 或性能计数器。

## Baseline

- `float` 常量已经能写入 `zr_aot_f0`。
- 但 `NEG_FLOAT` 没有进入 result-opcode f64 destination coverage，且 direct neg-float writer 缺少 skip-value-slot 判定。
- 生成物表现为：
  - MethodInfo signature 缺少 return type。
  - 一元负号块使用 `zr_aot_arith_exec_float_unary` 和 `SZrTypeValue *zr_aot_destination`。
  - return 边界走 generic `ZrLibrary_AotRuntime_Return(state, &frame, ...)`。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 增加 `f64_neg_expr` case。
  - 断言 f64 signature row、`ReturnF64`，并禁止 generic frame-slot return。
- Source contract: `tests/parser/test_aot_c_source_contracts.c`
  - 锁定 `NEG_FLOAT` result opcode -> f64 scalar-local destination。
  - 锁定 neg-float writer 接收 `execInstructionIndex` 并在 source 已写、result 可跳过 value slot 时发射 `zr_aot_scalar_exec_f64_unary`。
- Related regression: method-info signature、return contracts、frame setup contracts、source contracts、typed scalar generated/shared-library smoke。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_f64_neg_expr_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature test 1/1 失败，`f64_neg_expr` 缺失 `.returnType = &zr_aot_signature_0_types[0],`。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_f64_neg_expr_green_build_split.log'
./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test
```

结果：初次 build+run 合并命令在 124s 工具超时；拆分构建后 method-info signature 1/0。

Related build and regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_f64_neg_expr_related_build.log'
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

Generated f64 unary expression grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "returnType = &zr_aot_signature_0_types\[0\]|hasReturnValue = \(TZrUInt8\)1u|baseType = \(TZrUInt16\)11u|staticCType = \(TZrUInt16\)11u|ReturnF64\(state, zr_aot_f|ZrLibrary_AotRuntime_Return\(state, &frame|SZrTypeValue \*zr_aot_destination|zr_aot_scalar_exec_f64_unary|zr_aot_arith_exec_float_unary" build-wsl-gcc/tests_generated/aot_c_method_info_signature/f64_neg_expr/main.c'
```

结果：命中 f64 signature row、return type、`hasReturnValue=1`、`zr_aot_scalar_exec_f64_unary` 与 `ReturnF64(state, zr_aot_f1)`；未命中 generic frame-slot return、`SZrTypeValue *zr_aot_destination` 或 `zr_aot_arith_exec_float_unary`。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

## Results

- `backend_aot_c_scalar_locals.c` now treats `NEG_FLOAT` as an f64 result opcode and f64 scalar-local consumer; `NEG_SIGNED` is also classified as an i64 result opcode for parity, but this acceptance slice only validates f64.
- `backend_aot_write_c_direct_neg_float(...)` now receives the exec instruction index and emits a scalar-only unary block when the source f64 local is already written and the destination f64 value slot can be skipped.
- Generated `f64_neg_expr` C now keeps the unary result in `zr_aot_f1` and returns via `ZrLibrary_AotRuntime_ReturnF64`.
- 规模：`backend_aot_c_scalar_locals.c` 3041 physical lines；`backend_aot_c_lowering_typed_arithmetic.c` 1096 physical lines；`backend_aot_c_emitter.h` 825 physical lines；`test_aot_c_method_info_signature.c` 187 physical lines；`test_aot_c_source_contracts.c` 2135 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 f64 unary expression-return scalarization slice.

Remaining open: signed unary direct return, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
