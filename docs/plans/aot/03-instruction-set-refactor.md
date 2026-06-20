---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 优化指令集功能以便含义转化 / 消除复杂操作对 VM 行为的依赖 / 含义明确化
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_numeric.c
  - zr_vm_core/include/zr_vm_core/function.h
references:
  - lua/hybridclr/libil2cpp/hybridclr/interpreter/Instruction.h
  - lua/hybridclr/libil2cpp/hybridclr/interpreter/Instruction_Enum.h
---

# 03 · 指令集改造：含义明确化、typed 化、消除不确定性

承接差距 G2/G3。目标：把“运行时按 tag 决定行为”的指令，改造为“编译期已知操作数类型、
可一对一映射为 C 表达式”的指令；复杂语义拆为明确的原子指令，不再隐式依赖 VM 行为。

## 1. 分层指令模型

指令集分两层（对标 hybridclr：bytecode 编码层 vs interpreter typed IR 层）：

- **执行字节码层（`zr_instruction_conf.h`）**：保持 8B 定长、向后兼容，作为序列化/dynamic 执行格式。
- **typed SemIR 层（`function.h` 的 `SZrSemIrInstruction`）**：编译期产出的**类型确定中间表示**，
  是解释器 typed 快路径与 C 后端的**共同输入**。指令集改造的重心在此层（见 `04`）。

> 不必把 251 条字节码全部 typed 化；而是保证“可 typed 的语义”都有对应的 typed SemIR 表达，
> 且该表达满足不变量 A/B。字节码层只做最小整理。

## 2. 字节码层整理（最小改动）

原则：减少“同一 opcode 多义”，让每条 opcode 的操作数类型与效果单一可判定。

1. **通用算术显式化**：保留 `ADD` 等通用指令仅用于 dynamic 路径；typed 编译产物**不得**发出通用
   `ADD`，必须发出已特化的 `ADD_SIGNED/ADD_UNSIGNED/ADD_FLOAT`（这些已存在）。
   编译器对 typed 块只允许特化变体——把“运行时三路 tag 分支”前移到编译期选指令。
2. **操作数寻址明确化**：为 typed 块统一改用 SemIR 槽（byte-offset）寻址，
   不再依赖 `functionBase + slot` 的隐式逻辑槽。字节码层的 `GET_STACK/SET_STACK` 仅 dynamic 用。
3. **成员/调用路径收敛**：成员访问在编译期定为
   `FIELD_ADDR + LOAD_VALUE/STORE_VALUE`（typed，已知偏移）或 `META_GET/META_SET`（dynamic）二选一；
   调用定为 `CALL_TYPED`（已知签名）或 `DYN_CALL/META_CALL`（dynamic）二选一。
   消除“同一处可能走多条路径”的运行时选择（差距 G2）。
4. **指令文档化语义**：为每条仍保留的字节码标注：操作数静态类型约束、是否可 typed、
   是否含 VM 副作用。写入指令表注释/生成的元描述，作为审计基线。

## 3. typed SemIR opcode 设计

在现有 `EZrSemIrOpcode`（`function.h:306-334`）基础上，确立**可纯 C 降级**的 typed 算子族。
现有已含 `VALUE_ADDR/FIELD_ADDR/LOAD_VALUE/STORE_VALUE/INIT_VALUE/COPY_VALUE/CALL_TYPED/RETURN_TYPED`。
补齐 typed 标量算子（每个都携带 `typeTableIndex` 指定操作数静态类型）：

- 算术：`ADD/SUB/MUL/DIV/MOD/NEG`（typed，按 typeTableIndex 区分 i/u/f）。
- 位运算：`AND/OR/XOR/NOT/SHL/SHR`。
- 比较：`EQ/NE/LT/LE/GT/GE`（结果 `TZrBool`）。
- 转换：`CONV`（src 静态类型 → dst 静态类型，映射为 C 强制转换）。
- 常量：`CONST`（typed 立即数/常量池引用）。
- 控制：`BR/BR_COND/SWITCH/RET`（结构化，块化）。

