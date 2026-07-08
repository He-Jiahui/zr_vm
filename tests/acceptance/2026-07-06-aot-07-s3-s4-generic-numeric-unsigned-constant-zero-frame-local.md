# AOT 07-S3/S4 generic numeric unsigned constant zero-frame body local

完成时间：2026-07-06 00:58:36 +08:00

状态：完成（子切片）；07-S3/S4 部分完成；07~12 总目标继续进行中。

## 覆盖

- u64 `GET_CONSTANT` 在 proven scalar-local destination 上参与 local-only frame descriptor 判定，和 i64/f64 常量一样不再迫使纯标量函数生成字节帧 body。
- u64 `ADD`/`SUB`/`MUL`/`DIV`/`MOD`、u64 result stack-copy `MUL`、mixed i64/u64 `ADD`/`SUB`/`MUL`/`DIV`/`MOD`、mixed i64/u64 result stack-copy `MUL`、mixed u64/f64 `ADD`/`SUB`/`MUL`/`DIV`/`MOD` 均增加 generated-C zero-frame body 断言。
- generated C 断言要求 `.registerFrameBytes = 0u` 与 `value SemIR lowering frameByteSize=0`，并禁止 `/* zr_aot_generated_frame_setup */`、`ZrAotGeneratedFrame frame = {0};`、`frame.slotBase`、`ZrCore_Function_CheckStackAndGc`、`ZrCore_Value_ResetAsNull` 和 primitive constant value-slot fallback。

## RED

- 先给 u64 `ADD`/`SUB` 增加 zero-frame body 断言后，u64 常量仍因 unsigned constant 未进入 frame descriptor local-only 判定而保留 primitive constant/value-slot fallback。
- 扩展到 u64 `SUB`/`MUL`/`DIV`/`MOD` 后，发现 generic u64 local consumer 只覆盖了 `ADD` 和 `NEG`，导致这些 opcode 的常量虽然有 scalar locals，但仍阻止 frame-free body scan。

## GREEN

- `backend_aot_c_frame_descriptor.c` 接受 `ZR_VALUE_IS_TYPE_UNSIGNED_INT(...)` 常量进入 u64 scalar-local local-only 判定，并把 typed unsigned `ADD_UNSIGNED`/`SUB_UNSIGNED`/`MUL_UNSIGNED`/`DIV_UNSIGNED`/`MOD_UNSIGNED` 纳入 generic numeric local-only opcode gate。
- `backend_aot_c_scalar_locals.c` 将 generic `SUB`/`MUL`/`DIV`/`MOD` 列为 u64 local consumers，并同步覆盖 typed unsigned plain/non-plain opcode 的 u64 binary read proof。
- `tests/parser/test_aot_c_frame_setup_contracts.c` 锁定 unsigned constant gate、typed unsigned opcode gate 与 u64 scalar-local consumer/source markers，防止 frame descriptor 与 scalar-local proof 再次脱节。

## 验证

- WSL GCC：`zr_vm_aot_c_generic_numeric_shared_library_smoke_test` 50/0。
- WSL GCC：`zr_vm_aot_c_frame_setup_contracts_test` 1/0。
- WSL Clang：`zr_vm_aot_c_generic_numeric_shared_library_smoke_test` 50/0。
- WSL Clang：`zr_vm_aot_c_frame_setup_contracts_test` 1/0。
- MSVC Debug：`zr_vm_aot_c_generic_numeric_shared_library_smoke_test` 50 expected ignored / 0 failures。
- MSVC Debug：`zr_vm_aot_c_frame_setup_contracts_test` 1/0。

## 生成物扫描

- `build-wsl-gcc/tests_generated/aot_c_generic_numeric_shared_library/src` 中同时满足 `.registerFrameBytes = 0u` 与 `/* zr_aot_generated_frame_setup */` 的生成文件：0。
- u64/mixed 相关生成文件中的 `/* zr_aot_value_exec_primitive_constant */` fallback：0。

## 剩余

- 本切片只关闭 proven unsigned/generic numeric scalar-local 常量与相关 mixed numeric body 的 zero-frame gate；dynamic/unproven operands、更广 value-copy migration、GC roots/exports/frame cleanup、byte-frame narrowing 剩余形态、性能计数和完整 zero-frame typed bodies 仍待后续。
