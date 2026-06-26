# AOT M1.5 / 07-S5 I64 Bit-Not Expression Return Coverage

时间：2026-06-24 06:42:01 +08:00

状态：覆盖子切片完成；07-S5 expression direct-return scalarization 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 signed integer unary direct return 覆盖后，确认 signed integer 位取反直接返回：
  - `var value: int = 7;`
  - `return ~value;`
- 目标生成物必须同时满足：
  - `SZrAotSignature.returnType` 非空，`hasReturnValue=1`，signature type row 使用 i64 类型。
  - 返回边界调用 `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_sN)`。
  - 位取反结果通过 scalar i64 bit-not path 写 `zr_aot_sN`。
  - 生成 C 不回退到旧 `zr_aot_bitwise_exec_unary` 或 expression-result `SZrTypeValue *zr_aot_destination`。
- 本切片不处理 full typed ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridge ABI、完整 07-S5 acceptance 或性能计数器。

## Baseline

- 07-S2 已有 i64 bit-not result-local proof，`backend_aot_write_c_scalar_i64_bit_not()` 可在 source 已写且 result value slot 可跳过时只写 scalar local。
- 但 07-S5 expression direct-return 测试矩阵尚未锁定 script-level `return ~value;` 是否继续保持 i64 MethodInfo signature、typed return helper 和 no value-slot fallback。
- 本次新增 probe 没有暴露生产代码缺口；既有 scalar SemIR bit-not 路径已覆盖该直接返回形态。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 增加 `i64_bit_not_expr` case。
  - 断言 i64 signature row、`ReturnI64`，并禁止 `zr_aot_bitwise_exec_unary` 与 `SZrTypeValue *zr_aot_destination`。
- Related regression: method-info signature executable and registered CTest。

## Tooling Evidence

Probe build:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_bit_not_expr_probe_build.log'
```

结果：目标构建通过。一次 build+run 合并命令超过工具等待窗口；最终结果以拆分后的构建、测试和 CTest 为准。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：method-info signature 1/0；CTest `aot_c_method_info_signature` 1/1 passed。

Generated i64 bit-not expression grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "zr_aot_bitwise_exec_unary|SZrTypeValue \*zr_aot_destination|zr_aot_value_exec_primitive_constant|ZrLibrary_AotRuntime_ReturnI64" build-wsl-gcc/tests_generated/aot_c_method_info_signature/i64_bit_not_expr/main.c'
```

结果：只命中 `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s1)`；未命中 `zr_aot_bitwise_exec_unary`、`SZrTypeValue *zr_aot_destination` 或 `zr_aot_value_exec_primitive_constant`。

Generated scalar path grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "scalar|ReturnI64|destination|bitwise|primitive_constant|value_exec|slotBase" build-wsl-gcc/tests_generated/aot_c_method_info_signature/i64_bit_not_expr/main.c'
```

结果：命中 `zr_aot_scalar_constant_i64_local`、`zr_aot_scalar_stack_copy_i64`、`zr_aot_scalar_exec_i64_bit_not` 和 `ReturnI64(state, zr_aot_s1)`；未命中 forbidden value-slot fallback。

## Results

- `i64_bit_not_expr` now locks script-level `return ~value;` as an i64 scalar expression direct-return case.
- No production code change was required; the existing `BITWISE_NOT` scalar-local/result-local proof already emits the scalar-only bit-not block for this generated product.
- Generated `i64_bit_not_expr` C keeps the value in scalar locals and returns through `ZrLibrary_AotRuntime_ReturnI64`.
- 规模：`test_aot_c_method_info_signature.c` 243 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 i64 bit-not expression-return coverage slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
