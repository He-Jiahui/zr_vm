# AOT 09-S5B FFI native-call pin scope

时间：2026-06-25 10:56:08 +08:00

## 范围

- 核心 GC 新增 `SZrGcNativeCallPin` 与 `ZrCore_Gc_NativeCallPinObject/Value/Unpin`，为 native call 提供临时 pin/ignore root 边界。
- libffi symbol call 在 `zr_ffi_invoke_native_symbol(...)` 前 pin self object、owner value、每个 argument value，并在 cleanup 反向 unpin。
- FFI callback trampoline 通过 `SZrFunctionStackAnchor` 重新定位 native callback 前保存的 `stackTop`，避免 GC 或栈重分配后用旧裸指针比较。
- 新增 `tests/ffi/test_ffi_native_call_pin_contract.c` 作为 source contract，锁定 pin/unpin 顺序与 callback stack anchor 约束。

## RED / GREEN

- RED：`tests/core/test_aot_gc_root_frame.c` 的 native-call pin contract 无法编译，缺少 `SZrGcNativeCallPin` 与 `ZrCore_Gc_NativeCallPinValue/Unpin` API。
- GREEN 前修正：初版 pin 后测试暴露 temporary ignore root 被 `PinObject` 内部 escape 标记清除；实现改为先 pin，再按需 ignore，并保留调用前已有 ignore root 状态。
- GREEN 前修正：完整 FFI callback path 暴露 raw `stackTop` 在 callback 期间栈重定位后不可靠；callback trampoline 改用 `SZrFunctionStackAnchor` 保存和恢复比较点。

## 验证

- WSL gcc direct：`zr_vm_aot_gc_root_frame_test` 5/0；`zr_vm_ffi_native_call_pin_contract_test` 2/0。
- WSL clang direct：`zr_vm_aot_gc_root_frame_test` 5/0；`zr_vm_ffi_native_call_pin_contract_test` 2/0。
- Windows MSVC Debug direct：`zr_vm_aot_gc_root_frame_test` 5/0；`zr_vm_ffi_native_call_pin_contract_test` 2/0。

## 备注

- 本切片完成 09-S5；09 阶段仍有 09-S4C 编译期写屏障消除未关闭。
- 当前 unpin 清理临时 native pin flag 与 ignore root，不实现 pinned region demotion。
- 完整 `zr_vm_ffi_test` 已覆盖并通过本切片相关 symbol/callback 早期路径，但后续 source-extern `pointer<T>` 未限定类型解析仍是既有基线失败，未并入本切片。
