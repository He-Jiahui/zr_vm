# AOT 07-S2/S4 Bool Constant Frame Elision

时间：2026-06-28 08:54:52 +08:00

状态：子切片完成；07-S2/S4 部分完成，07/M1.5 继续推进。

## 完成项目

- `backend_aot_c_scalar_locals` 新增 bool immediate constant 的 value-slot skip 证明。
- `backend_aot_c_frame_descriptor` 允许可本地化 bool `GET_CONSTANT` 省略 generated frame descriptor。
- `backend_aot_write_c_direct_primitive_constant()` 在可跳过 value slot 时只发
  `zr_aot_scalar_constant_bool_local` 与 `zr_aot_bN = ZR_TRUE|ZR_FALSE`。
- 新增 shared-library smoke 覆盖 `var flag: bool = true; return flag;`，要求直接 bool local return，
  并禁止 frame setup、`ZrAotGeneratedFrame frame`、`frame.slotBase`、`ZrCore_Stack_GetValue`、
  `ZR_VALUE_FAST_SET` 和 `zr_aot_value_exec_primitive_constant`。
- 新增 f64 compare frame-free 执行护栏，覆盖 `left > right`、`left < right`、bool compare
  和 direct bool return。该护栏确认既有 f64 compare 生成路径已满足 07-S2/S4 目标。
- typed bool NOT 生成表达式改为 `((!value) != 0u)`，消除 Clang 对 `!x != 0u` 的警告。

## RED/GREEN

- RED：新增 bool constant return smoke 先失败，生成 C 缺少 `zr_aot_scalar_constant_bool_local`，
  且仍创建 generated frame 并通过 `zr_aot_value_exec_primitive_constant` 写
  `frame.slotBase[..].value.nativeBool`。
- GREEN：补齐 bool constant skip proof、frame descriptor 判定和 primitive constant emitter 后，
  同一 smoke 生成 `zr_aot_scalar_constant_bool_local`、`zr_aot_direct_return_bool_local`，
  且不再出现 frame/value-slot 写。

## 验证

- WSL GCC：
  - `zr_vm_aot_c_shared_library_smoke_test`：13 tests / 0 failures。
  - `zr_vm_aot_c_float_shared_library_smoke_test`：2 tests / 0 failures。
  - `zr_vm_aot_c_source_contracts_test`：21 tests / 0 failures。
  - `zr_vm_aot_c_frame_setup_contracts_test`：1 test / 0 failures。
- WSL Clang：
  - `zr_vm_aot_c_shared_library_smoke_test`：13 tests / 0 failures。
  - `zr_vm_aot_c_float_shared_library_smoke_test`：2 tests / 0 failures。
  - `zr_vm_aot_c_source_contracts_test`：21 tests / 0 failures。
  - `zr_vm_aot_c_frame_setup_contracts_test`：1 test / 0 failures。
- Windows MSVC Debug：
  - `zr_vm_aot_c_source_contracts_test`：21 tests / 0 failures。
  - `zr_vm_aot_c_frame_setup_contracts_test`：1 test / 0 failures。
  - `zr_vm_aot_c_shared_library_smoke_test`：0 failures / 13 ignored Unix-only。
  - `zr_vm_aot_c_float_shared_library_smoke_test`：0 failures / 2 ignored Unix-only。

## 备注

本记录只关闭 bool immediate constant 的 frame-free 窄口径。07-S2/S4 的完整 typed 函数体零
`SZrValue`/frame write、dynamic/generic/string 边界、GC roots/exports/frame cleanup、byte-frame
更广收窄和性能计数仍未完成。
