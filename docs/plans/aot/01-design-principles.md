---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 AOT 纯 C 降级原则 / 含义明确化 / 与 il2cpp/hybridclr 对标
references:
  - lua/hybridclr/libil2cpp/hybridclr/interpreter/Instruction.h
  - lua/hybridclr/libil2cpp/hybridclr/metadata/ClassFieldLayoutCalculator.h
  - lua/hybridclr/libil2cpp/codegen/il2cpp-codegen.h
---

# 01 · 设计纲领、不变量与对标

本文确立全计划必须遵守的不变量，以及与 `lua/il2cpp`、`lua/hybridclr` 的架构对标关系。
后续 02–06 的所有设计都必须可被本文的不变量检验。

## 1. 三条不变量（硬约束）

### 不变量 A — 确定性（Determinism）

> 进入 typed 路径的每个 SemIR 槽都必须有**单一静态 C 类型**，编译期已知，运行期不变。

合法静态类型集合（首批）：
`TZrBool / TZrInt8..64 / TZrUInt8..64 / TZrFloat32 / TZrFloat64 / 引用指针(SZrRawObject* 及子类) / struct ZrLayout_<TypeId>（内联值类型）`。

- 一个槽**不允许**在不同指令处呈现不同 C 类型。
- 类型无法静态确定的表达式**禁止**进入 typed 路径；它必须留在 dynamic 路径（tagged union），
  或在 typed 路径中以一个明确的 `deopt` 点切回 dynamic（见不变量配套的 deopt 机制）。
- 对应用户要求：“全局代码含义解析和明确化，不允许不明确的声明”。
  落地为编译期**类型完备性检查**：若某声明/表达式无法定出单一静态类型且未显式标注为
  `dyn`，编译器报错而非默默退化。

### 不变量 B — 纯降级（Pure Lowering）

> typed 路径的每条 SemIR 指令必须能映射为**不含 VM 调用**的 C 片段。

- 算术 → `+ - * / %`；位运算 → `& | ^ ~ << >>`；比较 → `== != < <= > >=`；
  赋值 → `=`；聚合复制 → `memcpy` 或逐字段 `=`；字段 → `.` / `->` / `*(T*)(base+off)`；
  分支 → `if` / `goto` / `switch`。
- 需要 VM 运行时的操作（堆分配、动态派发、GC 写屏障、所有权状态机、异常展开）**只能**
  以**显式 runtime 调用点**出现，且这些调用点是 ABI 契约的一部分（见 `04`/`05`），
  数量与位置可被审计；**禁止**为了图省事把纯标量操作也包进 `ZrCore_*` 调用。
- 验收口径：typed 函数体生成的 C 中，**算术/赋值/字段/比较/分支零 VM 调用**；
  仅允许在分配/调用/屏障/异常边界出现受控的 runtime 调用。

### 不变量 C — 单一真相（Single Source of Truth）

> 值的形状只由类型 metadata 描述一次，所有消费者读同一份。

- `SZrTypeLayout` / 原型是布局的唯一真相；解释器、AOT C、GC 扫描、反射、序列化全部从它派生。
- 生成的 C struct（`struct ZrLayout_<TypeId>`）的字段顺序/偏移/对齐**必须**与 `SZrTypeLayout`
  一致，由同一段布局计算产出（编译期断言 `offsetof == byteOffset`）。
- 禁止在后端各处硬编码字段偏移；偏移只能来自 metadata。

## 2. 与 il2cpp 的对标

| il2cpp 机制 | zr_vm 对应 | 计划动作 |
|-------------|-----------|---------|
| IL method → C/C++ 函数 | typed function → C 函数（`04`） | SemIR `CALL_TYPED`/`RETURN_TYPED` 降级为真实 C 调用/返回 |
| `ClassLayoutInfo`/`FieldLayout`（offset/size/align/blittable） | `SZrTypeLayout`/`SZrTypeLayoutField` | 补 `blittable` 判定，生成 `struct ZrLayout_*`（`02`） |
| 值类型按字段展开 / 隐藏指针返回 | frame slot inline struct + 返回 ABI | 定义 struct 传参/返回 ABI（`02`/`05`） |
| 局部变量 → C 局部变量 | typed 槽 → C 局部 `TZr*`/struct | 标量槽去 tagged-union（`02`） |
| metadata 表驱动、运行期查表不计算 | metadata token + 原型 | 偏移/大小编译期定死，运行期不算（`02`） |
| GC 引用字段标记 | layout `gcFieldCount`/`GC_VALUE` flag | 生成 GC 扫描描述符 + 写屏障点（`05`） |

## 3. 与 hybridclr 的对标（hybrid / 双路径）

| hybridclr 机制 | zr_vm 对应 | 计划动作 |
|----------------|-----------|---------|
| Interpreter IR（typed 变体 `IRBinOpVarVarVar_Add_i4` 等） | typed SemIR opcode + 静态类型表 | typed SemIR 既喂解释器快路径也喂 C 后端（`03`/`04`） |
| AOT 与解释器经 `MethodBridge` 互调 | typed ↔ dynamic 边界 + 调用桥 | 定义 bridge 与参数打包/展开（`05`） |
| 定长指令、size table 快速解码 | 8B 定长指令 | 保持定长，typed 路径用 SemIR 而非重解码字节码 |
| 解释器作为热更/回退 | dynamic 路径作为 deopt 回退 | typed 失败/类型未知时 deopt 到 dynamic（`04`） |

**关键借鉴**：hybridclr 不追求“消灭解释器”，而是让 typed/AOT 与解释器是**同一 IR 的两种降级**，
经 bridge 协作。本计划采用相同立场：纯 C 是主路径（减轻对解释器依赖），解释器是回退与动态能力载体。

## 4. 双路径分类规则（typed vs dynamic）

编译期对每个函数/基本块/表达式做分类：

- **typed**：满足不变量 A（所有参与槽有单一静态类型），全部操作满足不变量 B。
  → 进入 SemIR typed 降级，可生成纯 C，亦可被解释器 typed 快路径执行。
- **dynamic**：含类型不定（`dyn`）、反射、动态成员、跨边界未知调用。
  → 留在现有 tagged-union 指令路径（解释器执行 / AOT 以受控 runtime 调用降级）。
- **混合函数**：以基本块为粒度切分，typed 块与 dynamic 块之间以 `deopt`/bridge 衔接，
  衔接处做一次 tagged-union ↔ typed 槽的 marshaling（`05`）。

## 5. 设计反模式（明令禁止）

- ❌ 在生成的 C 里调用 `ZrCore_Stack_GetValue` 取标量再 `ZR_VALUE_FAST_SET` 写回（半降级）。
- ❌ 为类型确定的 `+`/`=` 生成 `ZrCore_*` 调用。
- ❌ 在后端硬编码字段偏移而不读 metadata。
- ❌ 允许“类型不明确仍进入 typed 路径”的隐式退化（必须显式 deopt 或编译期报错）。
- ❌ 让解释器与 C 后端各自实现一套值语义（必须共享 SemIR 与 layout）。

## 6. 成功判据（计划级）

1. 一个全 typed 的数值/struct 函数，AOT 产物中**算术/赋值/字段零 VM 调用**，
   且与解释器执行结果逐位一致。
2. `struct` 赋值在生成 C 中表现为 `=`/`memcpy`，传参/返回符合既定 ABI。
3. 所有偏移/大小可追溯到唯一 `SZrTypeLayout`，`offsetof` 编译期断言通过。
4. 类型不明确的声明在编译期被拒绝（除非显式 `dyn`），而非运行期退化。