设计要点：

- **每个 typed opcode 一条 C 降级规则**（见 `04` §2 的降级表），1 指令 → 1–3 行 C，无 VM 调用。
- `typeTableIndex` 是降级时选择 C 类型与运算符的唯一依据（不变量 A）。
- 不可 typed 的语义不进 typed opcode，保持为 `DYN_*/META_*/OWN_*`，由 runtime 调用点承载（不变量 B）。

## 4. 类型表与槽类型标注（补 G3）

- 扩展 `semIrTypeTable`：每个 typed 槽登记 `EZrStaticCType`（i8..i64/u8..u64/f32/f64/ref/struct(cTypeId)/bool）。
- 编译器在生成 SemIR 时做**类型流分析**，为每个 def/use 标注单一静态类型；
  冲突或未知 → 该块降级为 dynamic 并插入 `deopt`（不变量 A 的执行机制）。
- `SZrSemIrInstruction.deoptId` + `SZrSemIrDeoptEntry` 记录 typed→exec 的回退点，
  使 typed 块可在运行期遇到意外（如溢出策略、动态值）安全回退到字节码解释。

## 5. 复杂指令拆解（去 VM 依赖）

把“一条指令隐式做很多事”的复杂指令，拆为明确原子序列，让含义可转译：

| 复杂语义 | 现状（隐式 VM 行为） | 改造为（明确原子） |
|----------|----------------------|--------------------|
| 成员读 | `GET_MEMBER`（运行时查原型/元方法） | typed: `FIELD_ADDR(off)` + `LOAD_VALUE`；dynamic: `META_GET` |
| 成员写 | `SET_MEMBER` | typed: `FIELD_ADDR(off)` + `STORE_VALUE`（+ 写屏障点）；dynamic: `META_SET` |
| struct 复制 | 赋值经 VM | `COPY_VALUE`（blittable→memcpy；否则逐字段） |
| 调用 | 多路 `*_CALL` 运行时选择 | typed `CALL_TYPED`（签名已知，C 直接调）/ dynamic `DYN_CALL` |
| 数组元素 | `GET_BY_INDEX`（边界+类型运行时） | typed: 边界检查显式 `BR_COND` + `FIELD_ADDR`；dynamic 保留 |
| 迭代 | `ITER_*`（VM 驱动） | typed 可降为 `for` 索引循环；不可则 dynamic |

每行右列都满足不变量 B：typed 列零 VM 调用，dynamic 列把 VM 行为收敛到单一明确调用点。

## 6. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 03-S1 | typed 块只准发特化算术/比较指令；编译器拒绝在 typed 块发通用 `ADD` | 单测：typed 数值函数 SemIR 无通用算术 opcode |
| 03-S2 | `semIrTypeTable` 增 `EZrStaticCType`，类型流分析标注每槽 | 单测：标注覆盖率/冲突触发 deopt |
| 03-S3 | 成员/调用路径编译期二选一（typed/dynamic），消除运行时多路选择 | 单测：已知 struct 成员走 `FIELD_ADDR`，dyn 走 `META_*` |
| 03-S4 | 复杂指令拆解（表 §5）落到 SemIR 生成 | 解释器执行 typed SemIR 与旧字节码结果一致 |
| 03-S5 | deopt 点生成与回退执行打通 | 构造类型意外用例，typed 块正确 deopt 到字节码 |

## 7. 兼容与回归

- 字节码格式不破坏：dynamic 路径继续用现有 251 条指令与 computed-goto 分发。
- 每个切片以解释器 typed-SemIR 执行结果对齐旧字节码执行结果（黄金对比），再推进 C 后端（`04`）。
- 现有 `tests/parser/test_compiler_features.c` / `test_compiler_integration_main.c` 保持 GREEN。
