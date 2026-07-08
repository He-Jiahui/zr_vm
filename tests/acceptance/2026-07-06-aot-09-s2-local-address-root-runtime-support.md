# AOT 09-S2 LOCAL_ADDRESS Root Runtime Support

时间：2026-07-06 04:56:47 +08:00

## Scope

完成 09-S2/07-S6 的一个可选运行期子切片：GC AOT root-frame visitor 支持
`ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS`，让注册的 C local raw-object root 能在 minor GC 中被标记并重写。

07~12 总目标未完成；生成器的 LOCAL_ADDRESS root map 发射、GC 压力/root 正确性更广覆盖、exports/frame cleanup、
in/out writeback、性能计数和完整验收仍待后续。

## Baseline

既有 AOT root-frame 只处理 `ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET`，从 `frameBase + frameByteOffset`
解析 `SZrTypeValue` 栈 slot。新增测试注册 `LOCAL_ADDRESS` root 后，旧实现会跳过该 root，minor GC 后 local
raw-object pointer 不能被证明迁移到 survivor。

## Test Inventory

- `tests/core/test_aot_gc_root_frame.c`
  - `test_aot_root_frame_local_address_keeps_young_raw_object_live`

## RED

- WSL GCC `zr_vm_aot_gc_root_frame_test`：6 tests / 1 failure。
- 失败点：local-address raw-object root 仍停留在未搬迁/未 survivor 的旧对象状态，证明旧 GC visitor 未处理
  `LOCAL_ADDRESS`。

## GREEN

- `gc_mark.c`：AOT root-frame mark 按 `locationKind` 分派；`LOCAL_ADDRESS` 解析为 `SZrRawObject **` 并标记非空对象。
- `gc_cycle.c`：AOT root-frame rewrite 按 `locationKind` 分派；`LOCAL_ADDRESS` 复用 raw-object slot forwarding rewrite。
- `FRAME_BYTE_OFFSET` 的 `SZrTypeValue` 栈根路径、栈边界校验和 value rewrite 行为保持不变。

## Tooling Evidence

- WSL GCC：`zr_vm_aot_gc_root_frame_test` 6/0。
- WSL Clang：`zr_vm_aot_gc_root_frame_test` 6/0。
- Windows MSVC Debug：`zr_vm_aot_gc_root_frame_test` 6/0。

## Acceptance Decision

接受。运行期已能处理 ABI 预留的 local-address root slot；本验收不声明生成器已能发射 LOCAL_ADDRESS root map。
