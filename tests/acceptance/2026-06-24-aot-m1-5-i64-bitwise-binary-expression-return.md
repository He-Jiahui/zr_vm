# AOT M1.5 / 07-S5 I64 Bitwise Binary Expression Return Coverage

时间：2026-06-24 06:48:00 +08:00

状态：覆盖子切片完成；07-S5 expression direct-return scalarization 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 signed integer bit-not direct return 覆盖后，确认 signed integer 二元位运算直接返回：
  - `var left: int = 58;`
  - `var right: int = 47;`
  - `return left & right;`
- 目标生成物必须同时满足：
  - `SZrAotSignature.returnType` 非空，`hasReturnValue=1`，signature type row 使用 i64 类型。
  - 返回边界调用 `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_sN)`。
  - 二元位运算结果通过 scalar i64 bitwise path 写 `zr_aot_sN`。
  - 生成 C 不回退到旧 `zr_aot_bitwise_exec_binary` 或 expression-result `SZrTypeValue *zr_aot_destination`。
- 本切片不处理 full typed ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridge ABI、完整 07-S5 acceptance 或性能计数器。

## Baseline

- 07-S2 已有 i64 bitwise result-local proof，`backend_aot_write_c_scalar_i64_bitwise()` 可在左右源都已写且 result value slot 可跳过时只写 scalar local。
- 但 07-S5 expression direct-return 测试矩阵尚未锁定 script-level `return left & right;` 是否继续保持 i64 MethodInfo signature、typed return helper 和 no value-slot fallback。
- 本次新增 probe 没有暴露生产代码缺口；既有 scalar SemIR bitwise path 已覆盖该直接返回形态。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 增加 `i64_bitwise_and_expr` case。
  - 断言 i64 signature row、`ReturnI64`，并禁止 `zr_aot_bitwise_exec_binary` 与 `SZrTypeValue *zr_aot_destination`。
- Related regression: method-info signature executable and registered CTest。

## Tooling Evidence

Probe and GREEN:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_i64_bitwise_expr_probe_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature 1/0。

Generated i64 bitwise expression grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "zr_aot_bitwise_exec_binary|SZrTypeValue \*zr_aot_destination|zr_aot_value_exec_primitive_constant|ZrLibrary_AotRuntime_ReturnI64|zr_aot_scalar_exec_i64_bitwise" build-wsl-gcc/tests_generated/aot_c_method_info_signature/i64_bitwise_and_expr/main.c'
```

结果：命中 `zr_aot_scalar_exec_i64_bitwise` 与 `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s2)`；未命中 `zr_aot_bitwise_exec_binary`、`SZrTypeValue *zr_aot_destination` 或 `zr_aot_value_exec_primitive_constant`。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

## Results

- `i64_bitwise_and_expr` now locks script-level `return left & right;` as an i64 scalar expression direct-return case.
- No production code change was required; the existing scalar-local/result-local bitwise proof already emits the scalar-only bitwise block for this generated product.
- Generated `i64_bitwise_and_expr` C keeps both operands and the result in scalar locals and returns through `ZrLibrary_AotRuntime_ReturnI64`.
- 规模：`test_aot_c_method_info_signature.c` 252 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 i64 bitwise binary expression-return coverage slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
