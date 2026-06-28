# AOT 07-S2/S4 unsigned mixed-literal conversion frame elision

时间：2026-06-28 07:01:28 +08:00

状态：子切片完成。07-S2/S4 部分完成，07/M1.5 继续推进；08-12 仍按各自计划继续。

## 完成项目

- 新增 shared-library smoke：
  - `test_aot_c_generated_shared_library_elides_frame_for_unsigned_const_scalar_pipeline`
  - `test_aot_c_generated_shared_library_elides_frame_for_unsigned_to_signed_conversion_pipeline`
  - `test_aot_c_generated_shared_library_elides_frame_for_unsigned_mixed_literal_pipeline`
- unsigned const scalar pipeline 生成、编译、执行，并返回 `uint` 结果 1。
- explicit unsigned-to-signed conversion pipeline 生成、编译、执行，并返回 `int` 结果 15。
- unsigned mixed-literal pipeline 覆盖 `uint seed + int literal` 降级后的 u64 copy、generic `TO_INT`、i64 const arithmetic、direct return，并返回 `int` 结果 42。
- generated C 约束：
  - 必须包含 scalar marker：`zr_aot_scalar_exec_u64_binary`、`zr_aot_scalar_exec_to_i64`、`zr_aot_scalar_exec_i64_binary`、`zr_aot_direct_return_u64_local` / `zr_aot_direct_return_i64_local`。
  - 禁止出现 `/* zr_aot_generated_frame_setup */`、`ZrAotGeneratedFrame frame = {0};`、`ZrCore_Stack_GetValue(`、`ZR_VALUE_FAST_SET(`。
- `backend_aot_write_c_scalar_to_i64()` 现在让 generic `TO_INT` 支持已写入的 u64/i64/f64/bool scalar source，并继续使用范围保持的 u64->i64 转换。
- `backend_aot_c_frame_descriptor_conversion_can_use_local_only()` 将 `TO_INT` / `TO_UINT` / `TO_FLOAT` 转换族纳入 descriptor-free 证明。
- `backend_aot_c_scalar_stack_copy_can_use_local_only()` 与正文 stack-copy 发射器对齐，目的槽静态类型不可用时会尝试 source static/local 类型，修复 mixed-literal 早期 stack copy 的过度保守 frame 判定。
- `test_aot_c_frame_setup_contracts.c` 增加 conversion descriptor 与 source-type stack-copy fallback 的源码契约。

## RED/GREEN

- RED：`test_aot_c_generated_shared_library_elides_frame_for_unsigned_mixed_literal_pipeline` 失败，生成物仍包含 `ZrAotGeneratedFrame frame` 与 `zr_aot_generated_frame_setup`。
- 诊断：临时 frame-descriptor reject 日志定位到 `GET_STACK` stack copy `dst=2 src=0`，正文已能生成 `zr_aot_u2 = zr_aot_u0`，但 descriptor 端只按目的槽类型判定，没有走 source-type fallback。
- GREEN：对齐 stack-copy descriptor 判定，并补齐 scalar conversion descriptor proof 后，mixed-literal 生成物不再声明 frame，u64 copy、u64->i64 conversion、i64 arithmetic 和 return 全部保持 scalar-local 路径。

## 验证

- WSL gcc `build-wsl-gcc`：
  - `zr_vm_aot_c_frame_setup_contracts_test`：1 tests / 0 failures。
  - `zr_vm_aot_c_shared_library_smoke_test`：12 tests / 0 failures。
- WSL clang `build-wsl-clang`：
  - `zr_vm_aot_c_frame_setup_contracts_test`：1 tests / 0 failures。
  - `zr_vm_aot_c_shared_library_smoke_test`：12 tests / 0 failures。
  - 仍有既有 generated bool `!x != 0u` clang warning，非本切片新增失败。
- Windows MSVC Debug `build-msvc`：
  - `zr_vm_aot_c_frame_setup_contracts_test.exe`：1 tests / 0 failures。
  - `zr_vm_aot_c_shared_library_smoke_test.exe`：12 tests / 0 failures / 12 ignored Unix-only。
- `git diff --check`：touched files 通过，仅仓库行尾 LF/CRLF 提示。

## 剩余事项

本切片不声明 07-S2/S4 全量完成。f64 const/reset、load-const/load-stack const、dynamic/generic/string 边界、GC roots/exports/frame cleanup、byte-frame 更广收窄、性能计数和完整 typed 函数体零 `SZrValue`/frame write 仍待后续切片。
