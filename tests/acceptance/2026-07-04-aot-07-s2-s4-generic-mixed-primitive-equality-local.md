# AOT 07-S2/S4 Generic Mixed Primitive Equality Local Fold

- 时间：2026-07-04 10:22:37 +08:00
- 状态：完成本子切片；07~12 总目标继续进行中。

## 完成项目

- 为两个当前可证明为基础标量 C local、但 kind 不同的 generic
  `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL` 建立本地折叠。
- `LOGICAL_EQUAL` 在 mixed primitive operand 上直接生成 `ZR_FALSE`。
- `LOGICAL_NOT_EQUAL` 在 mixed primitive operand 上直接生成 `ZR_TRUE`。
- 生成 C 包含 `zr_aot_generic_mixed_primitive_compare_scalar_local` marker，并避免目标项目中的
  `ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual` /
  `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual`、bool sync、以及 operand value-slot 读取。
- scalar-local consumer 证明同步识别 mixed primitive operands，并保留同类 operand 快速路径，避免复用过的
  u64/f64 call-result slot 被误判为需要 typed-destination stack sync。
- frame descriptor 将可证明的 mixed primitive equality 标为 local-only。

## RED/GREEN

- RED：新增 mixed primitive equality shared-library smoke 缺少 mixed marker。
- RED：生成 C 没有 `zr_aot_b2 = ZR_FALSE;` 与 `zr_aot_b3 = ZR_TRUE;`。
- RED：源码契约缺少 mixed primitive kind proof、mixed compare emitter、scalar-local consumer 证明和
  frame descriptor local-only 证明。
- GREEN：mixed primitive equality/inequality 直接折叠为 bool local；同类 call-result equality 仍走原有
  u64/f64 scalar-local compare，不回写 typed destination。

## 验证

- WSL GCC：generic equality 5/0、logical contracts 4/0、generic JUMP_IF 9/0、generic LOGICAL_NOT 8/0、
  logical shared-library 6/0、frame setup contracts 1/0、control contracts 2/0、control shared-library 2/0。
- WSL Clang：同组 5/0、4/0、9/0、8/0、6/0、1/0、2/0、2/0。
- Windows MSVC Debug：generic equality 0 failures / 5 expected ignores；generic JUMP_IF
  0 failures / 9 expected ignores；generic LOGICAL_NOT 0 failures / 8 expected ignores；logical shared-library
  0 failures / 6 expected ignores；control shared-library 0 failures / 2 expected ignores；
  logical/frame/control contracts 4/0、1/0、2/0。

## 仍未完成

- value-copy migration、GC roots/exports/frame cleanup、更广 byte-frame narrowing。
- 性能计数、完整 typed 函数体零 `SZrValue`/frame write 证明。
