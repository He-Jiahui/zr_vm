# AOT 07-S2/S4 f64 mod frame-elision execution guardrail

时间：2026-06-28 07:09:58 +08:00

状态：验收护栏子切片完成。07-S2/S4 部分完成，07/M1.5 继续推进；08-12 仍按各自计划继续。

## 完成项目

- `test_aot_c_generated_shared_library_compiles_typed_float_mod` 从生成/编译 smoke 升级为项目级 AOT runtime 执行 smoke。
- 测试现在写入 `.zrp`、`.zr`、`.zro`，用 binary input hash 生成 AOT C，共享库编译后通过 `ZrLibrary_AotRuntime_ExecuteEntry()` 执行入口。
- 覆盖源：
  ```zr
  var left: float = 7.5;
  var right: float = 2.0;
  return left % right;
  ```
- 执行结果要求：
  - 返回值类型为 float/double。
  - 返回值为 1.5。
  - runtime 标记为 `ZR_LIBRARY_EXECUTED_VIA_AOT_C`。
- generated C 约束：
  - 必须包含 `zr_aot_scalar_exec_f64_binary`、`fmod(`、`zr_aot_direct_return_f64_local`。
  - 禁止 `/* zr_aot_generated_frame_setup */`、`ZrAotGeneratedFrame frame = {0};`、`ZrCore_Stack_GetValue(`、`ZR_VALUE_FAST_SET(`。
  - 禁止旧 float modulo runtime wrapper：`ZrLibrary_AotRuntime_ModFloat(state, &frame)`。

## RED/GREEN

- RED 线索：收紧断言前的旧生成物仍显示 `ZrAotGeneratedFrame frame`、`zr_aot_generated_frame_setup` 与 `frame.slotBase[2]` result materialization。
- GREEN：在当前代码重新生成后，f64 modulo 生成物直接满足 frame-free 约束，且项目级 AOT runtime 执行返回 1.5。
- 本切片未改生产代码，只把 07-S2/S4 已实现的 f64 modulo frame-elision 行为补成产品级执行验收。

## 验证

- WSL gcc `build-wsl-gcc`：
  - `zr_vm_aot_c_float_shared_library_smoke_test`：1 tests / 0 failures。
- WSL clang `build-wsl-clang`：
  - `zr_vm_aot_c_float_shared_library_smoke_test`：1 tests / 0 failures。
- Windows MSVC Debug `build-msvc`：
  - `zr_vm_aot_c_float_shared_library_smoke_test.exe`：0 failures / 1 ignored Unix-only。
- `git diff --check`：touched files 通过，仅仓库行尾 LF/CRLF 提示。

## 剩余事项

本切片不声明 07-S2/S4 全量完成。f64 更广算术/比较、load-const/load-stack const、dynamic/generic/string 边界、GC roots/exports/frame cleanup、byte-frame 更广收窄、性能计数和完整 typed 函数体零 `SZrValue`/frame write 仍待后续切片。
