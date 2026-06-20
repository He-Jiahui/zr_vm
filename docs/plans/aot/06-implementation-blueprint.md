---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 分阶段落地 AOT 纯 C 降级 / 指令集与数据模型 C# 化
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - tests/core/test_type_layout_inline_copy.c
  - tests/parser/test_compiler_features.c
  - tests/acceptance
---

# 06 · 分阶段路线图、里程碑、测试矩阵与验收

把 02–05 的切片编排为可执行的里程碑序列。原则：每个里程碑都以**解释器与 AOT 结果逐位一致**
为门槛，先 typed 标量、再 struct、再调用/所有权/GC，最后取代旧 ABI。

## 1. 里程碑序列

### M0 · 基线与护栏（前置）
- 建立黄金测试框架：同一 typed 函数分别走「解释器 typed-SemIR」与「AOT 生成 C」，逐位对比结果。
- 建立 CI 检查：typed C 产物对 VM 调用白名单的 grep 校验（`04` §4/§7）。
- 产出：现状基线快照 + 对比/校验脚手架。**无行为改动。**

### M1 · 类型确定性地基（02-S1/S2, 03-S1/S2/S4）
- `SZrTypeLayout` 扩展（blittable/cTypeId/gc&ownership 偏移表）+ 布局计算唯一入口。
- 生成 `struct ZrLayout_*` + `offsetof/sizeof` 静态断言。
- `semIrTypeTable` 增静态 C 类型标注 + 类型流分析；typed 块只发特化指令。
- 门槛：嵌套 struct 布局/blittable 判定单测 GREEN；typed 块无通用算术 opcode。

### M2 · 标量纯 C 降级（02-S3, 04-S1/S2, 03-S3）
- 帧 scalar 槽落为 C 局部；算术/比较/位/转换发裸 C，取代 `ZrCore_Stack_GetValue`+`FAST_SET`。
- 成员/调用编译期 typed/dynamic 二选一。
- 门槛：数值基准函数 AOT 产物 `grep` VM 取值/写值 = 0，结果与解释器逐位一致。
- **此里程碑即兑现用户核心诉求“运算用 +-*/ 而非 VM 行为”。**

### M3 · struct 值语义纯 C（02-S4/S5, 04-S3/S4/S5, 05-S1/S2）
- inline struct 槽 = 连续字节/C 局部；复制走 `memcpy`/逐字段 `=`。
- struct 传参/返回 ABI；`FIELD_ADDR/LOAD/STORE/COPY` 与 `CALL_TYPED/RETURN_TYPED` 发裸 C。
- layout 驱动 GC 扫描表 + 帧 GC 根 + 安全点。
- 门槛：struct 密集程序 AOT 与解释器一致；GC 压力测试不丢引用；赋值即 `=`/`memcpy`。
- **此里程碑兑现“struct 栈上连续布局、赋值用 = 或 memcpy”。**

### M4 · 所有权/GC/桥（05-S3/S4/S5/S6, 04-S6/S7, 02-S6）
- 所有权流分析消除冗余检查；写屏障；typed↔dynamic bridge 与 deopt 封送。
- union typed 布局 + `switch` 降级；typed try/using 作用域 drop 静态展开。
- 门槛：混合（typed+dynamic）程序跨边界结果一致；触发 deopt 正确回退；白名单 CI 通过。

### M5 · 取代旧 ABI（收尾）
- typed 函数全面使用 byte-offset 帧；旧固定槽 ABI 仅 dynamic 函数保留并标 legacy。
- 全量回归 + 性能基线（typed 路径相对解释器加速比、相对旧半降级 AOT 的体积/性能）。
- 门槛：全测试套件 GREEN；典型负载 AOT 显著快于解释器且无 VM 调用退化。

## 2. 依赖关系

```
M0 ─▶ M1 ─▶ M2 ─▶ M3 ─▶ M4 ─▶ M5
            （M2 兑现标量诉求；M3 兑现 struct 诉求；M4 打通 hybrid；M5 收口）
```
- M1 是一切的前提（确定性 + 唯一 layout）。
- M2 与 M3 可在 M1 完成后部分并行（标量 vs struct），但 struct 调用 ABI（M3）依赖 M2 的调用约定雏形。

## 3. 测试矩阵

| 维度 | 用例 | 现有/新增 |
|------|------|-----------|
| 布局正确性 | 嵌套 struct/union 偏移、对齐、blittable | 扩展 `tests/core/test_type_layout_inline_copy.c` |
| 标量降级 | i/u/f 算术、位、比较、转换 | 新增 `tests/parser/test_aot_c_typed_scalar.c` |
| struct 降级 | 复制、字段读写、传参/返回 | 新增 `tests/.../test_aot_c_struct_value.c` |
| 调用 | typed 跨函数、typed↔dynamic 桥 | 新增 bridge 用例 |
| 所有权/GC | unique/borrow 消除、shared 转移点、GC 根/屏障 | 扩展 `tests/gc/gc_tests.c` |
| deopt | 类型意外回退字节码 | 新增 deopt 用例 |
| 黄金一致性 | 解释器 vs AOT 逐位对比（覆盖以上全部） | 新增对比 harness |
| 反退化 CI | typed C 产物 VM 调用白名单 grep | 新增 CI 步骤 |
| 验收文档 | 每里程碑一份 `tests/acceptance/2026-06-xx-aot-*.md` | 按 using 计划惯例 |

## 4. 验收标准（计划级 DoD）

1. **纯降级**：全 typed 基准函数 AOT 产物中，算术/赋值/字段/比较/分支零 VM 调用；
   `grep -E 'ZrCore_Stack_GetValue|ZR_VALUE_FAST_SET'` = 0。
2. **值语义**：struct 赋值为 `=`/`memcpy`，传参/返回符合 ABI，结果与解释器逐位一致。
3. **确定性**：typed 槽均有单一静态 C 类型；类型不明确声明编译期被拒（除非显式 `dyn`）。
4. **单一真相**：所有偏移/大小来自唯一 `SZrTypeLayout`，`offsetof/sizeof` 静态断言通过。
5. **双路径一致**：解释器与 AOT 对全测试矩阵逐位一致；deopt 回退正确。
6. **无回归**：现有 `tests/` 全套 GREEN（parser/core/gc/module/acceptance）。

## 5. 风险登记

| 风险 | 缓解 |
|------|------|
| typed C 局部引用对 GC 不可见 | 帧 GC 根描述符 + 安全点（05-S2），M3 门槛卡死 |
| 生成 struct 与 metadata 偏移漂移 | `offsetof/sizeof` 静态断言（M1），不一致即编译失败 |
| typed/legacy ABI 并存期错配 | 以函数为单位整体二选一，禁单函数混寻址（02 §7） |
| 半降级写法回潮 | 反退化 CI 白名单校验（M0），违规即构建失败 |
| 所有权消除不健全致 UAF | 流分析保守默认（不能证明则保留检查），M4 门槛对拍 |
| 范围过大 | 严格按 M0→M5 切片推进，每切片 RED→GREEN→对拍后再下一步 |

## 6. 落地记录约定

每完成一个阶段或切片，在 [`index.md`](index.md) 的 `##状态与产出记录` 追加一行：
`时间戳 · 里程碑/切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果（套件 通过/失败 数）· 备注`，
并为每个里程碑产出 `tests/acceptance/2026-06-xx-aot-<milestone>.md`（沿用 using 计划验收文档格式）。
