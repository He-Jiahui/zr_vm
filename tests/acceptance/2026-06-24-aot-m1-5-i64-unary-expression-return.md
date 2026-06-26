# AOT M1.5 / 07-S5 I64 Unary Expression Return

时间：2026-06-24 06:32:09 +08:00

状态：子切片完成；07-S5 expression direct-return scalarization 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 f64 unary direct return 覆盖后，补齐 signed integer 一元负号直接返回：
  - `var value: int = 7;`
  - `return -value;`
- 目标生成物必须同时满足：
  - `SZrAotSignature.returnType` 非空，`hasReturnValue=1`，signature type row 使用 i64 类型。
  - 返回边界调用 `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_sN)`。
  - `NEG_SIGNED` 结果槽被识别为 i64 scalar-local destination。
  - source constant 只写 `zr_aot_sN`，不生成 `zr_aot_value_exec_primitive_constant` 或 `SZrTypeValue *zr_aot_destination`。
  - 生成 C 通过 scalar i64 unary path 写 `zr_aot_sN`，不回退到 `zr_aot_arith_exec_signed_unary`。
- 本切片不处理 full typed ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridge ABI、完整 07-S5 acceptance 或性能计数器。

## Baseline

- i64 constant local 已能写入 scalar local。
- 但 `backend_aot_write_c_direct_neg_signed(...)` 仍缺少 exec-instruction-index 驱动的 written-before 与 result-skip proof，直接返回 `-value` 时会落回 signed unary runtime helper。
- 补齐 unary result 后，source constant 仍因 `NEG_SIGNED` 未纳入 i64 consumer proof 而保留 value-slot materialization。
- RED generated product 表现为：
  - signature 和 typed return helper 可被识别。
  - unary block 仍含 `zr_aot_arith_exec_signed_unary`。
  - strict zero-materialization check 仍命中 `SZrTypeValue *zr_aot_destination`。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 增加 `assert_script_return_signature_without(...)` helper。
  - 增加 `i64_neg_expr` case。
  - 断言 i64 signature row、`ReturnI64`，并禁止 `zr_aot_arith_exec_signed_unary` 与 `SZrTypeValue *zr_aot_destination`。
- Source contract: `tests/parser/test_aot_c_source_contracts.c`
  - 锁定 `NEG_SIGNED` result opcode -> i64 scalar-local destination。
  - 锁定 `NEG_SIGNED` 进入 signed local consumer、read-set、destination-kind 与 operand-local proof。
  - 锁定 neg-signed writer 接收 `execInstructionIndex` 并在 source 已写、result 可跳过 value slot 时发射 `zr_aot_scalar_exec_i64_unary`。
- Related regression: method-info signature、return contracts、frame setup contracts、source contracts、typed scalar generated/shared-library smoke。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_i64_neg_expr_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：一次 build+run 合并命令在约 124s 工具超时；拆分构建后 method-info signature test 1/1 失败，`i64_neg_expr` 仍命中 forbidden `zr_aot_arith_exec_signed_unary`。

RED strict zero-materialization:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_i64_neg_const_skip_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature test 1/1 失败，`i64_neg_expr` 仍命中 forbidden `SZrTypeValue *zr_aot_destination`。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_i64_neg_const_skip_green_build.log'
./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test
```

结果：补齐 `NEG_SIGNED` signed-consumer proof 后 method-info signature 1/0。

Related build and regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_i64_neg_expr_related_build2.log'
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

Generated i64 unary expression grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "returnType = &zr_aot_signature_0_types\[0\]|hasReturnValue = \(TZrUInt8\)1u|ReturnI64\(state, zr_aot_s|zr_aot_scalar_constant_i64_local|zr_aot_scalar_exec_i64_unary|SZrTypeValue \*zr_aot_destination|zr_aot_value_exec_primitive_constant|zr_aot_arith_exec_signed_unary" build-wsl-gcc/tests_generated/aot_c_method_info_signature/i64_neg_expr/main.c'
```

结果：命中 return type、`hasReturnValue=1`、`zr_aot_scalar_constant_i64_local`、`zr_aot_scalar_exec_i64_unary` 与 `ReturnI64(state, zr_aot_s1)`；未命中 `SZrTypeValue *zr_aot_destination`、`zr_aot_value_exec_primitive_constant` 或 `zr_aot_arith_exec_signed_unary`。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

## Results

- `backend_aot_c_scalar_locals.c` now treats `NEG_SIGNED` as an i64 result opcode and as a signed i64 local consumer.
- `backend_aot_write_c_direct_neg_signed(...)` now receives the exec instruction index and emits a scalar-only unary block when the source i64 local is already written and the destination i64 value slot can be skipped.
- The i64 constant skip proof now sees `NEG_SIGNED` as reading the source i64 local, so `i64_neg_expr` can skip source value-slot materialization.
- Generated `i64_neg_expr` C now keeps the constant in `zr_aot_s0`, the unary result in `zr_aot_s1`, and returns via `ZrLibrary_AotRuntime_ReturnI64`.
- 规模：`backend_aot_c_scalar_locals.c` 3047 physical lines；`backend_aot_c_lowering_typed_arithmetic.c` 1114 physical lines；`backend_aot_c_emitter.h` 826 physical lines；`test_aot_c_method_info_signature.c` 235 physical lines；`test_aot_c_source_contracts.c` 2147 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 i64 unary expression-return scalarization slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
