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

### M1.5 · 环境隔离与去双写（先行，07-S1..S7）
- **目标**：先把生成函数的「解释器环境税」与「register/SZrValue 双写」彻底铲除，
  为 M2 的标量纯 C 降级铺好执行环境。对应 [`07`](07-codegen-register-model-and-environment-isolation.md) 全文与不变量 D。
- 删除 typed 函数体内 `zr_aot_begin_instruction`（PC/observation/debugHook 逐指令发布），观测改默认 NONE、按需边界化（07-S1/§8）。
- `CONST`/标量写改**单写寄存器**，删除 `frame.slotBase[..]` 的 `SZrTypeValue` 写与所有权释放（07-S2/§5）。
- 引入 `SZrAotMethodInfo` 取代胖 `frame` 装配；byte-frame 收窄到「GC/可寻址槽」，纯标量函数 `registerFrameBytes==0`、prologue 为空（07-S3/S4）。
- SZrValue 只在 4 类 VM↔native 边界（入参/返回/inout 写回/deopt）构建，typed→typed 直调直返；GC 引用寄存器直用 object gc 接口 + 帧根表（07-S5/S6）。
- 门槛：`var left:int=21;` 生成 == 1 条赋值语句；环境零注入/零双写 grep（07-§9.1/§9.2）通过；全 typed 基准运行期 `SZrTypeValue` 构造计数 = 0；解释器路径与现有测试不回归。
- **此里程碑直接兑现用户诉求“抹除 AOT 与解释器的环境耦合、追求极致性能、不得复杂到慢于解释器”**，并为反退化 CI（07-S7）落桩。

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

> M0–M5 兑现「typed 纯 C 降级 + 环境隔离」的核心引擎。M6–M7 在此之上补齐
> 「泛型共享 / 内存管理硬化 / 反射 / 元数据 / 裁剪」的平台能力（08–12），形成完整 AOT 工具链。

### M6 · 泛型共享（08-S1..S7, 09-S1）
- 泛型实例化收集 + 共享性判定（值类型单态化 / 引用类型共享）+ 约束求解。
- 泛型字典（RGCTX 等价）布局 + lazy 解析；单态/共享两形态代码生成与 `CALL_TYPED`。
- 未收集实例 deopt 到解释器动态实例化（hybrid）；可选 full-AOT 闭合模式。
- 门槛：值类型泛型 AOT 为纯 C 且对齐解释器；多引用类型实例共享一份代码；动态实例经 deopt 正确。
- **此里程碑兑现「il2cpp 式泛型共享」决策。**

### M7 · 内存管理硬化 + 反射/元数据/裁剪（09-S2..S5, 10, 11, 12）
- 内存：精确栈根 + safepoint + 移动 GC 根更新、写屏障编译期消除、装箱/FFI pin（09 剩余切片）。
- 元数据：zrp 两段式 + 运行期 token lazy 解析 + token↔cTypeId↔ZrLayout 三向表 + 版本校验（11）。
- 反射：三级元数据 + invoker thunk + token/泛型/offset 反射 + 保留注解（10）。
- 裁剪：mark-and-sweep 可达性驱动 AOT 生成/元数据/反射收窄 + manifest + trim 警告 + 体积统计（12）。
- 门槛：移动 GC 压力下 AOT 栈引用不丢/被更新；默认产物元数据最小且 token 通道可用；
  注解反射目标裁剪后可用、未注解给警告；裁剪前后体积对比可量化。
- **此里程碑兑现「内存管理 / 反射 / 元数据 / 代码裁剪」完善诉求。**

## 2. 依赖关系

```
M0 ─▶ M1 ─▶ M1.5 ─▶ M2 ─▶ M3 ─▶ M4 ─▶ M5 ─▶ M6 ─▶ M7
            （M1.5 抹除环境耦合/双写；M2 标量；M3 struct；M4 hybrid 桥；M5 收口引擎；
             M6 泛型共享；M7 内存硬化+反射+元数据+裁剪 平台能力）
```
- M1 是一切的前提（确定性 + 唯一 layout）。
- **M1.5 是 M2 的硬前置**：必须先有「寄存器=C 局部、零解释器环境、零双写」的执行环境，
  M2 的算术/比较/位/转换才能直接写裸 C（`zr_aot_sD = zr_aot_sA + zr_aot_sB;`）；
  否则裸 C 运算仍会被包在 begin_instruction + SZrValue 双写里，性能无从兑现。
- M2 与 M3 可在 M1.5 完成后部分并行（标量 vs struct），但 struct 调用 ABI（M3）依赖 M2 的调用约定雏形。
- **M6 依赖 M3（值类型/struct 布局）+ M4（deopt 桥）**：单态化要 layout，动态实例兜底要 deopt。
- **M7 依赖 M6 + M3**：泛型字典 TYPE_LAYOUT/GC descriptor、反射 invoker、元数据三向表都建立在
  layout 与泛型实例之上；裁剪（12）是 08/10/11 的共同上游，但其消费方需先存在，故置于 M6 之后。
  09 的 GC descriptor（09-S1）随 M6 先行（泛型字典需要），其余内存硬化（精确根/safepoint）归 M7。

## 3. 测试矩阵

| 维度 | 用例 | 现有/新增 |
|------|------|-----------|
| 布局正确性 | 嵌套 struct/union 偏移、对齐、blittable | 扩展 `tests/core/test_type_layout_inline_copy.c` |
| 标量降级 | i/u/f 算术、位、比较、转换 | 新增 `tests/parser/test_aot_c_typed_scalar.c` |
| struct 降级 | 复制、字段读写、传参/返回 | 新增 `tests/.../test_aot_c_struct_value.c` |
| 调用 | typed 跨函数、typed↔dynamic 桥 | 新增 bridge 用例 |
| 所有权/GC | unique/borrow 消除、shared 转移点、GC 根/屏障 | 扩展 `tests/gc/gc_tests.c` |
| deopt | 类型意外回退字节码 | 新增 deopt 用例 |
| 环境隔离/去双写 | `var x=21` 单行；零注入/零双写 grep；SZrValue 运行期计数=0 | 新增（M1.5，07§9） |
| 泛型共享 | 值类型单态化 / 引用类型共享 / 泛型字典解析 / 动态实例 deopt | 新增 `tests/.../test_aot_c_generic_sharing.c`（M6） |
| 移动 GC 精确根 | compact/evacuate 后 AOT 栈引用被更新、不丢 | 扩展 `tests/gc/`（M7，09） |
| 反射 | invoker 调用、token/泛型/offset 反射、注解保留 | 新增反射用例（M7，10） |
| 元数据 | zrp round-trip、token lazy 解析、版本不匹配 deopt | 新增 `tests/.../test_zrp_metadata.c`（M7，11） |
| 代码裁剪 | 可达性正确、死码不进产物、注解保留、体积对比 | 新增 `tests/.../test_aot_stripping.c`（M7，12） |
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
