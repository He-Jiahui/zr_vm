---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 生成代码过重（callInfo/destination/SZrValue/observation 全量注入）/
      要求 AOT 环境与解释器环境信息隔离与抹除 / 每函数仅用 MethodInfo 控制 native 堆栈 /
      VM↔native 仅在必要边界（返回值、in/out 写回）构建 SZrValue / 可 GC 对象直用 object gc 接口 /
      基于寄存器的生成、不必遵从 SZrValue+Object 形态 / 追求极致性能、参照 il2cpp / 不允许复杂到慢于解释器
references:
  - lua/hybridclr/libil2cpp/codegen/il2cpp-codegen.h
  - lua/hybridclr/libil2cpp/il2cpp-config.h
  - lua/hybridclr/libil2cpp/vm/MetadataCache.h        # il2cpp MethodInfo
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
---

# 07 · 代码生成寄存器模型 + AOT/解释器环境隔离（极致性能降级规则）

> 本文是 02/04/05 在 **prologue + 存储写入 + 函数描述符** 三个面上的收口。
> 02/04/05 说清了「typed 值是裸 C、运算是 `+-*/`、struct 是 `memcpy`」，
> 但**没有**规定生成函数的**执行环境**该长什么样。结果是：当前生成器即便对
> `var left: int = 21;` 也注入了整套解释器环境（callInfo / slotBase / stackTop /
> programCounter / observationMask / debugHook）并对同一个值**双写**（既写 `SZrTypeValue`
> 槽，又写寄存器 `zr_aot_s0`）。本文把这些**逐条列为删除/改写目标**，给出
> il2cpp 风格的极简生成规则。

本文新增第四条不变量，与 `01` 的 A/B/C 并列：

> **不变量 D（环境隔离）**：typed AOT 函数体内**不得**出现解释器执行环境的装配与维护
> （`SZrCallInfo` 帧装配、`slotBase`/`stackTop` 推进、`programCounter`/`observation`/`debugHook`
> 的逐指令发布）。解释器状态只在**函数边界**通过 marshaling 出现一次，函数体内只有
> **寄存器（C 局部）+ 纯 C 元素操作**。

---

## 1. 现状解剖：一条 `var left: int = 21;` 生成了什么（全部要删/改）

下面是当前生成物（用户报告），逐块标注处置：

```c
{   /* zr_aot_begin_instruction —— 整块删除（解释器逐指令观测机制） */
    SZrCallInfo *zr_aot_call_info = frame.callInfo;       // 删：AOT 不需要 callInfo
    TZrBool zr_aot_line_debug_enabled;                    // 删：调试观测
    TZrBool zr_aot_publish_all_instructions;              // 删：逐指令发布
    if (state == ZR_NULL || frame.function == ZR_NULL) ZR_AOT_C_FAIL();   // 删：运行期校验
    ...
    frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;     // 删：槽基址推进
    state->stackTop.valuePointer = zr_aot_call_info->functionTop...;      // 删：栈顶维护
    frame.currentInstructionIndex = 0;                                    // 删：PC 维护
    zr_aot_call_info->context.context.programCounter = ... + 0;           // 删：PC 发布
    ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_LINE, ...);             // 删：行 hook
}
{   /* zr_aot_value_exec_primitive_constant —— 改写：只保留寄存器写 */
    SZrTypeValue *zr_aot_destination = &frame.slotBase[0].value;          // 删：SZrValue 目标
    if (zr_aot_destination->ownershipKind != ... || ...isGarbageCollectable)
        ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);         // 删：所有权释放
    zr_aot_destination->type = ZR_VALUE_TYPE_INT64;                       // 删：tag 写入
    zr_aot_destination->value.nativeObject.nativeInt64 = (TZrInt64)21;    // 删：槽写入
    zr_aot_destination->isGarbageCollectable = ZR_FALSE;                  // 删
    zr_aot_destination->isNative = ZR_TRUE;                               // 删
    zr_aot_destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;     // 删
    zr_aot_destination->ownershipControl = ZR_NULL;                       // 删
    zr_aot_destination->ownershipWeakRef = ZR_NULL;                       // 删
    zr_aot_s0 = (TZrInt64)21;                                             // 留：唯一应保留的语句
}
```

**目标产物**（M2 收口后，整条声明）：

```c
    TZrInt64 zr_aot_s0 = (TZrInt64)21;     /* 就这一行 */
```

> 现状里 **23 行**做了一件事：把 21 放进一个 int。其中 22 行是解释器环境的税。
> il2cpp 对 `int left = 21;` 生成的也就是 `int32_t L_0 = 21;`。这就是本文的标准。

诊断结论（三类病灶，分别对应 §3/§4/§5）：
- **病灶 1：环境注入**——`zr_aot_begin_instruction` 把解释器逐指令观测搬进了 typed 函数体。
- **病灶 2：双写**——值既写 `SZrTypeValue` 槽（`frame.slotBase[0]`）又写寄存器 `zr_aot_s0`，
  寄存器写已是真相，SZrValue 写纯属冗余且带来所有权释放调用。
- **病灶 3：胖描述符**——`frame` 携带 ~15 个解释器字段（callInfo/module/functionTable/
  observationMask/publishAllInstructions/lastObserved*…），typed 函数一个都不需要。

---

## 2. 两套环境：彻底隔离（不变量 D 的模型）

```
+- 解释器环境（VM env，dynamic 路径独占）----------------------+
|  SZrState / SZrCallInfo / functionBase+slotBase / stackTop  |
|  SZrTypeValue 槽（tag + ownership 控制块 + GC 标记）          |
|  programCounter / debugHook / observationMask / publish     |
+------------------------------------------------------------+
            ^  仅在「函数边界」双向 marshaling（§6），函数体内不可见
            |
+- AOT native 环境（typed 路径独占）--------------------------+
|  寄存器文件：TZrInt64/TZrFloat64/ZrLayout_*/SZrRawObject*    |
|  （= C 局部，§3）                                            |
|  纯 C 元素操作（+-*/ = memcpy . -> if goto switch）          |
|  GC 引用：直接 SZrRawObject*，登记到帧根表（§5/05§3）        |
|  函数描述符：MethodInfo（§4），不是 SZrCallInfo              |
+------------------------------------------------------------+
```

隔离规则（硬性）：
1. typed 函数体内**禁止**出现 `SZrCallInfo`、`frame.slotBase`、`state->stackTop`、
   `programCounter`、`observationMask`、`publishAllInstructions`、`Debug_Hook`、
   `ZrCore_Ownership_ReleaseValue` 等解释器环境符号。
2. typed 函数体内**禁止**构造 `SZrTypeValue`，除非该语句正处于 §6 定义的边界 marshaling 点。
3. 解释器环境只在**进入/离开 typed 函数**时各出现一次（参数解包 / 返回打包），
   且由 §6 的固定模板生成，函数体本身对它无感。

---

## 3. 寄存器文件模型（il2cpp 风格，本计划的存储真相）

### 3.1 寄存器即 C 局部，不再有 SZrValue 槽

- 每个 typed SemIR 槽 `N` 映射到**一个寄存器** `zr_aot_sN`，它是一个**裸 C 局部变量**，
  类型由 `semIrTypeTable[N]` 决定（`TZrInt64` / `TZrUInt32` / `TZrFloat64` /
  `struct ZrLayout_<cTypeId>` / `SZrRawObject*` 之一，见 `02` §1/§3.2）。
- typed 函数体内**没有** `frame.slotBase[N]`，**没有** `SZrTypeValue`。寄存器就是值本身。
- 寄存器声明集中在函数 prologue 之后、首个 block label 之前（§4.3）：

  ```c
  TZrInt64      zr_aot_s0;   /* i64  */
  TZrFloat64    zr_aot_s1;   /* f64  */
  ZrLayout_42   zr_aot_s2;   /* inline struct */
  SZrRawObject *zr_aot_s3;   /* gc ref（登记到帧根表 §5） */
  ```

- 与 02§3.2 的差异：02 用 `sN` 命名，本文统一沿用现有发射前缀 `zr_aot_sN`
  （`backend_aot_c_scalar_locals.c` 已在用），避免二次改名。

### 3.2 寄存器分配策略（先简单，够用即止）

不追求图着色寄存器分配（那是编译器优化器的活，交给 C 编译器）。规则：

- **一槽一寄存器**：默认每个 SemIR 槽一个命名 C 局部，生命周期覆盖全函数。
  C 编译器（gcc/clang `-O2`）会自行做 SSA / 寄存器分配 / 死代码消除，AOT 不必抢这活。
- **SSA 友好但不强制**：若类型流分析已给出每槽单一静态类型（不变量 A），直接 `=` 即可；
  无需 PHI 节点显式化——沿用 SemIR 的块结构 + C 局部，让 C 编译器处理汇合。
- **常量折叠下放**：`CONST` 直接 `zr_aot_sN = (T)literal;`，不预先建表。
- **不做的事**：不生成 `zr_aot_s*` 与 `frame.slotBase[*]` 的同步、不生成槽溢出到 SZrValue
  的影子写（除边界 §6）。

> 设计取向：**AOT 后端只负责把语义翻成正确的、扁平的 C，性能交给 C 编译器**。
> 这正是 il2cpp 的立场（它生成朴素 C++，靠 IL2CPP→C++→native 工具链的 `-O2`），
> 也是「不允许复杂到慢于解释器」的根本保证——越薄的生成层越快。

### 3.3 寄存器 ↔ 字节帧（byte-frame）的关系

- `02` §3.1 的 byte-offset 帧仍是**值类型/可寻址槽**与 GC 根的承载方式；
  但**纯标量寄存器不进字节帧**——它们就是 C 局部，连续字节帧只承载：
  (a) 需要取地址的 inline struct 槽；(b) 含 GC 引用、需被根表登记的槽。
- 即：`frameByteSize` 只统计「必须落字节帧」的槽；纯标量/已证明不取地址的槽零字节帧占用。
- 这把 `backend_aot_c_frame_setup.c` 里 `frame_byte_slot_count` 的计算从「全槽」收窄到
  「GC/可寻址槽」，多数热函数 `frameByteSize == 0` → prologue 进一步坍塌（§4）。

---

## 4. 函数描述符：用 MethodInfo 取代解释器 frame

### 4.1 MethodInfo（新增，对标 il2cpp `MethodInfo`）

typed 函数运行期**唯一**需要的「自我描述」是一个轻量、只读、每函数一份的常量结构，
用于：(a) 在需要时定位 GC 根布局；(b) 边界 marshaling 时定位签名；(c) deopt 时定位字节码。
它**不**承载 PC、observation、stackTop 等运行状态。

```c
typedef struct SZrAotMethodInfo {
    TZrUInt32              functionIndex;       /* 模块内函数序号 */
    const SZrFunction     *metadataFunction;    /* deopt/反射时回到字节码与原型 */
    TZrUInt32              registerFrameBytes;   /* 需落字节帧的总字节（§3.3），多为 0 */
    const SZrAotGcRootMap *gcRootMap;           /* 含引用的寄存器布局表（05§3），无引用则 NULL */
    const SZrAotSignature *signature;           /* 边界 marshaling 用（§6），typed C 签名描述 */
    TZrUInt8               observationPolicy;    /* 默认 NONE；仅 debug 构建/显式策略时非 NONE */
} SZrAotMethodInfo;
```

- 每个 AOT 函数发射**一个 `static const SZrAotMethodInfo`**，地址即该函数的身份。
- typed 纯标量函数：`registerFrameBytes==0`、`gcRootMap==NULL`、`observationPolicy==NONE`
  → MethodInfo 在热路径上**根本不被读**（全部信息编译期内联进了 C）。

### 4.2 取代 `frame` 胖结构

当前 `backend_aot_write_c_frame_setup` 装配的 `frame.{callInfo,slotBase,module,
functionTable,functionThunks,observationMask,publishAllInstructions,lastObserved*,
generatedFrameSlotCount,...}` —— typed 函数体内**全部移除**。它们要么：
- 下放为 C 局部寄存器（值）；
- 收进 `static const SZrAotMethodInfo`（只读元数据）；
- 仅在边界 marshaling 模板里临时使用（`state`、`SZrCallInfo`），不进函数体语句流。

### 4.3 typed 函数骨架（最终形态）

```c
ZR_AOT_EXPORT TZrInt32 zr_fn_<Module>_<Name>(SZrState *state /*, typed 形参… */) {
    /* prologue：对纯标量 typed 函数 == 空。仅按需出现下列任意项： */
    /*   - GC 根登记：仅当 methodInfo.gcRootMap != NULL（含引用槽，05§3）        */
    /*   - 栈检查：仅当会发生 typed 调用/分配（safepoint 边界）                  */
    /*   - 观测：仅当 methodInfo.observationPolicy != NONE（debug 构建）          */

    TZrInt64 zr_aot_s0;            /* 寄存器声明（§3.1） */
    /* … */

L_blk0:
    zr_aot_s0 = (TZrInt64)21;     /* 纯 C，无环境维护 */
    /* … §2 降级表 … */
    return 0;                      /* 或按 §6 返回 marshaling */
}
```

> 纯标量 typed 函数的 prologue/epilogue **可为空**。这是「环境隔离」最直接的可见结果。

---

## 5. 存储写入规则：消除双写，寄存器是唯一写入目标

### 5.1 标量/值寄存器：单写

| 旧（双写，删除） | 新（单写） |
|---|---|
| 写 `SZrTypeValue` 槽（tag/ownership/isNative/gc 标记）**且** 写 `zr_aot_sN` | **只**写 `zr_aot_sN` |

- `CONST` → `zr_aot_sN = (T)literal;`
- 任意 typed 运算/转换 → 结果**只**落寄存器（`zr_aot_sD = …;`）。
- **禁止**对 typed 标量发 `ZrCore_Ownership_ReleaseValue`：标量无所有权，release 是 no-op 成本。
- **禁止**写 `->type / ->isNative / ->isGarbageCollectable / ->ownership*`：这些是 SZrValue 字段，
  typed 寄存器没有它们。

### 5.2 GC 引用寄存器：直接用 object 的 GC 接口

用户要求「可 GC 的对象直接使用 object 的 gc 接口生成进行管理」。落地：

- GC 引用寄存器类型为 `SZrRawObject*`（或其子类指针），**不**包成 `SZrTypeValue`。
- 分配：直接调对象侧 GC 分配接口（如 `ZrCore_Gc_New*` / `ZrCore_Object_Alloc*`，
  白名单内 runtime 调用，`04`§4），结果是 `SZrRawObject*`，直接进寄存器。
- 可达性：寄存器引用通过**帧 GC 根表**（`05`§3，`methodInfo.gcRootMap`）对 GC 可见，
  **不**靠 SZrValue 的 `isGarbageCollectable` 标记。
- 赋值：引用寄存器间 `=`；写入**对象字段/堆**时加写屏障（`05`§4），写入**寄存器**不加屏障。
- 所有权：按 `05`§1 的流分析——unique/borrow 静态消除；shared 仅在转移点 retain/release；
  函数体内**不**逐值带 ownership 控制块。

### 5.3 inline struct 寄存器

- 即 `ZrLayout_*` C 局部（`02`§3.2）。赋值 `=`（blittable）或逐字段（`02`§4），无 SZrValue。

---

## 6. VM ↔ native 边界：SZrValue 只在这里构建

> 用户要求「虚拟机和 native aot 之间的交互只需要必要信息转换，例如将返回值、in/out 值写回的时候，
> 再构建 SZrValue」。这是 SZrValue **唯一**被允许出现在 AOT 相关代码里的地方。

边界点共四类，每类一个固定 marshaling 模板（由 `methodInfo.signature` 驱动）：

| 边界 | 方向 | 规则 |
|---|---|---|
| **入参解包** | VM→native | 从调用方栈/SZrValue 解包为 typed 寄存器；typed→typed 直调时按 C 形参直接传，无 SZrValue |
| **返回打包** | native→VM | 仅当被解释器/dynamic 调用方调用时，把返回寄存器封为 `SZrTypeValue` 写回 `returnDestination`；typed→typed `return` 直接返裸 C 值 |
| **in/out 写回** | native→VM | `out`/`ref` 参数在函数末尾把寄存器值写回调用方提供的目标（typed 直写指针；跨 dynamic 边界才建 SZrValue） |
| **deopt / 调 dynamic** | native→VM | 按 layout 把相关寄存器 box 成 SZrValue（`05`§5 `Bridge_BoxTyped`），跳字节码/ dynamic callee |

关键优化：**typed→typed 调用全程不碰 SZrValue**（`04`§2 `CALL_TYPED` 直接 C 调用、C 返回）。
只有跨越「typed↔dynamic/解释器」这条线时，才在该线上做一次 box/unbox。
→ 全 typed 程序：SZrValue 构造次数 = 0（除非显式 deopt）。

---

## 7. 逐指令简化规则（统一收口，禁止再生成环境代码）

所有 typed opcode 的发射器（`backend_aot_c_scalar_*.c` / `backend_aot_c_lowering_*.c`）遵守：

1. **不发 `zr_aot_begin_instruction`**：删除逐指令的 PC/observation/debug 发布。
   观测改为「按需、整函数级」（§8），默认零。
2. **不发 SZrValue 写**：所有结果只落寄存器（§5）。删除 `zr_aot_value_exec_primitive_constant`
   里对 `frame.slotBase[..].value` 的全部写入，只留寄存器赋值。
3. **不发帧维护**：不写 `frame.slotBase`/`state->stackTop`/`frame.currentInstructionIndex`。
4. **一指令一表达式**：尽量发单条 C 表达式语句（`zr_aot_sD = zr_aot_sA + zr_aot_sB;`），
   不包 `{ … }` 作用域块、不声明临时 `SZrTypeValue*`。
5. **校验下放编译期**：`if (state==NULL||frame.function==NULL) FAIL();` 这类运行期断言，
   typed 路径删除（不变量 A 已在编译期保证）；仅保留**语义必需**的运行期检查
   （如 `DIV` 除零 → 抛异常/ deopt 点，`04`§2）。

before/after 速查（与 `04`§2 降级表合并使用）：

| 指令 | 现状（环境+双写） | 目标 |
|---|---|---|
| `CONST i64=21` | begin_instruction(23 行) + SZrValue 9 写 + `zr_aot_s0=21` | `zr_aot_s0=(TZrInt64)21;` |
| `ADD i64` | GetValue×2 + FAST_SET + 寄存器写 | `zr_aot_s2=zr_aot_s0+zr_aot_s1;` |
| `COPY struct` | layout 解析 + CopyInline 调用 | `memcpy(&zr_aot_s1,&zr_aot_s0,sizeof(ZrLayout_42));`（blittable） |
| `RETURN i64`（typed→typed） | 封 SZrValue 写 returnDestination | `return zr_aot_s0;` |

---

## 8. 观测 / 调试：从「默认全开」改为「默认全关、按需开」

当前 prologue 默认 `observationMask = MAY_THROW|CONTROL_FLOW|CALL|RETURN`、并在行 hook 时
`publishAllInstructions = TRUE`，这让**每个**生成函数都背负观测成本。改为：

- **release/typed 默认 `observationPolicy = NONE`**：不发 PC、不发 hook、不维护 `lastObserved*`。
- **debug 构建或显式策略**（`state->hasAotObservationPolicyOverride`）才发观测代码，
  且**集中在边界**（call/return/throw/回边），不再逐指令。
- 行号/异常源定位改为**侧表**（`methodInfo.metadataFunction` + 指令区间→行 映射），
  异常展开时**按 PC 反查**，而非热路径逐指令写 `programCounter`。
- 即：观测从「运行期持续发布」改为「需要时反查」，热路径零成本。

---

## 9. 验收口径（本文专属，叠加 `06` DoD）

1. **环境零注入**：typed 函数 C 产物中
   `grep -E 'zr_aot_begin_instruction|frame\.slotBase|state->stackTop|programCounter|publishAllInstructions|Debug_Hook'`
   命中 = 0。
2. **零双写**：typed 函数体内 `grep -E 'SZrTypeValue|->ownershipKind|ZR_VALUE_FAST_SET|ZrCore_Ownership_ReleaseValue'`
   命中 = 0（边界 marshaling 模板除外，且边界代码与函数体物理分离、可被 grep 排除）。
3. **行数基准**：`var left:int=21;` 生成 == 1 条赋值语句；典型 typed 标量函数的生成 C 行数
   ≈ 其 SemIR 指令数（±常数），不再是数十倍膨胀。
4. **MethodInfo 唯一描述**：每函数恰一个 `static const SZrAotMethodInfo`；纯标量函数热路径不读它。
5. **SZrValue 计数**：全 typed 基准程序运行期 `SZrTypeValue` 构造次数 = 0（仪表化计数器断言）。
6. **性能门槛（硬性，回应「不得慢于解释器」）**：典型 typed 标量/循环基准，AOT 产物
   wall-clock **显著快于**解释器（目标 ≥ 3×，且不低于 1× 为最低红线），并显著快于现状半降级 AOT。
7. **GC 正确**：引用寄存器经帧根表对 GC 可见；GC 压力测试不丢引用、不误回收（衔接 `05`§3）。

---

## 10. 落地切片（独立先行里程碑 M1.5，M2 的硬前置；见 `06`）

> 本文 07-S1..S7 构成独立里程碑 **M1.5 · 环境隔离与去双写**，排在 M1 之后、M2 之前。
> 理由：必须先有「寄存器=C 局部、零解释器环境、零双写」的执行环境，M2 的标量算术才能
> 直接写裸 C；否则裸 C 运算仍被包在 `begin_instruction` + SZrValue 双写里，性能无从兑现。

| 切片 | 内容 | 验收 |
|---|---|---|
| 07-S1 | 删除 typed 函数体内 `zr_aot_begin_instruction` 发射（`backend_aot_c_lowering_control.c`），观测改 §8 默认 NONE | 产物 grep 无 begin_instruction；解释器路径不受影响 |
| 07-S2 | `CONST`/标量写改单写寄存器，删除 `frame.slotBase[..]` SZrValue 写（`backend_aot_c_lowering_values.c`/`scalar_locals.c`） | `var x=21` → 1 行；grep 无 SZrValue 写 |
| 07-S3 | 引入 `SZrAotMethodInfo` + 每函数发射常量；`frame` 胖结构装配从 typed 函数移除（`backend_aot_c_frame_setup.c`/`frame_cleanup.c`） | typed 纯标量函数 prologue 为空；编译通过 |
| 07-S4 | byte-frame 收窄到「GC/可寻址槽」，纯标量零字节帧（§3.3） | 纯标量函数 `registerFrameBytes==0` |
| 07-S5 | 边界 marshaling 模板化（§6），SZrValue 只现于 4 类边界；typed→typed 直调直返 | typed→typed 调用 grep 无 SZrValue；跨边界用例一致 |
| 07-S6 | GC 引用寄存器直用 object gc 接口 + 帧根表登记（衔接 05-S1/S2） | GC 压力测试 GREEN；引用寄存器可见 |
| 07-S7 | 反退化 CI：§9.1/§9.2 grep + §9.5 SZrValue 计数 + §9.6 性能门槛 | 违规/退化使构建失败 |

---

## 11. 不变量校验

- **不变量 A（确定性）**：寄存器单一静态类型由 `semIrTypeTable` 保证；本文不引入运行期类型分支。
- **不变量 B（纯降级）**：函数体只剩 `+-*/ = memcpy . -> if goto`；runtime 调用仅在
  分配/写屏障/safepoint/边界 box-unbox/throw/deopt，全在 `04`§4 白名单内。
- **不变量 C（单一真相）**：寄存器类型、GC 根布局、签名全部来自唯一 `SZrTypeLayout` +
  `SZrAotMethodInfo`（后者由 layout/原型派生），无散落硬编码。
- **不变量 D（环境隔离）**：本文即其定义与落地——typed 函数体内零解释器环境符号。

---

##状态与产出记录

- 2026-06-24 05:06:03 +08:00 · M1.5 / 07-S5 script typed-local return route ·
  状态：子切片完成、07-S5 typed return boundary route 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：script-level bool/u64/f64 typed-local return 现在复用 inferred scalar proof helper，
  在 callable return metadata 缺失时也能走 `ZrLibrary_AotRuntime_ReturnBool` / `ReturnU64` / `ReturnF64`
  native→VM typed return helper，而不是 generic `ZrLibrary_AotRuntime_Return(state, &frame, slot, ...)`。
  `backend_aot_c_typed_return.c` 对 bool/u64/f64 route 同时接受 direct callable proof 与 inferred script proof；
  callable metadata gate 仍保留在 scalar-local direct-return proof 中。RED/GREEN：focused method-info signature
  test 先因缺失 `ReturnBool(state, zr_aot_b...)` 失败 1/1；实现后 method-info signature 1/0。
  补充验证通过 return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0；
  generated bool/u64/f64 grep 分别命中 `ReturnBool`/`ReturnU64`/`ReturnF64`，generic frame-slot return grep
  无命中；CTest `aot_c_method_info_signature` 1/1 passed。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-script-typed-local-return-route.md`。规模：typed return 56 lines、
  method-info signature test 153 lines、return contract 385 lines。备注：本切片只补 typed-local script return
  runtime route；表达式直接返回标量化、full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance、
  性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 04:38:40 +08:00 · M1.5 / 07-S5 MethodInfo scalar return signatures ·
  状态：子切片完成、07-S5 MethodInfo/signature boundary metadata 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：在 i64 script-level return signature 的基础上，补齐 bool/u64/f64 typed-local
  script return 的 inferred MethodInfo signature。`backend_aot_c_emitter.c` 现在通过通用 scalar
  return 推断器按 i64/bool/u64/f64 顺序扫描所有 `FUNCTION_RETURN`，只有所有返回都能由同一标量 proof
  证明时才合成 `SZrAotSignature.returnType`；`backend_aot_c_scalar_locals.c` 抽出共享
  `can_return_kind_local`，typed callable direct-return 仍要求 callable return metadata，script
  signature inference 使用不要求 callable metadata 的 bool/u64/f64 proof helper。RED/GREEN：
  focused method-info signature test 先因 bool return 缺失
  `.returnType = &zr_aot_signature_0_types[0]` 失败 1/1；实现后 method-info signature 1/0。
  补充验证通过 frame setup contracts 1/0、source contracts 19/0、return contracts 1/0、typed scalar 1/0；
  CTest `aot_c_method_info_signature` 1/1 passed；generated bool/u64/f64 signature grep 分别命中
  `baseType/staticCType=1/9/11`、return pointer 与 `hasReturnValue=1`。产出：
  `tests/acceptance/2026-06-24-aot-m1-5-method-info-scalar-return-signatures.md`。规模：emitter 631 lines、
  scalar locals 2942 lines、scalar locals header 75 lines、new method-info signature test 140 lines、
  frame setup contract 362 lines、return contract 382 lines。备注：本切片只补 typed-local script return
  metadata；full typed ABI、inline structs、in/out writeback、表达式直接返回标量化、完整 07-S5 acceptance、
  性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 04:00:05 +08:00 · M1.5 / 07-S5 MethodInfo typed return signature ·
  状态：子切片完成、07-S5 MethodInfo/signature boundary metadata 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_emitter.c` 在 `function->hasCallableReturnType` 缺失时，
  通过 `SZrAotExecIrFunction` 扫描 `FUNCTION_RETURN` 并复用
  `backend_aot_c_scalar_locals_can_direct_return_i64_local(...)` proof，为 script-level i64
  direct return 生成非空 `SZrAotSignature.returnType`、`hasReturnValue=1` 和 i64 signature type row；
  mixed/unknown return 仍保持 `ZR_NULL`。RED/GREEN：typed-scalar generated-product 先因缺失
  `.returnType = &zr_aot_signature_0_types[0]` 失败 1/1；实现后 focused typed scalar 1/0。
  补充验证通过 frame setup contracts 1/0、source contracts 19/0、return contracts 1/0、
  typed scalar 1/0；generated typed scalar 的 07§9 environment grep 与 SZrValue/double-write grep
  均无命中，signature grep 命中 return pointer、`hasReturnValue=1`、i64 base/static C type。
  产出：`tests/acceptance/2026-06-24-aot-m1-5-method-info-typed-return-signature.md`。规模：
  emitter 550 lines、typed scalar 1101 lines、frame setup contract 344 lines。备注：本切片只补
  i64 script-level return metadata；full typed ABI、inline structs、in/out writeback、bool/u64/f64
  script-level inferred signatures、完整 07-S5 acceptance、性能/SZrValue 计数和 08-12 仍未完成。

- 2026-06-24 03:34:00 +08:00 · M1.5 / 07-S5 boundary guardrail allowlist hardening ·
  状态：支撑子切片完成、07-S5 acceptance/guardrail 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`tests/parser/test_aot_c_guardrail_contracts.c` 的 runtime-call classifier
  显式允许当前 07-S5 VM↔native 边界 helper：`ReturnI64` / `ReturnBool` / `ReturnU64` /
  `ReturnF64` / `ReturnInlineStruct`、`Sync*Local`、`CallStackValue`、`CallStaticDirect`、
  `CallInlineStruct`、`CallDynamicDeoptBridge`、`ValidateDynamicDeoptBridge` 以及
  dynamic member/index `Get*` / `Set*` helpers；同时继续拒绝 `ZrCore_Stack_GetValue`、
  `ZR_VALUE_FAST_SET` 和 `ZrLibrary_AotRuntime_Add` 这类未分类 VM fallback。RED/GREEN：
  guardrail 合约先因新增 07-S5 boundary helper 未在 allowlist 中分类而失败 1/4；
  扩展 allowlist 后 focused guardrail contracts 4/0。补充验证通过 source contracts 19/0、
  guardrail contracts 4/0、global contracts 7/0、return contracts 1/0、call contracts 4/0、
  typed-call contracts 4/0、dynamic deopt bridge smoke 2/0。`git diff --check` 对 guardrail
  文件退出 0，仅提示既有 LF/CRLF 规范化警告。规模：`test_aot_c_guardrail_contracts.c`
  159 physical / 139 non-empty lines。备注：本切片只收紧 07-S5 acceptance 护栏，不改变
  generated C 行为；07-S5 full typed ABI、inline structs、in/out writeback、完整 07-S5
  acceptance 和 08-12 仍未完成。

- 2026-06-24 03:24:02 +08:00 · M1.5 / 07-S5 dynamic value-access deopt bridge surfacing ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：AOT C dynamic member/index value-access 边界 writer 现在接收 ExecIR/SemIR
  `deoptId`，在 `GET_MEMBER` / `SET_MEMBER` / `GET_MEMBER_SLOT` /
  `SET_MEMBER_SLOT` / `GET_BY_INDEX` / `SET_BY_INDEX` runtime boundary 前生成
  `zr_aot_value_dynamic_deopt_bridge deopt=...` marker，并调用
  `ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(...)` 校验可见 SemIR deopt id。
  `ZrLibrary_AotRuntime_CallDynamicDeoptBridge(...)` 改为复用同一校验入口后继续委托
  `CallStackValue`，成员/索引实际语义仍由原 `GetMember` / `SetMember` /
  `GetByIndex` / `SetByIndex` helper 执行。RED/GREEN：global contracts 先因缺失
  `ValidateDynamicDeoptBridge` 和 value-access deopt marker 失败 1/7；实现后 focused
  global contracts 7/0、dynamic deopt bridge smoke 2/0。补充验证通过 source contracts 19/0、
  global contracts 7/0、dynamic deopt bridge smoke 2/0、global shared-library smoke 9/0、
  SemIR dynamic member deopt 1/0、SemIR dynamic index deopt 1/0。`git diff --check`
  对本切片相关 tracked 文件退出 0，仅提示既有 LF/CRLF 规范化警告。规模：
  `backend_aot_c_value_access_boundaries.c` 201 physical / 180 non-empty lines，
  `test_aot_c_dynamic_deopt_bridge_smoke.c` 337 / 288，
  `test_aot_c_global_contracts.c` 716 / 666。仍未完成：07-S5 full typed ABI、
  inline structs、in/out writeback、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 02:53:01 +08:00 · M1.5 / 07-S5 dynamic/deopt bridge surfacing ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：AOT C dynamic call 边界 writer 现在接收 SemIR/ExecIR `deoptId`，生成
  `zr_aot_dynamic_deopt_bridge deopt=...` marker，并通过
  `ZrLibrary_AotRuntime_CallDynamicDeoptBridge(...)` 进入运行时；普通 generic
  `fn(value)` direct-call 路径继续走 `CallStackValue`，不误标为 deopt bridge。runtime helper
  在可见 SemIR deopt table 时校验 deopt id，缺少 metadata 或 NONE 哨兵时保持兼容并委托
  `CallStackValue`。新增独立 `test_aot_c_dynamic_deopt_bridge_smoke.c`，手工构造
  `FUNCTION_CALL` SemIR dynamic deopt fixture，验证生成 C 文本与 shared-library 链接；原 call smoke
  回到 5 项普通调用路径覆盖。RED/GREEN：call contracts 先因缺失 `TZrUInt32 deoptId`
  与 bridge 文本失败 1/4；实现后 call contracts 4/0。验证：重新配置 `build-wsl-gcc` 后，
  call contracts 4/0、call shared-library smoke 5/0、dynamic deopt bridge smoke 1/0；
  相关回归 source contracts 19/0、global contracts 7/0、SemIR dynamic call deopt 1/0。
  `git diff --check` 对本切片相关 tracked 文件通过。规模：`backend_aot_c_call_boundaries.c`
  233 physical / 223 non-empty lines，`test_aot_c_call_shared_library_smoke.c` 拆回 898 行，
  新 bridge smoke 189 行。仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、
  broader dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 02:04:55 +08:00 · M1.5 / 07-S5 value-access boundary writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_value_access_boundaries.c`，从
  `backend_aot_c_lowering_values.c` 迁出 unsupported meta/dynamic value-access 边界 writer、
  `GET_MEMBER` / `SET_MEMBER` / `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT` /
  `GET_BY_INDEX` / `SET_BY_INDEX` 动态运行时边界 writer；value lowering 文件继续保留
  `TO_STRING`、常量/物化、所有权和其他值 lowering。RED/GREEN：global contracts 先读取缺失的
  value-access boundary 模块并按预期 2/7 `Expected Non-NULL`，拆分后 focused global contracts 7/0。
  验证：`zr_vm_parser_shared` 构建通过并确认
  `backend_aot_c_value_access_boundaries.c.o` 编入 parser shared；相关合约/烟测通过 global contracts 7/0、
  global shared-library smoke 9/0、source contracts 19/0。结构检查确认 value lowering 不再持有
  dynamic/member/index/meta value-access 边界 writer 字符串。规模：`backend_aot_c_lowering_values.c`
  降至 839 physical / 755 non-empty lines，新 value-access boundary source 165 / 147，
  global contract 703 / 653。仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、
  deopt/dynamic bridges、broader dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 01:47:59 +08:00 · M1.5 / 07-S5 call-boundary writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_call_boundaries.c`，从 `backend_aot_c_lowering_calls.c`
  迁出 generic/dynamic `CallStackValue` 边界 writer、static resolved `CallStaticDirect` 边界 writer
  以及 i64/bool/u64/f64 scalar-local call-result sync marker/helper 发射；原 lowering 文件继续保留
  unsupported meta call 和具体 typed direct-call scalar writer。RED/GREEN：call contract 先读取缺失的
  call-boundary 模块并按预期 3/4 `Expected Non-NULL`，迁出后 focused call contracts 4/0。验证：
  `zr_vm_parser_shared` 构建通过并确认 `backend_aot_c_call_boundaries.c.o` 编入 parser shared；相关
  AOT 合约/烟测分组通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、global 7/0、
  logical 4/0、frame setup 1/0、return 1/0、value SemIR 4/0、shared-library 8/0、call smoke 5/0、
  typed direct-call 5/0、bool 28/0、u64 25/0。备注：focused call-contract GREEN 末尾出现 CMake
  `GLOB mismatch`，后续 parser shared 构建完成重新生成并编译新对象；结构检查确认 lowering 文件不再持有
  `CallStackValue` / `CallStaticDirect` / scalar sync helper 发射字符串。规模：
  `backend_aot_c_lowering_calls.c` 降至 481 physical / 455 non-empty lines，新 call-boundary source
  185 / 177，call contract 424 / 386。仍未完成：07-S5 full typed ABI、inline structs、
  in/out writeback、deopt/dynamic bridges、dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 01:30:26 +08:00 · M1.5 / 07-S5 typed-return route split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_return.{h,c}`，从 `backend_aot_c_function_body.c`
  迁出 `FUNCTION_RETURN` 的 i64/bool/u64/f64 scalar typed-return route 判定和 typed native-to-VM
  return writer 调用；函数体现在只委托 `backend_aot_try_write_c_typed_return(...)`，
  generic/value-SemIR fallback 和普通 direct return boundary 保留在函数体。RED/GREEN：
  return contract 先读取缺失的 typed-return route 模块并按预期 `Expected Non-NULL`，迁出后
  focused return contracts 1/0。验证：相关 AOT 合约/烟测分组通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、global 7/0、logical 4/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared-library 8/0、call smoke 5/0、typed direct-call 5/0、bool 28/0、
  u64 25/0；`zr_vm_parser_shared` 构建通过并确认 `backend_aot_c_typed_return.c.o`
  编入 parser shared。备注：一次 all-in-one broad build/test 命令在 304s 超时且当时仍在长 smoke
  运行，未作为失败证据；随后拆分执行全部通过。规模：`backend_aot_c_function_body.c`
  2082 physical / 2034 non-empty lines，新 typed-return source 50 / 42，header 14 / 10，
  return contract 370 / 344。仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、
  deopt/dynamic bridges、dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 01:03:26 +08:00 · M1.5 / 07-S5 i64 typed-direct route proof split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_direct_i64_calls.{h,c}`，从
  `backend_aot_c_typed_direct_calls.c` 迁出 i64 no/one/two/three-arg typed-direct can-write
  route proof；顶层 typed-direct 文件删除旧的私有 function-table 查表 helper 与 i64 静态 proof，只保留
  unified typed-direct dispatch、result sync 判定和 writer 调用。RED/GREEN：typed-call i64 合约先改为读取缺失的
  `backend_aot_c_typed_direct_i64_calls.{h,c}`，`zr_vm_aot_c_typed_call_contracts_test`
  按预期在 i64 合约处 `Expected Non-NULL`；迁出后 focused typed-call contracts 4/0、i64 typed
  direct-call shared-library smoke 5/0。验证：相关 AOT 合约/烟测串联执行通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、global 7/0、logical 4/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  shared-library 8/0、call smoke 5/0、typed direct-call 5/0、bool 28/0、u64 25/0。备注：focused
  typed-call GREEN 末尾出现 CMake `GLOB mismatch`，这是新增源码进入 glob 后的重新生成提示；i64 smoke
  构建日志确认 `backend_aot_c_typed_direct_i64_calls.c.o` 编入 `zr_vm_parser_shared`。规模：
  `backend_aot_c_typed_direct_calls.c` 降至 467 physical / 436 non-empty lines，新 i64 route source
  152 / 132，header 43 / 40，i64 typed-call 合约 302 / 297。仍未完成：07-S5 full typed ABI、
  inline structs、in/out writeback、deopt/dynamic bridges、general typed-return ABI、
  dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 00:50:03 +08:00 · M1.5 / 07-S5 bool typed-direct route proof split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_direct_bool_calls.{h,c}`，从
  `backend_aot_c_typed_direct_calls.c` 迁出 bool no/one/two/three-arg typed-direct can-write
  route proof 与 i64 参数到 bool 结果的 two-arg route proof；顶层 typed-direct 文件新增 bool route
  header include，并保留统一调度与 direct-call writer 调用，匹配既有 u64/f64 route split 形态。
  RED/GREEN：typed-call bool 合约先改为读取缺失的
  `backend_aot_c_typed_direct_bool_calls.{h,c}`，`zr_vm_aot_c_typed_call_contracts_test`
  按预期在 bool 合约处 `Expected Non-NULL`；迁出后 focused typed-call contracts 4/0、bool typed
  direct-call shared-library smoke 28/0。验证：相关 AOT 合约/烟测串联执行通过 source 19/0、
  call 4/0、typed call 4/0、constant 5/0、global 7/0、logical 4/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared-library 8/0、call smoke 5/0、typed direct-call 5/0、bool 28/0、u64 25/0。
  备注：第一次 focused GREEN 构建命令因工具 129s 超时被切断，延长超时后同一目标通过；bool smoke
  构建日志确认 `backend_aot_c_typed_direct_bool_calls.c.o` 编入 `zr_vm_parser_shared`。规模：
  `backend_aot_c_typed_direct_calls.c` 降至 610 physical / 560 non-empty lines，新 bool route source
  189 / 165，header 53 / 50，bool typed-call 合约 394 / 388。仍未完成：07-S5 full typed ABI、
  inline structs、in/out writeback、deopt/dynamic bridges、general typed-return ABI、
  dynamic value access hardening、完整 07-S5 acceptance 和 08-12。

- 2026-06-24 00:32:08 +08:00 · M1.5 / 07-S5 bool call lowering writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_lowering_typed_bool_calls.c`，迁出
  `backend_aot_write_c_static_direct_bool_{no,one,two,three}_arg_function_call()` 以及
  `i64/u64/f64 -> bool` two-arg typed direct-call lowering writer；`backend_aot_c_lowering_calls.c`
  回落为 generic/dynamic call boundary、static VM-boundary call、i64/u64/f64 direct-call writer
  聚合，不再承载 bool typed direct-call writer。RED/GREEN：typed-call bool 合约先改为读取缺失的
  `backend_aot_c_lowering_typed_bool_calls.c`，`zr_vm_aot_c_typed_call_contracts_test`
  按预期在 bool 合约处 `Expected Non-NULL`；迁出后 focused typed-call contracts 4/0、
  bool typed direct-call shared-library smoke 28/0。验证：干净串联执行相关 AOT 合约/烟测通过
  source 19/0、call 4/0、typed call 4/0、constant 5/0、global 7/0、logical 4/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、shared-library 8/0、call smoke 5/0、
  typed direct-call 5/0、bool 28/0、u64 25/0。备注：第一次脚本化汇总命令因 PowerShell/WSL
  换行与变量展开干扰未作为退出码证据，最终使用直接串联二进制命令取得 exit 0。规模：
  `backend_aot_c_lowering_calls.c` 降至 664 physical / 630 non-empty lines，新 bool
  lowering source 为 272 / 257，bool typed-call contract 为 350 / 344；growth watch 仍包括
  `backend_aot_c_typed_u64_thunks.c` 832 / 748。仍未完成：07-S5 full typed ABI、inline
  structs、in/out writeback、deopt/dynamic bridges、general typed-return ABI、dynamic value
  access hardening、完整 07-S5 acceptance 以及 08-12。

- 2026-06-24 00:08:20 +08:00 · M1.5 / 07-S5 bool two-arg thunk writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_bool_two_arg_thunks.{h,c}`，迁出 plain bool/bool
  two-arg equal/not-equal/logical-and/logical-or recognizer、can-emit、forward declaration writer
  与 definition writer；`backend_aot_c_typed_bool_thunks.c` 保留 no/one-arg、三参委托以及
  i64/u64/f64 比较 writer，并通过 `backend_aot_c_try_write_bool_two_arg_thunk_definition()`
  委托二参 bool/bool 写入。RED/GREEN：typed-call 合约先读取缺失的 bool two-arg thunk 模块，
  `zr_vm_aot_c_typed_call_contracts_test` 按预期在 bool 合约处 `Expected Non-NULL`；迁出后
  focused typed-call contracts 4/0、bool smoke 28/0、u64 smoke 25/0。验证：迁出后、清理重复
  null guard 前补充 AOT 合约/烟测组通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float 1/0、shared-library 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg 8/0、bool 28/0、u64 25/0、f64 19/0、typed arithmetic 7/0、typed bitwise 6/0、
  global 9/0、logical smoke 4/0、power smoke 1/0、value-type 1/0、generic numeric smoke 1/0、
  float smoke 1/0；随后仅删除一条重复 `function->instructionsList == ZR_NULL` guard，并重跑
  focused typed-call contracts 4/0 与 bool smoke 28/0。备注：一次整组重跑因 f64 generated
  shared-library 编译超过工具超时窗口而未作为证据使用，残留进程已自然结束。规模：
  `backend_aot_c_typed_bool_thunks.c` 降至 673 physical / 587 non-empty lines，
  新二参 source 为 298 / 257，header 为 12 / 8；growth watch 仍包括
  `backend_aot_c_lowering_calls.c` 933 / 885 与 `backend_aot_c_typed_u64_thunks.c` 832 / 748。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  general typed-return ABI、dynamic value access hardening、完整 07-S5 acceptance 以及 08-12。

- 2026-06-23 23:32:28 +08:00 · M1.5 / 07-S5 u64 three-arg thunk writer split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_u64_three_arg_thunks.{h,c}`，迁出 u64 three-arg
  typed thunk can-emit、forward declaration writer 与 add/multiply/subtract/divide/modulo/bitwise
  definition writer；`backend_aot_c_typed_u64_thunks.c` 保留 no/one/two-arg writer 和总入口，
  对三参 thunk 仅委托新模块。RED/GREEN：typed-call 合约先要求缺失的
  `backend_aot_c_typed_u64_three_arg_thunks.{h,c}`，`zr_vm_aot_c_typed_call_contracts_test`
  按预期在 u64 合约处 `Expected Non-NULL`；迁出后 focused typed-call contracts 4/0、
  u64 smoke 25/0、bool smoke 28/0。验证：补充 AOT 合约/烟测组通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0、shared-library 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 8/0、bool 28/0、u64 25/0、f64 19/0、typed arithmetic 7/0、
  typed bitwise 6/0、global 9/0、logical smoke 4/0、power smoke 1/0、value-type 1/0、
  generic numeric smoke 1/0、float smoke 1/0。备注：第一次 focused 验证因两个目标并行构建同一
  build 目录超时，已清理残留进程并串行重跑通过；宽验证第一次停在旧 target 名，改为实际
  `typed_direct_call_arithmetic/bitwise` 名称后剩余项通过。规模：`backend_aot_c_typed_u64_thunks.c`
  降至 832 physical / 748 non-empty lines，新增三参 source 为 126 / 109，header 为 12 / 8。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  general typed-return ABI、dynamic value access hardening、完整 07-S5 acceptance 以及 08-12。

- 2026-06-23 22:51:07 +08:00 · M1.5 / 07-S5 u64 typed-direct route split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_direct_u64_calls.{h,c}`，迁出 u64 no/one/two/three-arg
  typed-direct can-write proof 与 u64->bool two-arg route proof；`backend_aot_c_typed_direct_calls.c`
  保留顶层调度和 writer 调用，不改变生成语义。typed-call 合约改为读取 u64 direct-route
  模块，bool 合约同步锁定 u64->bool route proof 的归属。RED/GREEN：先让合约读取尚不存在的
  `backend_aot_c_typed_direct_u64_calls.c`，`zr_vm_aot_c_typed_call_contracts_test`
  按预期在 u64 no-arg 合约处 `Expected Non-NULL`；迁出实现后 focused typed-call contracts
  4/0，u64 smoke 25/0，bool smoke 28/0。验证：补充 AOT 合约/烟测组通过 source 19/0、call 4/0、
  typed call 4/0、constant 5/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0、shared-library 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg 8/0、bool 28/0、u64 25/0、f64 19/0、arithmetic 7/0、
  bitwise 6/0、global 9/0、logical smoke 4/0、power smoke 1/0、value-type 1/0；generic numeric
  smoke 初次执行暴露 stale 目标，单独重建后 1/0 通过。规模：`backend_aot_c_typed_direct_calls.c`
  降至 771 physical / 702 non-empty lines，新增 u64 direct module 为 189 / 165，header 为 53 / 50。
  仍未完成：07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、
  general typed-return ABI、dynamic value access hardening、完整 07-S5 acceptance 以及 08-12。

- 2026-06-23 22:20:33 +08:00 · M1.5 / 07-S5 typed-call contract file split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：将到达 1000 行边界的 `tests/parser/test_aot_c_typed_call_contracts.c`
  拆为 16 行汇总入口，并把 i64、bool、u64、f64 四组 typed-call source-shape 合约分别迁入
  `test_aot_c_typed_call_i64_contracts.c`、`test_aot_c_typed_call_bool_contracts.c`、
  `test_aot_c_typed_call_u64_contracts.c`、`test_aot_c_typed_call_f64_contracts.c`。
  新增 `aot_c_typed_call_contract_cases.h` 管理四个用例原型，保留既有
  `aot_c_typed_call_contract_support.h` 的 repo 文件读取和 needle 断言 helper；
  `zr_vm_aot_c_typed_call_contracts_test` 目标名不变，CMake 只把四个拆分源编进同一聚合测试。
  本切片不新增 AOT 生成行为。RED/GREEN：RED 为先接入 CMake 后构建失败，明确缺少
  `test_aot_c_typed_call_i64_contracts.c`；补齐拆分文件后 `cmake --build ... --target
  zr_vm_aot_c_typed_call_contracts_test` 和聚合测试 GREEN，typed call contracts 4/0。
  首次 GREEN 暴露入口 include 支撑 helper 与用例缺原型的 warning，补 cases header 后重建无该警告。
  验证：focused typed call contracts 4/0、bool typed direct-call smoke 28/0；补充 AOT 合约/烟测组通过
  source 19/0、call 4/0、typed call 4/0、constant 5/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；
  shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、
  bool 28/0、u64 25/0、f64 19/0、arithmetic 7/0、bitwise 6/0、global 9/0、logical 4/0、
  power 1/0、value-type 1/0。`git diff --check` scoped 通过，仅有既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-typed-call-contract-file-split.md`。
  文件规模：typed-call 汇总入口 16 physical / 12 non-empty lines，i64 contract 260 / 255，
  bool contract 308 / 302，u64 contract 224 / 218，f64 contract 207 / 201，
  cases header 9 / 7，support header 97 / 78。备注：该支撑拆分为继续扩展 07-S5 typed-call /
  ABI 边界清理测试容量；general typed-return ABI、inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 21:45:12 +08:00 · M1.5 / 07-S5 static bool three-arg logical-or
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool 三参 typed thunk 现在识别三 bool 参数 `arg0 || arg1 || arg2`
  的窄返回形态，覆盖 compact `LOGICAL_OR -> LOGICAL_OR -> FUNCTION_RETURN` 和当前源码
  `left || middle || right` 生成的 12 条短路指令形态（两段 `JUMP_IF_BOOL_FALSE` + `JUMP`）。
  `backend_aot_c_typed_bool_three_arg_thunks.c` 新增 logical-or recognizer/can-emit gate/writer，
  生成 `return (TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2);`；既有
  `zr_aot_static_bool_three_arg_direct_call` 调用路径复用三 bool scalar-local 参数证明，继续拒绝
  `CallStaticDirect` / `CallStackValue` fallback 和三参 stack-sync marker。RED/GREEN：RED 为
  typed call contracts 缺 `backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_or_return(`，
  bool smoke 新增 `either3(false, false, true)` 后缺三参 OR typed thunk 声明；补实现后 focused
  GREEN 为 typed call contracts 4/0、bool smoke 28/0。补充验证：contracts 组通过 source 19/0、
  call 4/0、typed call 4/0、constant 5/0、generic numeric 1/0、global 7/0、logical 4/0、
  power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library
  smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、
  arithmetic 7/0、bitwise 6/0、bool 28/0、u64 25/0、f64 19/0、global 9/0、logical 4/0、
  power 1/0、value-type 1/0。`git diff --check` scoped 通过，仅有既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-bool-three-arg-logical-or-typed-thunk.md`。
  文件规模：bool three-arg source 277 physical / 243 non-empty lines，typed call contract
  1000 / 979，bool smoke 790 / 733。备注：typed call contract 已到 1000 行边界；后续再扩
  该契约面时应优先拆分或迁移。仅完成 bool 三参 `||` typed direct-call；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 21:11:23 +08:00 · M1.5 / 07-S5 static bool three-arg logical-and
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：bool 三参 typed thunk 现在识别三 bool 参数 `arg0 && arg1 && arg2`
  的窄返回形态，生成 `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state,
  TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2)` 和
  `return (TZrBool)(zr_aot_arg0 && zr_aot_arg1 && zr_aot_arg2);`。`backend_aot_c_lowering_calls.c`
  新增 `zr_aot_static_bool_three_arg_direct_call` 路径，直接赋值到 bool scalar local，并继续拒绝
  `CallStaticDirect` / `CallStackValue` fallback 和三参 stack-sync marker。`backend_aot_callable_provenance.c`
  补齐 `GET_CLOSURE` / `GETUPVAL` callable slot provenance，`backend_aot_write_c_create_closure()`
  将可解析 capture 闭包转交 `ZrLibrary_AotRuntime_CreateClosure()`；`CopyConstant` / `CreateClosure`
  运行时 helper 在 `frame.recordHandle` 被 07-S3 省略时按当前函数反查 AOT module record，保持
  frame setup 合同不回退。RED/GREEN：RED 为 typed call contracts 缺
  `backend_aot_c_can_emit_typed_bool_three_arg_thunk(`，随后 bool smoke 暴露运行时闭包物化的
  frame-record fallback 缺口；补实现并把 smoke 恢复到现有顶层入口语义后，focused GREEN 为
  constant contracts 5/0、frame setup contracts 1/0、typed call contracts 4/0、bool smoke 27/0。
  补充验证：contracts 组通过 source 19/0、call 4/0、typed call 4/0、constant 5/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、call 5/0、
  typed direct-call 5/0、i64 three-arg 8/0、arithmetic 7/0、bitwise 6/0、bool 27/0、
  u64 25/0、f64 19/0、global 9/0、logical 4/0、power 1/0、value-type 1/0。生成文本检查：
  bool 三参生成 C 含 typed thunk 声明和 `zr_aot_typed_bool_fn_1(state, ...)` 直调，无
  `frame.recordHandle`、无 `ZrLibrary_AotRuntime_CallStaticDirect` / `CallStackValue`。说明：宽 smoke
  目标一次性构建受工具时限截断，最终证据采用分批构建和所有 smoke executable 拆批运行。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-three-arg-logical-and-typed-thunk.md`。
  文件规模：bool three-arg source 199 physical / 173 non-empty lines，bool three-arg header
  12 / 8，bool thunk writer 928 / 812，typed direct calls 932 / 844，call lowering 933 / 885，
  callable provenance 257 / 224，typed call contract 994 / 973，bool smoke 761 / 706，
  constant contract 404 / 359，runtime source 8619 / 7566。备注：仅完成 bool 三参 `&&`
  typed direct-call 及必要 closure materialization fallback；general typed-return ABI、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 19:45:07 +08:00 · M1.5 / 07-S5 static u64 three-arg modulo
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 三参 typed thunk 现在识别 `MOD_UNSIGNED -> MOD_UNSIGNED ->
  FUNCTION_RETURN` 的有序 `arg0 % arg1 % arg2` 窄形态；`backend_aot_c_try_read_u64_modulo_operands()`
  和 `backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return()` 复用三参 binary-return helper 的
  `preserveOperandOrder` 路径，保证取模不接受交换操作数；`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  纳入 modulo gate；`backend_aot_c_typed_u64_thunks.c` 新增三参 modulo writer，生成
  `zr_aot_arg1 == 0u || zr_aot_arg2 == 0u` 防护、`generated AOT unsigned three-arg modulo by zero`
  运行时错误和正常路径 `return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1 % zr_aot_arg2);`。
  u64 shared-library smoke 新增 `remainder3(92, 50, 43)`，继续拒绝 `CallStaticDirect` /
  `CallStackValue` fallback 和三参 stack-sync marker。RED/GREEN：RED 为 typed call contracts
  缺 `generated AOT unsigned three-arg modulo by zero`；补实现后 focused GREEN 为 typed call
  contracts 4/0、u64 smoke 25/0。补充验证：拆批验证通过；contracts 组通过 source 19/0、
  call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、
  call 5/0、typed direct-call 5/0、i64 three-arg 8/0、arithmetic 7/0、bitwise 6/0、bool 26/0、
  u64 25/0、f64 19/0、global 9/0、logical 4/0、power 1/0。说明：宽目标一次性/烟测批量构建
  受工具时限截断，最终证据采用 focused 构建、合约目标构建以及所有 smoke executable 拆批运行。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-modulo-typed-thunk.md`。
  文件规模：u64 thunk writer 912 physical / 824 non-empty lines，u64 shape source
  731 / 653，u64 shape header 23 / 20，typed call contract 935 / 914，u64 smoke
  634 / 583。备注：仅完成 u64 三参 modulo typed direct-call；u64 三参除法/取模窄算术对已覆盖，
  但 general typed-return ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 19:18:23 +08:00 · M1.5 / 07-S5 static u64 three-arg divide
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：u64 三参 typed thunk 现在识别 `DIV_UNSIGNED -> DIV_UNSIGNED ->
  FUNCTION_RETURN` 的有序 `arg0 / arg1 / arg2` 窄形态；`backend_aot_c_try_read_u64_divide_operands()`
  和 `backend_aot_c_try_get_u64_arg0_arg1_arg2_divide_return()` 复用三参 binary-return helper 的
  `preserveOperandOrder` 路径，保证除法不接受交换操作数；`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  纳入 divide gate；`backend_aot_c_typed_u64_thunks.c` 新增三参 divide writer，生成
  `zr_aot_arg1 == 0u || zr_aot_arg2 == 0u` 防护、`generated AOT unsigned three-arg divide by zero`
  运行时错误和正常路径 `return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);`。
  u64 shared-library smoke 新增 `quotient3(168, 2, 2)`，继续拒绝 `CallStaticDirect` /
  `CallStackValue` fallback 和三参 stack-sync marker。RED/GREEN：RED 为 typed call contracts
  缺 `generated AOT unsigned three-arg divide by zero`；补实现后 focused GREEN 为 typed call
  contracts 4/0、u64 smoke 24/0。补充验证：首次全链构建+测试命令在 304s 超时且无失败细节，
  随后拆批验证通过；目标构建通过；contracts 组通过 source 19/0、call 4/0、typed call 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float 1/0；shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、
  i64 three-arg 8/0、arithmetic 7/0、bitwise 6/0、bool 26/0、u64 24/0、f64 19/0、global 9/0、
  logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-divide-typed-thunk.md`。
  文件规模：u64 thunk writer 897 physical / 810 non-empty lines，u64 shape source
  710 / 635，u64 shape header 22 / 19，typed call contract 930 / 909，u64 smoke
  608 / 559。备注：仅完成 u64 三参除法 typed direct-call；u64 三参 modulo parity、
  general typed-return ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 18:55:27 +08:00 · M1.5 / 07-S5 static i64 three-arg modulo
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64 三参 typed thunk 现在识别 `MOD_SIGNED -> MOD_SIGNED ->
  FUNCTION_RETURN` 的有序 `arg0 % arg1 % arg2` 窄形态；`backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return()`
  复用三参 binary-return helper 的 `preserveOperandOrder` 路径，保证取模不接受交换操作数；
  `backend_aot_c_can_emit_typed_i64_three_arg_thunk()` 纳入 modulo gate；
  `backend_aot_c_typed_i64_thunks.c` 新增三参 modulo writer，生成
  `zr_aot_arg1 == 0 || zr_aot_arg2 == 0` 防护、`generated AOT signed three-arg modulo by zero`
  运行时错误和正常路径 `return (TZrInt64)(zr_aot_arg0 % zr_aot_arg1 % zr_aot_arg2);`。
  i64 三参 shared-library smoke 新增 `remainder3(92, 50, 43)`，继续拒绝 `CallStaticDirect` /
  `CallStackValue` fallback 和三参 stack-sync marker。RED/GREEN：RED 为 typed call contracts
  缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(`；补实现后 focused GREEN 为
  typed call contracts 4/0、i64 three-arg smoke 8/0。补充验证：较宽 AOT contracts 组通过
  source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、
  power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library smokes
  通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 8/0、arithmetic 7/0、
  bitwise 6/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-modulo-typed-thunk.md`。
  文件规模：i64 thunk writer 305 physical / 283 non-empty lines，i64 shape source
  819 / 714，i64 shape header 38 / 35，typed call contract 924 / 903，i64 three-arg smoke
  259 / 241。备注：仅完成 i64 三参 modulo typed direct-call；i64 三参除法/取模窄算术对已覆盖，
  但 general typed-return ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 18:35:09 +08:00 · M1.5 / 07-S5 static i64 three-arg divide
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64 三参 typed thunk 现在识别 `DIV_SIGNED -> DIV_SIGNED ->
  FUNCTION_RETURN` 的有序 `arg0 / arg1 / arg2` 窄形态；`backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return()`
  复用三参 binary-return helper 的 `preserveOperandOrder` 路径，保证除法不接受交换操作数；
  `backend_aot_c_can_emit_typed_i64_three_arg_thunk()` 纳入 divide gate；
  `backend_aot_c_typed_i64_thunks.c` 新增三参 divide writer，生成
  `zr_aot_arg1 == 0 || zr_aot_arg2 == 0` 防护、`generated AOT signed three-arg divide by zero`
  运行时错误和正常路径 `return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);`。
  i64 三参 shared-library smoke 新增 `quotient3(64, 4, 2)`，继续拒绝 `CallStaticDirect` /
  `CallStackValue` fallback 和三参 stack-sync marker。RED/GREEN：RED 为 typed call contracts
  缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(`；补实现后 focused GREEN 为
  typed call contracts 4/0、i64 three-arg smoke 7/0。补充验证：较宽 AOT contracts 组通过
  source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、
  power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library smokes
  通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 7/0、arithmetic 7/0、
  bitwise 6/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-divide-typed-thunk.md`。
  备注：仅完成 i64 三参除法 typed direct-call；modulo parity、general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 18:12:47 +08:00 · M1.5 / 07-S5 i64 no/one-arg thunk shape split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：保持生成行为不变，将 i64 no-arg constant-return 与 one-arg
  identity/negate/bitwise-not/bitwise-const/add-const/subtract-const/multiply-const
  shape recognizer 从 `backend_aot_c_typed_i64_thunks.c` 迁入
  `backend_aot_c_typed_i64_thunk_shapes.{h,c}`；writer 文件继续只负责 can-emit gate、
  forward declaration 和 thunk definition 发射。typed call contract 改为在 shape 源文件中
  锁定 no/one-arg shape 的 opcode/type/parameter 校验。RED/GREEN：RED 为 typed call
  contracts 缺 `backend_aot_c_try_get_i64_constant_return(` 于 shape 文件；迁移后第一次
  focused smoke 暴露 shape 文件缺 `backend_aot_c_get_constant_value()` 声明导致隐式声明与
  运行时崩溃，补 `backend_aot_c_emitter.h` 后 GREEN。focused GREEN 为 typed call
  contracts 4/0、i64 no/one/two-arg smoke 5/0、i64 three-arg smoke 6/0、arithmetic
  shared-library smoke 7/0、bitwise shared-library smoke 6/0。补充验证：较宽 AOT
  contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float 1/0；shared-library smokes 通过 shared 8/0、call 5/0、typed direct-call 5/0、
  i64 three-arg 6/0、arithmetic 7/0、bitwise 6/0、bool 26/0、u64 23/0、f64 19/0、
  global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-i64-no-one-arg-shape-split.md`。备注：
  仅完成行为保持的 i64 no/one-arg shape 拆分；general typed-return ABI、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 17:43:06 +08:00 · M1.5 / 07-S5 i64 two-arg thunk shape split ·
  状态：支撑子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：保持生成行为不变，将 i64 两参 typed thunk 的 add/subtract/multiply/divide/
  modulo/AND/OR/XOR shape recognizer 从 `backend_aot_c_typed_i64_thunks.c` 迁入
  `backend_aot_c_typed_i64_thunk_shapes.{h,c}`；writer 文件继续只负责 can-emit gate、
  forward declaration 和 thunk definition 发射，divide/modulo 的 zero-denominator guard
  保持在 writer。typed call contract 改为在 shape 源文件中锁定两参 shape 内部校验。
  RED/GREEN：RED 为 typed call contracts 缺
  `backend_aot_c_try_get_i64_arg0_arg1_add_return(` 于 shape 文件；迁移 recognizer 后
  focused GREEN 为 typed call contracts 4/0、i64 three-arg smoke 6/0、arithmetic
  shared-library smoke 7/0。补充验证：较宽 AOT contracts 组通过 source 19/0、call 4/0、
  typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0；shared-library smokes
  通过 shared 8/0、call 5/0、typed direct-call 5/0、i64 three-arg 6/0、arithmetic 7/0、
  bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-i64-two-arg-shape-split.md`。备注：
  仅完成行为保持的 i64 两参 shape 拆分；general typed-return ABI、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 17:11:47 +08:00 · M1.5 / 07-S5 static i64 two-arg modulo
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：先将 arithmetic shared-library smoke 从复制型 harness
  拆为 `aot_c_typed_direct_call_arithmetic_smoke_support.h` 共享 runner + case 数据，
  现有 6 个算术 smoke 保持 6/0；随后 i64 两参 typed thunk 识别 `MOD_SIGNED -> FUNCTION_RETURN`
  的 `return left % right` 窄形态，`backend_aot_c_can_emit_typed_i64_two_arg_thunk()`
  纳入 modulo gate，生成的 `zr_aot_typed_i64_fn_N(state, arg0, arg1)` 对
  `zr_aot_arg1 == 0` 调用 `ZrCore_Debug_RunError(state, "generated AOT signed modulo by zero")`
  并防御性返回 `(TZrInt64)0`，否则直接返回 `(TZrInt64)(zr_aot_arg0 % zr_aot_arg1)`。
  arithmetic shared-library smoke 新增 `remainder(left: int, right: int): int { return left % right; }`，
  要求 `zr_aot_static_i64_two_arg_direct_call`，并禁止 `CallStaticDirect`、`CallStackValue`
  和 typed-destination stack sync fallback。RED/GREEN：typed call contract 先因缺
  `backend_aot_c_try_get_i64_arg0_arg1_modulo_return(` 失败；新增 arithmetic smoke
  再因缺 i64 modulo typed thunk generated C 片段失败；补 recognizer/gate/writer 后 focused
  GREEN 为 typed call contracts 4/0、arithmetic shared-library smoke 7/0。补充验证：
  较宽 AOT contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float 1/0；shared-library smokes 拆批后通过 shared 8/0、call 5/0、typed direct-call 5/0、
  arithmetic 7/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-modulo-typed-thunk.md`。
  备注：仅完成 i64 两参 modulo 直调与必要 smoke harness 拆分；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 16:50:50 +08:00 · M1.5 / 07-S5 static i64 two-arg divide
  typed thunk direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64 两参 typed thunk 现在识别 `DIV_SIGNED -> FUNCTION_RETURN`
  的 `return left / right` 窄形态；`backend_aot_c_can_emit_typed_i64_two_arg_thunk()`
  纳入 divide gate；生成的 `zr_aot_typed_i64_fn_N(state, arg0, arg1)` 对
  `zr_aot_arg1 == 0` 调用 `ZrCore_Debug_RunError(state, "generated AOT signed divide by zero")`
  并防御性返回 `(TZrInt64)0`，否则直接返回 `(TZrInt64)(zr_aot_arg0 / zr_aot_arg1)`。
  arithmetic shared-library smoke 新增 `ratio(left: int, right: int): int { return left / right; }`，
  要求 `zr_aot_static_i64_two_arg_direct_call`，并禁止 `CallStaticDirect`、`CallStackValue`
  和 typed-destination stack sync fallback。RED/GREEN：typed call contract 先因缺
  `backend_aot_c_try_get_i64_arg0_arg1_divide_return(` 失败；新增 arithmetic smoke
  再因缺 i64 divide typed thunk generated C 片段失败；补 recognizer/gate/writer 后 focused
  GREEN 为 typed call contracts 4/0、arithmetic shared-library smoke 6/0。补充验证：
  较宽 AOT contracts 组通过 source 19/0、call 4/0、typed call 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  float 1/0；shared-library smokes 拆批后通过 shared 8/0、call 5/0、typed direct-call 5/0、
  arithmetic 6/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-divide-typed-thunk.md`。
  备注：仅完成 i64 两参除法直调；i64 modulo parity、general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 16:30:13 +08:00 · M1.5 / 07-S5 i64/bool/u64/f64 no-arg local-constant
  typed direct-call · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：i64/bool/u64/f64 no-arg typed thunk recognizer 现在接受
  `var result = constant; return result;` 产生的本地常量返回形态；i64/bool/f64 接受
  `GET_CONSTANT -> GET_STACK/SET_STACK copy -> FUNCTION_RETURN`，u64 额外接受
  copy 后接 `TO_UINT` / `TO_UINT_SIGNED` 再返回。对应 return-boundary smoke 现在要求
  `zr_aot_typed_i64/bool/u64/f64_fn_N` forward declaration、definition 与
  `zr_aot_static_*_no_arg_direct_call` call site，并禁止
  `ZrLibrary_AotRuntime_CallStaticDirect()` / `CallStackValue()` fallback；
  static numeric call shared-library smoke 也从旧 runtime fallback 断言改为要求 u64/f64
  no-arg typed thunk 直调；i64 no-arg typed direct-call smoke 也从直接常量返回收紧为
  本地常量返回。RED/GREEN：bool return-boundary smoke 先因缺 typed bool forward
  declaration 失败；i64 no-arg smoke 收紧后同样因缺 typed i64 forward declaration
  失败；补 recognizer 后 focused GREEN 为 i64 typed direct-call shared-library smoke
  5/0、bool smoke 26/0、u64 smoke 23/0、f64 smoke 19/0，call shared-library smoke
  5/0 对齐旧断言后通过。补充验证：较宽 AOT 组通过 source 19/0、call 4/0、
  typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、power 2/0、
  frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool 26/0、u64 23/0、f64 19/0、
  global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-no-arg-local-constant-typed-direct-call.md`。
  备注：仅完成 no-arg local-constant return shape 的 typed→typed 直调收紧；
  general typed-return ABI、inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 15:35:25 +08:00 · M1.5 / 07-S5 bool/u64/f64 direct-return
  frame descriptor gates · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_frame_descriptor.c` 的
  `FUNCTION_RETURN` local-only 判定不再只认 i64 direct return，在非 export tail 下同时复用
  `backend_aot_c_scalar_locals_can_direct_return_bool_local()`、
  `backend_aot_c_scalar_locals_can_direct_return_u64_local()` 和
  `backend_aot_c_scalar_locals_can_direct_return_f64_local()`；其余需要 frame 的指令仍按
  既有保守判定保留 frame descriptor。RED/GREEN：frame setup contract 先因缺 bool
  direct-return descriptor gate 失败；补 bool/u64/f64 gates 后 focused GREEN 为 frame setup
  contracts 1/0、bool typed direct-call shared-library smoke 26/0、u64 smoke 23/0、f64 smoke
  19/0。补充验证：较宽 AOT 组通过 source 19/0、call 4/0、typed call 4/0、generic
  numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float 1/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 26/0、u64 23/0、f64 19/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-bool-u64-f64-return-frame-descriptor-gates.md`。
  备注：仅完成 return-frame descriptor gate 对齐；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 15:17:34 +08:00 · M1.5 / 07-S5 static f64 native-to-VM return
  boundary helper · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：`ZrLibrary_AotRuntime_ReturnF64()` 进入 public AOT runtime
  header 与拆分后的 `aot_runtime_return.c`，用 `ZrCore_Value_InitAsFloat()` 把
  `TZrFloat64` 直接打包到 caller result；`backend_aot_write_c_direct_return_f64_local()`
  生成 `zr_aot_direct_return_f64_local` 和
  `ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f*)`；
  `backend_aot_c_scalar_locals_can_direct_return_f64_local()` 只在 callable return type
  可证明为 `float`、source slot 有 f64 local 且 written-before、无异常处理/导出槽/
  constructor 时开放；`FUNCTION_RETURN` 在 i64/bool/u64 fast path 后接入 f64 fast path。
  RED/GREEN：return contracts 先因缺 f64 writer prototype 失败；新增普通 static call
  进入 f64 callee 的 shared-library smoke 后 focused GREEN 为 return contracts 1/0、
  f64 typed direct-call shared-library smoke 19/0。补充验证：较宽 AOT 组通过 source
  19/0、call 4/0、typed call 4/0、generic numeric 1/0、global 7/0、logical 4/0、
  power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float 1/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool 26/0、u64 23/0、f64 19/0、global 9/0、
  logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-return-boundary-helper.md`。
  备注：仅完成窄 f64 native-to-VM return packing；general typed-return ABI、
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 14:59:10 +08:00 · M1.5 / 07-S5 static bool native-to-VM return
  boundary helper · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：`ZrLibrary_AotRuntime_ReturnBool()` 进入 public AOT runtime
  header 与拆分后的 `aot_runtime_return.c`，用 `ZrCore_Value_InitAsBool()` 把
  `TZrBool` 直接打包到 caller result；`backend_aot_write_c_direct_return_bool_local()`
  生成 `zr_aot_direct_return_bool_local` 和
  `ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b*)`；
  `backend_aot_c_scalar_locals_can_direct_return_bool_local()` 只在 callable return type
  可证明为 `bool`、source slot 有 bool local 且 written-before、无异常处理/导出槽/
  constructor 时允许该路径；`FUNCTION_RETURN` 在 i64 fast path 后、u64 fast path 前
  接入 bool fast path。RED/GREEN：return contracts 先因缺 bool direct-return writer
  prototype 得到 RED；bool shared-library smoke 新增普通 static call 进入 bool callee
  的 native-to-VM return 边界 case。focused GREEN 为 return contracts 1/0、bool
  typed direct-call shared-library smoke 26/0。补充验证：较宽 AOT 组通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool 26/0、
  u64 23/0、f64 18/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-return-boundary-helper.md`。
  备注：本切片只覆盖窄 bool native-to-VM return packing；f64/general typed-return
  ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 14:26:38 +08:00 · M1.5 / 07-S5 static u64 native-to-VM return
  boundary helper · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：`ZrLibrary_AotRuntime_ReturnU64()` 进入 public AOT runtime
  header 与拆分后的 `aot_runtime_return.c`，用 `ZrCore_Value_InitAsUInt()` 把
  `TZrUInt64` 直接打包到 caller result；`backend_aot_write_c_direct_return_u64_local()`
  生成 `zr_aot_direct_return_u64_local` 和
  `ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u*)`；
  `backend_aot_c_scalar_locals_can_direct_return_u64_local()` 只在 callable return type
  可证明为 `uint`、source slot 有 u64 local 且 written-before、无异常处理/导出槽/
  constructor 时允许该路径；`FUNCTION_RETURN` 在 i64 fast path 后接入 u64 fast path。
  RED/GREEN：先以 return contracts 缺 u64 writer prototype 得到 RED；初版 u64 proof
  过宽导致既有 int-return u64 smoke 误走 u64 return，收窄到 callable return type U64
  后修复；最终 focused GREEN 为 return contracts 1/0、u64 typed direct-call
  shared-library smoke 23/0。补充验证（2026-06-23 14:42:02 +08:00）：较宽 AOT
  组通过 source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric
  1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value
  SemIR 4/0、float contracts 1/0、shared 8/0、call smoke 5/0、typed direct-call
  5/0、bool 25/0、u64 23/0、f64 18/0、global 9/0、logical 4/0、power 1/0；
  u64 smoke support 结果断言改为按实际 signed/unsigned value type 读取，避免 unsigned
  返回结果被按 signed union 字段误读。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-return-boundary-helper.md`。
  备注：本切片只覆盖窄 u64 native-to-VM return packing；bool/f64/general typed-return
  ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridges、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 13:49:07 +08:00 · M1.5 / 07-S5 dynamic member/index runtime
  boundary helpers · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_lowering_values.c` 新增
  `backend_aot_write_c_direct_get_member()`、`backend_aot_write_c_direct_set_member()`、
  `backend_aot_write_c_direct_get_member_slot()`、
  `backend_aot_write_c_direct_set_member_slot()`、
  `backend_aot_write_c_direct_get_by_index()`、
  `backend_aot_write_c_direct_set_by_index()` 六个动态 member/index 边界 writer，
  生成 C 现在分别调用既有
  `ZrLibrary_AotRuntime_GetMember` / `SetMember` / `GetMemberSlot` /
  `SetMemberSlot` / `GetByIndex` / `SetByIndex`，不再对这些已具备 runtime helper
  的 opcode 生成 `UnsupportedDynamicValueAccess`。`backend_aot_c_function_body.c`
  将 `GET_MEMBER`、`GET_MEMBER_SLOT`、`GET_BY_INDEX`、`SET_MEMBER`、
  `SET_MEMBER_SLOT`、`SET_BY_INDEX` 分支切到上述 writer；GET 分支继续清空
  callable slot tracking，SET 分支保持写边界语义。支撑产出：
  `tests/parser/aot_c_typed_call_contract_support.h` 抽出 typed-call source contract
  共享 helper，`tests/parser/test_aot_c_typed_call_contracts.c` 从 982 / 944 回落到
  893 / 872。RED/GREEN：RED 为 global contracts 7 tests / 1 failure，缺
  `backend_aot_write_c_direct_get_member(FILE *file,`；global shared-library smoke
  新期望尚未满足。补实现后 focused GREEN 为 global contracts 7/0、global smoke
  9/0，typed call contracts 拆分后保持 4/0。测试结果：较宽 AOT 组通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、global 7/0、
  logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool 25/0、u64 22/0、f64 18/0、global 9/0、logical 4/0、power 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-dynamic-member-index-runtime-boundaries.md`。
  文件规模：lowering values 989 / 889，function body 2085 / 2037，emitter header
  805 / 800，global contracts 670 / 620，global smoke 1206 / 1048，typed call
  contract 893 / 872，typed call support 97 / 78。备注：dynamic member/index
  boundary helper 项已完成当前六 opcode 的显式 runtime helper 路由；inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、general typed-return ABI、07-S5
  完整验收和 08-12 仍未完成。

- 2026-06-23 13:05:52 +08:00 · M1.5 / 07-S5 static f64 two-arg
  comparison bool typed thunk family · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：在上一条 f64 `<` bool direct-call 路径基础上，
  `backend_aot_c_typed_bool_thunks.c` 补齐两 f64 参数返回 bool 的比较族识别和
  thunk writer，`backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(...)` 现在覆盖
  `<`、`<=`、`==`、`!=`、`>`、`>=`，对应 `LOGICAL_LESS_FLOAT`、
  `LOGICAL_LESS_EQUAL_FLOAT`、`LOGICAL_EQUAL_FLOAT`、`LOGICAL_NOT_EQUAL_FLOAT`、
  `LOGICAL_GREATER_FLOAT`、`LOGICAL_GREATER_EQUAL_FLOAT` + `FUNCTION_RETURN`
  窄形态，并发出直接 `TZrBool` 比较返回。调用侧继续复用
  `backend_aot_write_c_static_direct_f64_bool_two_arg_function_call()` 与
  `zr_aot_static_f64_bool_two_arg_direct_call`，以 `zr_aot_f*` 参数调用 bool thunk，
  只在需要时同步 bool 栈槽。测试补强：typed call contracts 增加 f64 比较族
  recognizer、opcode、writer 和 return 表达式 needles；bool shared-library smoke 新增
  `<=`、`==`、`!=`、`>`、`>=` 五个 f64 bool direct-call case。RED/GREEN：第一轮 RED 为
  typed call contracts 缺 `backend_aot_c_try_get_bool_f64_arg0_arg1_less_equal_return(`
  且 bool smoke 21 tests / 1 failure（新 `<=` case 为 `Expected Non-NULL`）；补 `<=`
  后 focused GREEN 为 typed call contracts 4/0、bool smoke 21/0。第二轮 RED 为
  contracts 缺 `backend_aot_c_try_get_bool_f64_arg0_arg1_equal_return(`，bool smoke
  25 tests / 4 failures（`==`、`!=`、`>`、`>=` 均为 `Expected Non-NULL`）；补齐后
  focused GREEN 为 typed call contracts 4/0、bool smoke 25/0。测试结果：WSL GCC debug
  宽构建通过；合约组 source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric 1/0、global 7/0、logical 4/0、power 2/0、frame setup 1/0、return
  1/0、value SemIR 4/0、float contracts 1/0；smoke 组在 300s 首次超时后废弃该输出并以
  900s 重跑，结果为 shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg
  6/0、bool 25/0、u64 22/0、f64 18/0、typed arithmetic 5/0、typed bitwise 6/0、
  value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-comparison-bool-typed-thunk.md`。
  文件规模：bool thunks 906 / 791，typed call contract 982 / 944，bool direct-call
  smoke 702 / 651，本计划 4878 / 4605，AOT index 5007 / 4862，C# value-type
  SemIR AOT doc 1178 / 810。备注：本切片只补齐窄 f64 两参 bool 比较族；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
  general typed-return ABI、07-S5 完整验收和 08-12 仍未完成。大文件备注：
  `tests/parser/test_aot_c_typed_call_contracts.c` 已接近 1000 行，后续再扩充 contract
  前应优先拆分 focused contract/support。

- 2026-06-23 12:45:38 +08:00 · M1.5 / 07-S5 static f64 two-arg less bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_bool_thunks.c` 新增
  `backend_aot_c_try_get_bool_f64_arg0_arg1_less_return()`，识别两 f64 参数
  `arg0 < arg1` 的 `LOGICAL_LESS_FLOAT` + `FUNCTION_RETURN` 窄形态；bool typed
  thunk forward decl/definition 现在覆盖 `TZrFloat64` 两参签名并返回
  `(TZrBool)(zr_aot_arg0 < zr_aot_arg1)`。调用侧新增
  `backend_aot_write_c_static_direct_f64_bool_two_arg_function_call()`，生成
  `zr_aot_static_f64_bool_two_arg_direct_call`，直接传 `zr_aot_f*` 参数并只在需要时
  同步 bool 栈槽。支撑拆分：新增 `backend_aot_c_typed_direct_f64_calls.{h,c}`，
  把 f64 direct-call route proof 从 `backend_aot_c_typed_direct_calls.c` 抽出，
  主调度文件降至 868 / 785 行。RED/GREEN：RED 为 typed call contracts 缺
  `backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(...)`，bool smoke 新 f64 less
  case 为 `Expected Non-NULL`；补实现后 focused GREEN 为 typed call contracts 4/0、
  bool smoke 20/0；拆分后 focused GREEN 为 typed call contracts 4/0、bool smoke
  20/0、f64 smoke 18/0。测试结果：WSL GCC debug 宽构建通过；合约组 source
  19/0、call contracts 4/0、typed call contracts 4/0、generic numeric 1/0、
  global 7/0、logical 4/0、power 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组 shared 8/0、call smoke 5/0、typed
  direct-call 5/0、i64 three-arg 6/0、bool 20/0、u64 22/0、f64 18/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-less-bool-typed-thunk.md`。
  文件规模：bool thunks 821 / 716，typed direct calls 868 / 785，f64 direct route
  source 189 / 165，f64 direct route header 53 / 50，lowering calls 892 / 846，
  emitter header 781 / 776，typed call contract 965 / 927，bool smoke 562 / 521。
  备注：只覆盖窄 f64 `<` 返回 bool 的 typed direct-call；inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、general typed-return ABI、dynamic value access
  helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 12:10:24 +08:00 · M1.5 / 07-S5 f64 three-arg shape split
  支撑切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12
  未开始 · 完成项目：新增 `backend_aot_c_typed_f64_three_arg_shapes.{h,c}`，
  将 f64 三参数 add/subtract/multiply/divide/modulo typed thunk shape 识别从
  `backend_aot_c_typed_f64_thunk_shapes.c` 拆出；`backend_aot_c_typed_f64_thunk_shapes.h`
  include 新三参 shape header，原基础 shape source 只保留常量、一参数和二参数 f64
  形态；typed call contracts 分别读取基础 f64 shape source 与三参数 f64 shape
  source，锁住 `function->parameterMetadataCount < 3u`、第三参数类型校验和
  `ADD_FLOAT`/`SUB_FLOAT`/`MUL_FLOAT`/`DIV_FLOAT`/`MOD_FLOAT` 三参识别归属。
  RED/GREEN：RED 为 typed call contracts 读取新三参 shape source 时 `Expected Non-NULL`；
  补拆分实现后 focused GREEN 为 typed call contracts 4/0、f64 shared-library smoke 18/0。
  测试结果：WSL GCC debug 目标构建通过，CMake 因新增 glob source 自动重新配置并编译新源；
  合约组通过 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts
  2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0；共享库烟测组通过
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg smoke 6/0、bool
  19/0、u64 22/0、f64 18/0、typed arithmetic 5/0、typed bitwise 6/0、value-type
  1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke
  4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-f64-three-arg-shape-split.md`。
  文件规模：f64 thunk writer 267 physical / 246 non-empty lines，f64 base shape
  source 698 / 602，f64 base shape header 21 / 18，f64 three-arg shape source
  256 / 229，f64 three-arg shape header 12 / 9，typed call contract 919 / 881，
  f64 smoke 452 / 415。备注：该切片只做行为保持的 shape ownership 拆分；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
  general multi-arg typed-return ABI、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:48:03 +08:00 · M1.5 / 07-S5 static f64 three-arg modulo
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_f64_arg0_arg1_arg2_modulo_return()`，只接受三参数
  float/double return callee 中两条有序 `MOD_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态：
  第一步必须为 `arg0 % arg1`，第二步必须为 first-result `% arg2`。
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 纳入 modulo shape；
  `backend_aot_c_typed_f64_thunks.c` 新增三参 modulo thunk writer，生成
  `ZR_UNLIKELY(zr_aot_arg1 == 0.0 || zr_aot_arg2 == 0.0)` 保护，失败时调用
  `ZrCore_Debug_RunError(state, "generated AOT float three-arg modulo by zero")`
  并 defensive 返回 `(TZrFloat64)0.0`，正常路径返回
  `return (TZrFloat64)fmod(fmod(zr_aot_arg0, zr_aot_arg1), zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_f64_three_arg_direct_call` route proof/writer 与 `nativeDouble`
  destination sync guard。RED/GREEN：RED 为 typed call contracts 缺三参 modulo
  writer 文本，新增 f64 三参 modulo smoke 失败为 `Expected Non-NULL`；补实现后
  focused GREEN 为 typed call contracts 4/0、f64 shared-library smoke 18/0。
  测试结果：WSL GCC debug 目标构建通过；合约组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 6/0、bool 19/0、u64 22/0、f64 18/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-modulo-typed-thunk.md`。
  文件规模：f64 thunk writer 267 physical / 246 non-empty lines，f64 shape source
  943 / 822，f64 shape header 25 / 22，typed call contract 907 / 869，f64 smoke
  452 / 415。大文件备注：f64 shape source 已接近 1000 行，后续再新增 f64 shape 前应
  优先拆出三参数 shape 模块。备注：该切片只覆盖窄 f64 三参 modulo typed direct-call；
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access
  helpers、general multi-arg typed-return ABI、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:38:58 +08:00 · M1.5 / 07-S5 static f64 three-arg divide
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return()`，只接受三参数
  float/double return callee 中两条有序 `DIV_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态：
  第一步必须为 `arg0 / arg1`，第二步必须为 first-result `/ arg2`。
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 纳入 divide shape；
  `backend_aot_c_typed_f64_thunks.c` 新增三参 divide thunk writer，生成
  `ZR_UNLIKELY(zr_aot_arg1 == 0.0 || zr_aot_arg2 == 0.0)` 保护，失败时调用
  `ZrCore_Debug_RunError(state, "generated AOT float three-arg divide by zero")`
  并 defensive 返回 `(TZrFloat64)0.0`，正常路径返回
  `return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_f64_three_arg_direct_call` route proof/writer 与 `nativeDouble`
  destination sync guard。RED/GREEN：RED 为 typed call contracts 缺三参 divide
  writer 文本，新增 f64 三参 divide smoke 失败为 `Expected Non-NULL`；补实现后
  focused GREEN 为 typed call contracts 4/0、f64 shared-library smoke 17/0。
  测试结果：WSL GCC debug 目标构建通过；合约组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 6/0、bool 19/0、u64 22/0、f64 17/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-divide-typed-thunk.md`。
  文件规模：f64 thunk writer 252 physical / 232 non-empty lines，f64 shape source
  894 / 778，f64 shape header 24 / 21，typed call contract 903 / 865，f64 smoke
  426 / 391。备注：该切片只覆盖窄 f64 三参 divide typed direct-call；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
  general multi-arg typed-return ABI、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:29:03 +08:00 · M1.5 / 07-S5 static f64 three-arg subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_f64_arg0_arg1_arg2_subtract_return()`，只接受三参数
  float/double return callee 中两条有序 `SUB_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态：
  第一步必须为 `arg0 - arg1`，第二步必须为 first-result `- arg2`。
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 纳入 subtract shape；
  `backend_aot_c_typed_f64_thunks.c` 复用三参 f64 thunk definition writer，发出
  `return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_f64_three_arg_direct_call` route proof/writer 与 `nativeDouble`
  destination sync guard。RED/GREEN：RED 为 typed call contracts 缺三参 subtract
  return 文本，新增 f64 三参 subtract smoke 失败为 `Expected Non-NULL`；补实现后
  focused GREEN 为 typed call contracts 4/0、f64 shared-library smoke 16/0。
  测试结果：WSL GCC debug 目标构建通过；合约组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 6/0、bool 19/0、u64 22/0、f64 16/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-subtract-typed-thunk.md`。
  文件规模：f64 thunk writer 237 physical / 218 non-empty lines，f64 shape source
  845 / 734，f64 shape header 23 / 20，typed call contract 898 / 860，f64 smoke
  400 / 367。备注：该切片只覆盖窄 f64 三参 subtract typed direct-call；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
  general multi-arg typed-return ABI、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:17:58 +08:00 · M1.5 / 07-S5 static f64 three-arg multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_f64_arg0_arg1_arg2_multiply_return()`，只接受三参数
  float/double return callee 中两条 `MUL_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态。
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 纳入 multiply shape；
  `backend_aot_c_typed_f64_thunks.c` 复用三参 f64 thunk definition writer，发出
  `return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`。调用侧复用已有
  `zr_aot_static_f64_three_arg_direct_call` route proof/writer 与 `nativeDouble`
  destination sync guard。RED/GREEN：RED 为 typed call contracts 缺三参 multiply
  return 文本，新增 f64 三参 multiply smoke 失败为 `Expected Non-NULL`；补实现后
  focused GREEN 为 typed call contracts 4/0、f64 shared-library smoke 15/0。
  测试结果：WSL GCC debug 目标构建通过；合约组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 6/0、bool 19/0、u64 22/0、f64 15/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-multiply-typed-thunk.md`。
  文件规模：f64 thunk writer 231 physical / 212 non-empty lines，f64 shape source
  796 / 690，f64 shape header 22 / 19，typed call contract 896 / 858，f64 smoke
  374 / 343。备注：该切片只覆盖窄 f64 三参 multiply typed direct-call；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、
  general multi-arg typed-return ABI、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 11:07:26 +08:00 · M1.5 / 07-S5 f64 thunk shape split
  支撑切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_f64_thunk_shapes.{h,c}`，把 f64 typed thunk
  shape 识别函数从 `backend_aot_c_typed_f64_thunks.c` 抽出；f64 thunk writer 现在只保留
  can-emit gate、forward declarations 和 thunk definition writer，并通过
  `backend_aot_c_typed_f64_thunk_shapes.h` 调用 shape recognizers。
  `tests/parser/test_aot_c_typed_call_contracts.c` 改为分别检查 f64 thunk writer 与 f64
  shape source，和现有 i64/u64 shape split 模式对齐。RED/GREEN：RED 为 typed call
  contracts 读取新 shape source 时 `Expected Non-NULL`；拆分后 focused GREEN 为 typed
  call contracts 4/0、f64 shared-library smoke 14/0。测试结果：WSL GCC debug 目标构建
  通过，CMake 因新增 glob source 自动重新配置并编译
  `backend_aot_c_typed_f64_thunk_shapes.c`；合约组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 6/0、bool 19/0、u64 22/0、f64 14/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-f64-thunk-shape-split.md`。文件规模：
  f64 thunk writer 225 physical / 206 non-empty lines，f64 shape source 748 / 646，
  f64 shape header 21 / 18，typed call contract 894 / 856，f64 smoke 348 / 319。备注：
  这是行为保持的支撑拆分；inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  dynamic value access helpers、general multi-arg typed-return ABI、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 10:54:32 +08:00 · M1.5 / 07-S5 static f64 three-arg add
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_f64_thunks.c`
  新增 `backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return()`，只接受三参数
  float/double return callee 中两条 `ADD_FLOAT` 后接 `FUNCTION_RETURN` 的窄形态，
  参数槽必须覆盖 `arg0 + arg1 + arg2`。`backend_aot_c_can_emit_typed_f64_three_arg_thunk()`
  进入 f64 can-emit gate；f64 thunk forward/definition writer 生成
  `static TZrFloat64 ... (TZrFloat64, TZrFloat64, TZrFloat64)`，正常路径返回
  `return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。
  `backend_aot_c_typed_direct_calls.c` 新增 f64 三参 route proof，证明 destination 与
  三个 call-window 参数槽都是已写入的 f64 scalar locals；
  `backend_aot_c_lowering_calls.c` / `backend_aot_c_emitter.h` 新增
  `backend_aot_write_c_static_direct_f64_three_arg_function_call()`，发出
  `zr_aot_static_f64_three_arg_direct_call` 并在需要时同步 `nativeDouble` 栈槽。
  RED/GREEN：RED 为 typed call contracts 缺
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk(const SZrFunction *function)`，
  新增 f64 三参 add smoke 失败为 `Expected Non-NULL`；补实现后 focused GREEN 为
  typed call contracts 4/0、f64 shared-library smoke 14/0。测试结果：WSL GCC debug
  全量构建通过；合约组通过 source 19/0、call contracts 4/0、typed call contracts
  4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、
  power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts
  1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、i64
  three-arg smoke 6/0、bool 19/0、u64 22/0、f64 14/0、typed arithmetic 5/0、typed
  bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、global
  smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-three-arg-add-typed-thunk.md`。
  文件规模：emitter header 774 physical / 769 non-empty lines，f64 thunk source
  968 / 849，lowering calls 853 / 809，typed direct-call route 973 / 876，typed
  call contract 886 / 848，f64 smoke 348 / 319。大文件备注：f64 thunk source 与
  typed direct-call route 仍低于 1000 行且职责仍分别集中；本切片不强拆，后续若继续添加
  f64 多参数/更多 direct-call route，最小拆分边界应分别抽出 f64 thunk shape helpers 与按
  标量类型拆分的 direct-call route 模块。备注：该切片只覆盖窄
  f64 三参 add typed direct-call；inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、dynamic value access helpers、general multi-arg typed-return ABI、07-S5
  完整验收和 08-12 仍未完成。验证备注：CTest 在该构建目录未注册这些测试名，最终使用
  现有测试二进制逐个执行并通过。

- 2026-06-23 10:30:14 +08:00 · M1.5 / 07-S5 static u64 two-arg modulo
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_modulo_return()`，只接受两参数
  unsigned return callee 中 `MOD_UNSIGNED` 后接 `FUNCTION_RETURN` 且 operand 顺序为
  `arg0 % arg1` 的窄形态。`backend_aot_c_can_emit_typed_u64_two_arg_thunk()`
  纳入 modulo；`backend_aot_write_c_typed_u64_thunks()` 为 matching callee 发出带
  `ZR_UNLIKELY(zr_aot_arg1 == 0u)` 的运行期取模除零保护，除零时调用
  `ZrCore_Debug_RunError(state, "generated AOT unsigned modulo by zero")` 并返回
  defensive `(TZrUInt64)0`，正常路径返回
  `return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1);`。调用侧复用已有
  `zr_aot_static_u64_two_arg_direct_call` proof/writer 和 scalar-only destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `generated AOT unsigned modulo by zero`，新增 modulo smoke 失败为
  `Expected Non-NULL`；补 shape 与 guarded writer 后第一次 focused GREEN 暴露
  contract needle 需匹配 generator 源文件里的 `%%` 转义，调整契约后 focused GREEN 为
  typed call contracts 4/0、u64 shared-library smoke 22/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts
  4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg smoke 6/0、bool 19/0、u64 22/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-modulo-typed-thunk.md`。
  文件规模：u64 thunk 主文件 852 physical / 767 non-empty lines，u64 shape source
  689 / 617，shape header 21 / 18，typed call contract 866 / 828，u64 smoke 553 / 508。
  备注：u64 两参 divide/modulo 窄形态与生成除零保护已覆盖；更多
  runtime-failure-capable shapes、inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 10:20:11 +08:00 · M1.5 / 07-S5 static u64 two-arg divide
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_divide_return()`，只接受两参数
  unsigned return callee 中 `DIV_UNSIGNED` 后接 `FUNCTION_RETURN` 且 operand 顺序为
  `arg0 / arg1` 的窄形态。`backend_aot_c_can_emit_typed_u64_two_arg_thunk()`
  纳入 divide；`backend_aot_write_c_typed_u64_thunks()` 为 matching callee 发出带
  `ZR_UNLIKELY(zr_aot_arg1 == 0u)` 的运行期除零保护，除零时调用
  `ZrCore_Debug_RunError(state, "generated AOT unsigned divide by zero")` 并返回
  defensive `(TZrUInt64)0`，正常路径返回
  `return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1);`。调用侧复用已有
  `zr_aot_static_u64_two_arg_direct_call` proof/writer 和 scalar-only destination
  sync elision。RED/GREEN：初版 RED contract 曾过度要求 u64 thunk writer 源文件直接包含
  `ZR_INSTRUCTION_ENUM(DIV_UNSIGNED)`，随后按职责收窄到 shape-source needle；最终 RED 为
  typed call contracts 缺 `generated AOT unsigned divide by zero`，新增 divide smoke
  失败为 `Expected Non-NULL`；补 shape 与 guarded writer 后 focused GREEN 为
  typed call contracts 4/0、u64 shared-library smoke 21/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts
  4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg smoke 6/0、bool 19/0、u64 21/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-divide-typed-thunk.md`。
  文件规模：u64 thunk 主文件 837 physical / 753 non-empty lines，u64 shape source
  653 / 585，shape header 20 / 17，typed call contract 862 / 824，u64 smoke 528 / 485。
  备注：u64 两参 divide 窄形态与生成除零保护已覆盖；u64/f64 modulo、更多
  runtime-failure-capable shapes、inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 10:04:13 +08:00 · M1.5 / 07-S5 static u64 three-arg bitwise-xor
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return()` 与
  bitwise-xor operand reader，复用三参 u64 binary-return helper 校验两条
  `BITWISE_XOR` 指令和最终 `FUNCTION_RETURN`。`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  现在覆盖 add/multiply/subtract/bitwise-and/bitwise-or/bitwise-xor，`backend_aot_write_c_typed_u64_thunks()`
  对 matching callee 发出 `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`。
  调用侧复用已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer 和
  scalar-only destination sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`，新增 bitwise-xor
  smoke 失败为 `Expected Non-NULL`；补 shape 与 writer 后 focused GREEN 为
  typed call contracts 4/0、u64 shared-library smoke 20/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts
  4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg smoke 6/0、bool 19/0、u64 20/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-bitwise-xor-typed-thunk.md`。
  文件规模：u64 thunk 主文件 822 physical / 739 non-empty lines，u64 shape source
  617 / 553，shape header 19 / 16，typed call contract 857 / 819，u64 smoke 503 / 462。
  备注：三参 u64 add/multiply/subtract/bitwise-and/bitwise-or/bitwise-xor 窄形态已覆盖；
  runtime-failure-capable division/modulo、inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、dynamic value access helpers、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 09:55:55 +08:00 · M1.5 / 07-S5 static u64 three-arg bitwise-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return()` 与
  bitwise-or operand reader，复用三参 u64 binary-return helper 校验两条
  `BITWISE_OR` 指令和最终 `FUNCTION_RETURN`。`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  现在覆盖 add/multiply/subtract/bitwise-and/bitwise-or，`backend_aot_write_c_typed_u64_thunks()`
  对 matching callee 发出 `return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);`。
  调用侧复用已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer 和
  scalar-only destination sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);`，新增 bitwise-or
  smoke 失败为 `Expected Non-NULL`；补 shape 与 writer 后 focused GREEN 为
  typed call contracts 4/0、u64 shared-library smoke 19/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts
  4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg smoke 6/0、bool 19/0、u64 19/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-bitwise-or-typed-thunk.md`。
  文件规模：u64 thunk 主文件 816 physical / 733 non-empty lines，u64 shape source
  595 / 534，shape header 18 / 15，typed call contract 854 / 816，u64 smoke 477 / 438。
  备注：三参 u64 add/multiply/subtract/bitwise-and/bitwise-or 窄形态已覆盖；u64 三参
  XOR、runtime-failure-capable division/modulo、inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、dynamic value access helpers、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 09:46:43 +08:00 · M1.5 / 07-S5 static u64 three-arg bitwise-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return()` 与
  bitwise-and operand reader，复用三参 u64 binary-return helper 校验两条
  `BITWISE_AND` 指令和最终 `FUNCTION_RETURN`。`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  现在覆盖 add/multiply/subtract/bitwise-and，`backend_aot_write_c_typed_u64_thunks()`
  对 matching callee 发出 `return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);`。
  调用侧复用已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer 和
  scalar-only destination sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);`，新增 bitwise-and
  smoke 失败为 `Expected Non-NULL`；补 shape 与 writer 后 focused GREEN 为
  typed call contracts 4/0、u64 shared-library smoke 18/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts
  4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组第一次整串运行在 124s 超时且无失败输出，拆分为两段后通过
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg smoke 6/0、bool
  19/0、u64 18/0、f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-bitwise-and-typed-thunk.md`。
  文件规模：u64 thunk 主文件 810 physical / 727 non-empty lines，u64 shape source
  573 / 515，shape header 17 / 14，typed call contract 851 / 813，u64 smoke 451 / 414。
  备注：三参 u64 add/multiply/subtract/bitwise-and 窄形态已覆盖；u64 三参 OR/XOR、
  runtime-failure-capable division/modulo、inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、dynamic value access helpers、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 09:33:50 +08:00 · M1.5 / 07-S5 static u64 three-arg subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return()` 与
  subtract operand reader。三参 u64 binary-return helper 增加 `preserveOperandOrder`，
  subtract 必须证明 `(arg0 - arg1) - arg2`，add/multiply 继续使用 commutative
  operand set。`backend_aot_c_can_emit_typed_u64_three_arg_thunk()` 现在覆盖
  add/multiply/subtract，`backend_aot_write_c_typed_u64_thunks()` 对 matching callee
  发出 `return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer 和 scalar-only destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`，新增 subtract
  smoke 失败为 `Expected Non-NULL`；补 ordered shape 与 writer 后 focused GREEN 为
  typed call contracts 4/0、u64 shared-library smoke 17/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts
  4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float
  contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call
  5/0、i64 three-arg smoke 6/0、bool 19/0、u64 17/0、f64 13/0、typed arithmetic
  5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke
  1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-subtract-typed-thunk.md`。
  文件规模：u64 thunk 主文件 804 physical / 721 non-empty lines，u64 shape source
  551 / 496，shape header 16 / 13，typed call contract 849 / 811，u64 smoke 425 / 390。
  备注：三参 u64 add/multiply/subtract 窄形态已覆盖；u64 三参 bitwise、
  runtime-failure-capable division/modulo、inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、dynamic value access helpers、07-S5 完整验收和 08-12
  仍未完成。

- 2026-06-23 09:24:25 +08:00 · M1.5 / 07-S5 static u64 three-arg multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return()` 与
  multiply operand reader，复用三参 u64 binary-return helper 校验 `(arg0 * arg1) * arg2`
  两条 multiply 指令和最终 `FUNCTION_RETURN`。`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  现在覆盖 add/multiply，`backend_aot_write_c_typed_u64_thunks()` 对 matching callee
  发出 `return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_u64_three_arg_direct_call` proof/writer 和 scalar-only destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`，新增 multiply
  smoke 失败为 `Expected Non-NULL`；补 shape 与 writer 后 focused GREEN 为 typed
  call contracts 4/0、u64 shared-library smoke 16/0。测试结果：实际存在的较宽 WSL GCC
  AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call contracts
  4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、
  power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts
  1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、typed direct-call 5/0、
  i64 three-arg smoke 6/0、bool 19/0、u64 16/0、f64 13/0、typed arithmetic 5/0、
  typed bitwise 6/0、value-type 1/0、float smoke 1/0、generic numeric smoke 1/0、
  global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-multiply-typed-thunk.md`。
  文件规模：u64 thunk 主文件 798 physical / 715 non-empty lines，u64 shape source
  514 / 464，shape header 15 / 12，typed call contract 847 / 809，u64 smoke 399 / 366。
  备注：这是 broader typed parameter ABI 的三参 u64 multiply 窄覆盖；u64 三参
  subtract/bitwise、runtime-failure-capable division/modulo、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、dynamic value access helpers、07-S5
  完整验收和 08-12 仍未完成。

- 2026-06-23 09:13:42 +08:00 · M1.5 / 07-S5 static u64 three-arg add
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return()`、三参 u64
  binary-return helper 和 add operand reader，覆盖三参数 unsigned callee 的两条
  `ADD_UNSIGNED` / `ADD_UNSIGNED_PLAIN_DEST` 或兼容 signed-add 形态后接
  `FUNCTION_RETURN`。`backend_aot_c_can_emit_typed_u64_three_arg_thunk()`
  接入 u64 三参 can-emit gate，`backend_aot_write_c_typed_u64_thunks()`
  对 matching callee 发出
  `return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。
  `backend_aot_can_write_c_static_direct_u64_three_arg_call()` 证明 destination
  与三个 call-window arguments 都是已写入的 u64 scalar locals，lowering writer
  发出 `zr_aot_static_u64_three_arg_direct_call`，并在 scalar-only destination
  下继续省略 typed-destination stack sync。RED/GREEN：RED 为 typed call contracts
  缺 `backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function)`，
  新增 u64 three-arg add smoke 在 generated C 中找不到三参 thunk/direct-call 文本，
  失败为 `Expected Non-NULL`；补 shape、can-emit、writer 和 route 后 focused GREEN
  为 typed call contracts 4/0、u64 shared-library smoke 15/0。测试结果：实际存在的
  较宽 WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke
  5/0、typed direct-call 5/0、i64 three-arg smoke 6/0、bool 19/0、u64 15/0、
  f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke
  1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-three-arg-add-typed-thunk.md`。
  文件规模：u64 thunk 主文件 792 physical / 709 non-empty lines，u64 shape source
  485 / 439，shape header 14 / 11，typed direct-call router 909 / 817，call
  lowering 812 / 770，emitter header 766 / 761，typed call contract 845 / 807，
  u64 smoke 373 / 342。备注：这是 broader typed parameter ABI 的首个三参 u64
  add 窄覆盖；u64 三参 subtract/multiply/bitwise、runtime-failure-capable
  division/modulo、inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:52:55 +08:00 · M1.5 / 07-S5 static i64 three-arg subtract
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return()` 与
  `backend_aot_c_try_read_i64_subtract_operands()`，覆盖三参数 i64 callee 的
  `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` + `FUNCTION_RETURN` shape。
  私有 `backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return()` 增加
  `preserveOperandOrder`，subtract 形态必须证明 `(arg0 - arg1) - arg2`，不走
  add/multiply/bitwise 的 commutative operand set。`backend_aot_c_can_emit_typed_i64_three_arg_thunk()`
  现在覆盖 add/subtract/multiply/bitwise-and/bitwise-or/bitwise-xor，
  thunk writer 对 matching callee 发出
  `return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);`。调用侧继续复用
  `zr_aot_static_i64_three_arg_direct_call` proof/writer 和 scalar-only destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(`，新增 subtract smoke
  失败为 `Expected Non-NULL`；初次实现还因过度指定不存在的
  `SUB_SIGNED_LOAD_STACK` opcode 编译失败，随后收紧为当前 enum 表面
  `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` 后 focused GREEN 为 typed call contracts 4/0、
  i64 three-arg smoke 6/0。测试结果：实际存在的较宽 WSL GCC AOT 目标构建通过；
  合约组通过 source 19/0、call contracts 4/0、typed call contracts 4/0、generic
  numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、power contracts
  2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts 1/0；共享库烟测组通过
  shared 8/0、call smoke 5/0、typed direct-call 5/0、i64 three-arg smoke 6/0、bool
  19/0、u64 14/0、f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、
  float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-subtract-typed-thunk.md`。
  文件规模：i64 thunk 主文件 843 physical / 755 non-empty lines，i64 shape source
  221 / 190，shape header 13 / 10，typed call contract 828 / 790，i64 three-arg
  smoke 197 / 183，i64 smoke support 197 / 178。备注：这是 broader typed parameter
  ABI 的三参 i64 subtract 窄覆盖；至此三参 i64 add/subtract/multiply/AND/OR/XOR
  窄形态已覆盖，但 inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  general multi-arg returns、dynamic value access helpers、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 08:40:52 +08:00 · M1.5 / 07-S5 static i64 three-arg bitwise-xor
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return()` 与
  `backend_aot_c_try_read_i64_bitwise_xor_operands()`，复用三参 binary-return
  helper 校验三参数 i64 metadata、return type、两条 `BITWISE_XOR` 指令和最终
  `FUNCTION_RETURN` result。`backend_aot_c_can_emit_typed_i64_three_arg_thunk()`
  现在覆盖 add/multiply/bitwise-and/bitwise-or/bitwise-xor，
  `backend_aot_write_c_typed_i64_thunks()` 对 matching callee 发出
  `return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);`。调用侧继续复用
  `zr_aot_static_i64_three_arg_direct_call` proof/writer，保持三参 native C 形参直传
  与 scalar-only destination sync elision。RED/GREEN：RED focused 为 typed call
  contracts 缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(`，
  新增 bitwise-xor smoke 也因缺少三参 bitwise-xor thunk/direct-call 文本失败为
  `Expected Non-NULL`；补 shape、can-emit gate 与 writer branch 后 focused GREEN
  为 typed call contracts 4/0、i64 three-arg smoke 5/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical
  contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 5/0、bool 19/0、u64 14/0、f64 13/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-bitwise-xor-typed-thunk.md`。
  文件规模：i64 thunk 主文件 838 physical / 750 non-empty lines，i64 shape source
  183 / 156，shape header 12 / 9，typed call contract 822 / 784，i64 three-arg
  smoke 166 / 154，i64 smoke support 197 / 178。备注：这是 broader typed parameter
  ABI 的三参 i64 bitwise-xor 窄覆盖；至此三参 i64 add/multiply/AND/OR/XOR
  窄形态已覆盖，但 inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  general multi-arg returns、dynamic value access helpers、07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 08:32:29 +08:00 · M1.5 / 07-S5 static i64 three-arg bitwise-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return()` 与
  `backend_aot_c_try_read_i64_bitwise_or_operands()`，复用三参 binary-return helper
  校验三参数 i64 metadata、return type、两条 `BITWISE_OR` 指令和最终
  `FUNCTION_RETURN` result。`backend_aot_c_can_emit_typed_i64_three_arg_thunk()`
  现在覆盖 add/multiply/bitwise-and/bitwise-or，
  `backend_aot_write_c_typed_i64_thunks()` 对 matching callee 发出
  `return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);`。调用侧继续复用
  `zr_aot_static_i64_three_arg_direct_call` proof/writer，保持三参 native C 形参直传
  与 scalar-only destination sync elision。RED/GREEN：RED focused 为 typed call
  contracts 缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(`，
  新增 bitwise-or smoke 也因缺少三参 bitwise-or thunk/direct-call 文本失败为
  `Expected Non-NULL`；补 shape、can-emit gate 与 writer branch 后 focused GREEN
  为 typed call contracts 4/0、i64 three-arg smoke 4/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical
  contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 4/0、bool 19/0、u64 14/0、f64 13/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-bitwise-or-typed-thunk.md`。
  文件规模：i64 thunk 主文件 833 physical / 745 non-empty lines，i64 shape source
  161 / 138，shape header 11 / 8，typed call contract 817 / 779，i64 three-arg
  smoke 135 / 125，i64 smoke support 197 / 178。备注：这是 broader typed parameter
  ABI 的三参 i64 bitwise-or 窄覆盖；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:24:03 +08:00 · M1.5 / 07-S5 static i64 three-arg bitwise-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`backend_aot_c_typed_i64_thunk_shapes.{h,c}`
  新增 `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return()` 与
  `backend_aot_c_try_read_i64_bitwise_and_operands()`，复用三参 binary-return
  helper 校验三参数 i64 metadata、return type、两条 `BITWISE_AND` 指令和最终
  `FUNCTION_RETURN` result。`backend_aot_c_can_emit_typed_i64_three_arg_thunk()`
  现在覆盖 add/multiply/bitwise-and，`backend_aot_write_c_typed_i64_thunks()`
  对 matching callee 发出
  `return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);`。调用侧继续复用
  `zr_aot_static_i64_three_arg_direct_call` proof/writer，保持三参 native C 形参直传
  与 scalar-only destination sync elision。RED/GREEN：RED focused 为 typed call
  contracts 缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(`，
  新增 bitwise-and smoke 也因缺少三参 bitwise-and thunk/direct-call 文本失败为
  `Expected Non-NULL`；补 shape、can-emit gate 与 writer branch 后 focused GREEN
  为 typed call contracts 4/0、i64 three-arg smoke 3/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、typed call
  contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、logical
  contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR
  4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke 5/0、
  typed direct-call 5/0、i64 three-arg smoke 3/0、bool 19/0、u64 14/0、f64 13/0、
  typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke
  1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-bitwise-and-typed-thunk.md`。
  文件规模：i64 thunk 主文件 828 physical / 740 non-empty lines，i64 shape source
  139 / 120，shape header 10 / 7，typed call contract 812 / 774，i64 three-arg
  smoke 104 / 96，i64 smoke support 197 / 178。备注：这是 broader typed parameter
  ABI 的三参 i64 bitwise-and 窄覆盖；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:13:44 +08:00 · M1.5 / 07-S5 static i64 three-arg multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：在上一轮 i64 三参 add 和 shape split
  基础上，`backend_aot_c_typed_i64_thunk_shapes.{h,c}` 新增
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return()` 与
  `backend_aot_c_try_read_i64_multiply_operands()`，识别三参数 int callee 中两条
  signed multiply 后 `FUNCTION_RETURN` 的 `arg0 * arg1 * arg2` 形态。
  shape 模块现在通过私有
  `backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return()` 共享三参
  add/multiply 的参数元数据、返回类型、两段二元指令和 return-result 校验。
  `backend_aot_c_can_emit_typed_i64_three_arg_thunk()` 纳入 multiply shape，
  `backend_aot_write_c_typed_i64_thunks()` 对匹配 callee 发出
  `return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);`。调用侧复用
  已有 `zr_aot_static_i64_three_arg_direct_call` route proof/writer，继续证明
  destination 与三个 call-window i64 argument locals 已写入后发出直接 C 调用并省略
  不必要的 destination stack sync。RED/GREEN：RED focused 为 typed call contracts
  缺 `backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(`，新增 i64
  three-arg multiply smoke 也因缺少三参乘法 thunk/direct-call 文本失败为
  `Expected Non-NULL`；补 shape、can-emit gate 与 writer branch 后 focused GREEN 为
  typed call contracts 4/0、i64 three-arg smoke 2/0。测试结果：实际存在的较宽
  WSL GCC AOT 目标构建通过；合约组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0；共享库烟测组通过 shared 8/0、call smoke
  5/0、typed direct-call 5/0、i64 three-arg smoke 2/0、bool 19/0、u64 14/0、
  f64 13/0、typed arithmetic 5/0、typed bitwise 6/0、value-type 1/0、float smoke
  1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power
  smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-multiply-typed-thunk.md`。
  文件规模：i64 thunk 主文件 823 physical / 735 non-empty lines，i64 shape source
  117 / 102，shape header 9 / 6，typed call contract 807 / 769，i64 three-arg
  smoke 73 / 67，i64 smoke support 197 / 178。备注：这是 broader typed parameter
  ABI 的三参 i64 乘法窄覆盖；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、general multi-arg returns、dynamic value access helpers、
  07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 08:00:16 +08:00 · M1.5 / 07-S5 i64 three-arg thunk shape split
  支持切片 · 状态：支持切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：为继续扩展 broader typed parameter ABI，先把三参 i64 thunk shape 识别从
  `backend_aot_c_typed_i64_thunks.c` 拆到 `backend_aot_c_typed_i64_thunk_shapes.{h,c}`。
  新模块目前承载 `backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return()` 及其 add
  operand reader/type-ref predicate；主 i64 thunk 文件继续负责 no/one/two/three-arg
  can-emit gate、forward declaration 和 thunk definition writer。契约测试同步拆分文件归属，
  `tests/parser/test_aot_c_typed_call_contracts.c` 现在同时读取 i64 thunk 主文件与 i64
  shape 文件，避免三参参数元数据/ADD shape 校验随模块拆分丢失。RED/GREEN：这是同行为
  拆分，未新增行为 RED；首次 focused split 后契约测试暴露旧测试仍只读主文件，缺少
  `function->parameterMetadataCount < 3u`，调整契约归属后 focused GREEN 为 typed call
  contracts 4/0、i64 three-arg smoke 1/0。测试结果：较宽 WSL GCC focused AOT 组通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts
  1/0、global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup
  1/0、return 1/0、value SemIR 4/0、float contracts 1/0、shared 8/0、call smoke 5/0、
  typed direct-call i64 smoke 5/0、i64 three-arg smoke 1/0、bool smoke 19/0、u64 smoke
  14/0、f64 smoke 13/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type
  1/0、float smoke 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke
  4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-i64-three-arg-shape-split.md`。文件规模：
  i64 thunk 主文件降至 818 physical / 730 non-empty lines，新 i64 shape source 为
  76 / 67，新 header 为 8 / 5，typed call contract 为 800 / 762。备注：这是后续三参
  i64 multiply/更多三参表达式切片的支撑拆分，不新增 runtime 行为；inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、dynamic
  value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 07:47:14 +08:00 · M1.5 / 07-S5 static i64 three-arg add
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`i64(i64, i64, i64)` typed direct-call route
  首次把 i64 native 参数 ABI 从 no/one/two-arg 扩展到三参窄形态。`backend_aot_c_typed_i64_thunks.c`
  新增 `backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return()`，识别三参数 int
  callee 中连续两条 signed add 后 `FUNCTION_RETURN` 的 `arg0 + arg1 + arg2`
  形态，并发出
  `static TZrInt64 zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2)`
  native thunk，直接返回
  `return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);`。
  `backend_aot_c_typed_direct_calls.c` 新增三参 route proof，要求 destination local
  与 `functionSlot + 1/2/3` 三个 argument locals 均为已写入 i64 scalar locals；
  `backend_aot_c_lowering_calls.c` 新增 `zr_aot_static_i64_three_arg_direct_call`
  writer，发出
  `zr_aot_sD = zr_aot_typed_i64_fn_N(state, zr_aot_sA, zr_aot_sB, zr_aot_sC)`
  并沿用 scalar-only destination sync elision。RED/GREEN：RED 为 typed call contracts
  缺少 `backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function)`，
  新增 i64 three-arg shared-library smoke 也因缺少三参 thunk/direct-call 形态失败为
  `Expected Non-NULL`；补 recognizer、can-emit、writer、route proof 与新测试目标后
  focused GREEN 为 typed call contracts 4/0、i64 three-arg smoke 1/0。测试结果：较宽
  WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、typed call contracts
  4/0、generic numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、
  power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、float contracts
  1/0、shared 8/0、call smoke 5/0、typed direct-call i64 smoke 5/0、i64 three-arg
  smoke 1/0、bool smoke 19/0、u64 smoke 14/0、f64 smoke 13/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、float smoke 1/0、generic numeric
  smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-three-arg-add-typed-thunk.md`。
  文件规模：i64 thunk 主文件 883 physical / 788 non-empty lines，typed direct-call
  route 845 / 758，call lowering 771 / 731，emitter header 758 / 753，typed call
  contract 787 / 749，新增 i64 smoke support 197 / 178，新增 i64 three-arg smoke
  42 / 38。备注：这是 broader typed parameter ABI 的三参 i64 加法窄覆盖；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、general multi-arg returns、
  dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 07:27:42 +08:00 · M1.5 / 07-S5 static f64 two-arg modulo
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`f64(f64, f64)` typed direct-call route
  从 add/subtract/multiply/divide 扩展到有运行期失败通道的 ordered modulo。`backend_aot_c_typed_f64_thunks.c`
  新增 `MOD_FLOAT/FUNCTION_RETURN` recognizer，`backend_aot_c_can_emit_typed_f64_two_arg_thunk()`
  现在覆盖 add/subtract/multiply/divide/modulo；modulo thunk 维持
  `static TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1)`
  native ABI，先检查 `zr_aot_arg1 == 0.0`，通过
  `ZrCore_Debug_RunError(state, "generated AOT float modulo by zero")`
  进入运行期错误通道，并保留防御性 `return (TZrFloat64)0.0;`，正常路径直接发出
  `return (TZrFloat64)fmod(zr_aot_arg0, zr_aot_arg1);`。调用侧复用
  `zr_aot_static_f64_two_arg_direct_call` proof/writer，继续证明 f64 destination local
  与两个已写入 f64 argument locals 后发
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_f64_arg0_arg1_modulo_return(`；新增 f64 shared-library smoke
  `remainder(93.0, 51.0)` 覆盖运行路径；补 modulo recognizer、can-emit gate 与带零除
  guard 的 thunk writer 后 focused GREEN 为 typed call contracts 4/0、f64 typed direct-call
  smoke 13/0。测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts
  4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、float contracts 1/0、shared 8/0、call smoke 5/0、typed direct-call
  5/0、bool smoke 19/0、u64 smoke 14/0、f64 smoke 13/0、arithmetic 5/0、bitwise
  6/0、typed scalar 1/0、value-type 1/0、float smoke 1/0、generic numeric smoke
  1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-modulo-typed-thunk.md`。
  文件规模：f64 thunk 主文件 894 physical / 782 non-empty lines，typed call contract
  766 / 728，f64 smoke 322 / 295，f64 smoke support 211 / 191。备注：这是 f64 二参
  modulo-return 窄覆盖并补齐 typed f64 dynamic denominator modulo 的运行期零除失败通道；
  inline structs、`in`/`out` writeback、deopt/dynamic bridges、broader ABI、
  dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 07:08:37 +08:00 · M1.5 / 07-S5 static f64 two-arg divide
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：`f64(f64, f64)` typed direct-call route
  从 add/subtract/multiply 扩展到有运行期失败通道的 ordered divide。`backend_aot_c_typed_f64_thunks.c`
  新增 `DIV_FLOAT/FUNCTION_RETURN` recognizer，`backend_aot_c_can_emit_typed_f64_two_arg_thunk()`
  现在覆盖 add/subtract/multiply/divide；divide thunk 维持
  `static TZrFloat64 zr_aot_typed_f64_fn_N(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1)`
  native ABI，先检查 `zr_aot_arg1 == 0.0`，通过
  `ZrCore_Debug_RunError(state, "generated AOT float divide by zero")`
  进入运行期错误通道，并保留防御性 `return (TZrFloat64)0.0;`，正常路径直接发出
  `return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1);`。调用侧复用
  `zr_aot_static_f64_two_arg_direct_call` proof/writer，继续证明 f64 destination local
  与两个已写入 f64 argument locals 后发
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_f64_arg0_arg1_divide_return(`；新增 f64 shared-library smoke
  `ratio(84.0, 2.0)` 覆盖运行路径；补 divide recognizer、can-emit gate 与带零除 guard
  的 thunk writer 后 focused GREEN 为 typed call contracts 4/0、f64 typed direct-call smoke
  12/0。测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 19/0、u64 smoke 14/0、f64 smoke 12/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-f64-two-arg-divide-typed-thunk.md`。
  文件规模：f64 thunk 主文件 843 physical / 736 non-empty lines，typed call contract
  763 / 725，f64 smoke 297 / 272，f64 smoke support 211 / 191。备注：这是 f64 二参
  divide-return 窄覆盖并首次为 typed f64 dynamic denominator route 写入运行期零除失败通道；
  f64 modulo runtime-failure route、inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、broader ABI、dynamic value access helpers、07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 06:53:10 +08:00 · M1.5 / 07-S5 static u64 two-arg greater-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route
  从 unsigned less-than-or-equal 扩展到 unsigned greater-than-or-equal。`backend_aot_c_typed_bool_thunks.c`
  复用 shared u64 bool-return compare helper，新增 `LOGICAL_GREATER_EQUAL_UNSIGNED/FUNCTION_RETURN`
  wrapper；u64 greater-equal thunk 发出
  `return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);`，且 u64-bool can-emit gate
  现在覆盖 `<`、`==`、`!=`、`>`、`<=`、`>=`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` proof/writer，继续证明 bool destination local
  与两个已写入 u64 argument locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_greater_equal_return(`；新增 bool shared-library
  smoke `at_least(50u, 8u)` 覆盖运行路径；补 greater-equal wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 19/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 19/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-greater-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return greater-than-or-equal 窄覆盖并完成当前 unsigned comparison
  subset；07-S5 完整验收和 08-12 仍未完成。

- 2026-06-23 06:44:19 +08:00 · M1.5 / 07-S5 static u64 two-arg less-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route
  从 unsigned greater-than 扩展到 unsigned less-than-or-equal。`backend_aot_c_typed_bool_thunks.c`
  复用 shared u64 bool-return compare helper，新增 `LOGICAL_LESS_EQUAL_UNSIGNED/FUNCTION_RETURN`
  wrapper；u64 less-equal thunk 发出
  `return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);`，且 u64-bool can-emit gate
  现在接受 `<`、`==`、`!=`、`>`、`<=`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` proof/writer，继续证明 bool destination local
  与两个已写入 u64 argument locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_less_equal_return(`；新增 bool shared-library
  smoke `at_most(8u, 50u)` 覆盖运行路径；补 less-equal wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 18/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 18/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-less-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return less-than-or-equal 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 06:34:55 +08:00 · M1.5 / 07-S5 static u64 two-arg greater bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route
  从 unsigned inequality 扩展到 unsigned greater-than。`backend_aot_c_typed_bool_thunks.c`
  复用 shared u64 bool-return compare helper，新增 `LOGICAL_GREATER_UNSIGNED/FUNCTION_RETURN`
  wrapper；u64 greater thunk 发出
  `return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);`，且 u64-bool can-emit gate
  现在接受 `<`、`==`、`!=`、`>`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` proof/writer，继续证明 bool destination local
  与两个已写入 u64 argument locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(`；新增 bool shared-library
  smoke `greater(50u, 8u)` 覆盖运行路径；补 greater wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 17/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 17/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-greater-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return greater-than 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 06:24:43 +08:00 · M1.5 / 07-S5 static u64 two-arg not-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route
  从 unsigned equality 扩展到 unsigned inequality。`backend_aot_c_typed_bool_thunks.c`
  复用 shared u64 bool-return compare helper，新增 `LOGICAL_NOT_EQUAL_UNSIGNED/FUNCTION_RETURN`
  wrapper；u64 inequality thunk 发出
  `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`，且 u64-bool can-emit gate
  现在接受 `<`、`==`、`!=`。调用侧复用
  `zr_aot_static_u64_bool_two_arg_direct_call` proof/writer，继续证明 bool destination local
  与两个已写入 u64 argument locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_not_equal_return(`；新增 bool shared-library
  smoke `different(21u, 22u)` 覆盖运行路径；补 not-equal wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 16/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 16/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-not-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return inequality 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 06:10:01 +08:00 · M1.5 / 07-S5 static u64 two-arg equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route
  从 unsigned less-than 扩展到 unsigned equality。`backend_aot_c_typed_bool_thunks.c`
  复用 shared u64 bool-return compare helper，新增 `LOGICAL_EQUAL_UNSIGNED/FUNCTION_RETURN`
  wrapper；u64 equality thunk 发出
  `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`。调用侧复用刚建立的
  `zr_aot_static_u64_bool_two_arg_direct_call` proof/writer，继续证明 bool destination local
  与两个已写入 u64 argument locals 后发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_u64_arg0_arg1_equal_return(`；新增 bool shared-library
  smoke `same(21u, 21u)` 覆盖运行路径；补 equality wrapper 与 thunk writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 15/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 15/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-equal-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return equality 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 05:56:13 +08:00 · M1.5 / 07-S5 static u64 two-arg less bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(u64, u64)` typed direct-call route
  覆盖第一条 unsigned comparison：`uint < uint -> bool`。`backend_aot_c_typed_bool_thunks.c`
  新增 u64 type-ref 判定、shared u64 bool-return compare helper、`LOGICAL_LESS_UNSIGNED`
  wrapper 与 `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk()`；生成 thunk 签名为
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1)`，
  并直接发 `return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);`。调用侧新增
  `zr_aot_static_u64_bool_two_arg_direct_call` proof/writer，证明 bool destination local
  与两个已写入 u64 argument locals 后，发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` 并允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function)`；
  新增 bool shared-library smoke `smaller(7u, 9u)` 覆盖运行路径，并要求无
  `CallStaticDirect` / `CallStackValue` / typed-destination sync。补 recognizer、thunk writer
  与 u64-bool route/writer 后 focused GREEN 为 typed call contracts 4/0、bool typed direct-call
  smoke 14/0。测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 14/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-u64-two-arg-less-bool-typed-thunk.md`。
  备注：这是 mixed u64-parameter bool-return less-than 窄覆盖；07-S5 完整验收和
  08-12 仍未完成。

- 2026-06-23 05:34:37 +08:00 · M1.5 / 07-S5 static i64 two-arg greater-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route
  扩展到 signed greater-than-or-equal，并完成当前窄范围 signed compare set
  `<`、`==`、`!=`、`>`、`<=`、`>=`。`backend_aot_c_typed_bool_thunks.c`
  复用 shared i64 bool-return compare helper，新增
  `LOGICAL_GREATER_EQUAL_SIGNED/FUNCTION_RETURN` wrapper；greater-equal thunk 发出
  `return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);`。调用侧复用既有
  `zr_aot_static_i64_bool_two_arg_direct_call` route proof 与 writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_i64_arg0_arg1_greater_equal_return(`；新增 bool shared-library
  smoke `at_least(50, 8)` 覆盖运行路径。补 greater-equal wrapper 与 writer branch 后
  focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 13/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 13/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-greater-equal-bool-typed-thunk.md`。
  文件规模：bool thunk 主文件 597 physical / 520 non-empty lines，typed call contract
  725 physical / 687 non-empty lines，bool smoke 366 physical / 339 non-empty lines。
  备注：当前 signed compare narrow set 已覆盖，但本切片仍不关闭 07-S5；inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、broader ABI、dynamic value access helpers、
  runtime-failure-capable division/modulo policy 与 08-12 仍待后续。

- 2026-06-23 05:24:23 +08:00 · M1.5 / 07-S5 static i64 two-arg less-equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route
  从 signed less-than/equality/inequality/greater-than 扩展到 signed less-than-or-equal。
  `backend_aot_c_typed_bool_thunks.c` 复用 shared i64 bool-return compare helper，
  新增 `LOGICAL_LESS_EQUAL_SIGNED/FUNCTION_RETURN` wrapper，并在 mixed can-emit gate 中
  同时接受 `<`、`==`、`!=`、`>`、`<=`。less-equal thunk 发出
  `return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);`。调用侧复用既有
  `zr_aot_static_i64_bool_two_arg_direct_call` route proof 与 writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，允许 destination
  sync elision。RED/GREEN：RED 为 typed call contracts 缺少
  `backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(`；新增 bool shared-library
  smoke `at_most(8, 50)` 证明旧生成物还需要 `TZrInt64` 参数 bool thunk/direct typed call。
  补 less-equal wrapper 与 writer branch 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 12/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 12/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-less-equal-bool-typed-thunk.md`。
  文件规模：bool thunk 主文件 580 physical / 505 non-empty lines，typed call contract
  723 physical / 685 non-empty lines，bool smoke 338 physical / 313 non-empty lines。
  备注：本切片仍不关闭 07-S5；remaining signed greater-equal comparison、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、broader ABI、dynamic value access helpers、
  runtime-failure-capable division/modulo policy 与 08-12 仍待后续。

- 2026-06-23 05:10:52 +08:00 · M1.5 / 07-S5 static i64 two-arg greater bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route
  从 signed less-than/equality/inequality 扩展到 signed greater-than。
  `backend_aot_c_typed_bool_thunks.c` 复用 shared i64 bool-return compare helper，
  新增 `LOGICAL_GREATER_SIGNED/FUNCTION_RETURN` wrapper，并在 mixed can-emit gate 中
  同时接受 `<`、`==`、`!=`、`>`。greater-than thunk 发出
  `return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);`。调用侧复用既有
  `zr_aot_static_i64_bool_two_arg_direct_call` route proof 与 writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `greater(50, 8)`
  case 缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；
  补 greater wrapper 与 writer branch 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 11/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 11/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-greater-bool-typed-thunk.md`。
  文件规模：bool thunk 主文件 563 physical / 490 non-empty lines，typed call contract
  721 physical / 683 non-empty lines，bool smoke 310 physical / 287 non-empty lines。
  备注：本切片仍不关闭 07-S5；remaining signed comparisons、inline structs、
  `in`/`out` writeback、deopt/dynamic bridges、broader ABI、dynamic value access helpers、
  runtime-failure-capable division/modulo policy 与 08-12 仍待后续。

- 2026-06-23 04:58:12 +08:00 · M1.5 / 07-S5 static i64 two-arg not-equal
  bool typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call
  route 从 signed less-than/equality 扩展到 signed inequality。
  `backend_aot_c_typed_bool_thunks.c` 复用 shared i64 bool-return compare helper，
  新增 `LOGICAL_NOT_EQUAL_SIGNED/FUNCTION_RETURN` wrapper，并在 mixed can-emit gate 中
  同时接受 `<`、`==`、`!=`。inequality thunk 发出
  `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`。调用侧复用既有
  `zr_aot_static_i64_bool_two_arg_direct_call` route proof 与 writer，继续发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `different(21, 22)`
  case 缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；
  补 not-equal wrapper 与 writer branch 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 10/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 10/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-not-equal-bool-typed-thunk.md`。
  文件规模：bool thunk 主文件 546 physical / 475 non-empty lines，typed call contract
  719 physical / 681 non-empty lines，bool smoke 282 physical / 261 non-empty lines。
  备注：本切片仍不关闭 07-S5；remaining signed comparisons、inline structs、`in`/`out`
  writeback、deopt/dynamic bridges、broader ABI、dynamic value access helpers、
  runtime-failure-capable division/modulo policy 与 08-12 仍待后续。

- 2026-06-23 04:43:21 +08:00 · M1.5 / 07-S5 static i64 two-arg equal bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：mixed `bool(i64, i64)` typed direct-call route 从
  signed less-than 扩展到 signed equality。`backend_aot_c_typed_bool_thunks.c`
  将 i64 bool-return recognizer 泛化为 shared compare helper，新增
  `LOGICAL_EQUAL_SIGNED/FUNCTION_RETURN` wrapper，并在 mixed can-emit gate 中同时接受
  `<` 与 `==`。equality thunk 发出
  `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`。调用侧复用既有
  `zr_aot_static_i64_bool_two_arg_direct_call` route proof 与 writer，继续要求 bool
  destination local、两个已写入 i64 argument locals，并发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 新增 `same(21, 21)` case
  缺少 `TZrInt64` 参数 bool thunk 且仍走 `CallStaticDirect` + `SyncBoolLocal`；补
  shared i64 compare recognizer、equality wrapper 与 writer branch 后 focused GREEN 为
  typed call contracts 4/0、bool typed direct-call smoke 9/0。测试结果：较宽 WSL GCC
  focused AOT 组继续通过 source 19/0、call contracts 4/0、typed call contracts 4/0、
  generic numeric contracts 1/0、global contracts 7/0、logical contracts 4/0、
  power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 9/0、u64 smoke 14/0、
  f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-equal-bool-typed-thunk.md`。
  文件规模：bool thunk 主文件 529 physical / 460 non-empty lines，typed direct-call
  726 physical / 649 non-empty lines，typed call contract 717 physical / 679 non-empty
  lines，bool smoke 254 physical / 235 non-empty lines。备注：本切片仍不关闭 07-S5；
  additional signed comparisons、inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、broader ABI、dynamic value access helpers、runtime-failure-capable
  division/modulo policy 与 08-12 仍待后续。

- 2026-06-23 04:30:53 +08:00 · M1.5 / 07-S5 static i64 two-arg less bool
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：新增 mixed scalar native signature 覆盖
  `bool smaller(int left, int right) { return left < right; }` 形态。
  `backend_aot_c_typed_bool_thunks.c` 识别 bool 返回、两个 i64 参数且函数体为
  `LOGICAL_LESS_SIGNED/FUNCTION_RETURN` 的 callee，并生成
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)`
  以及 `return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);`。
  `backend_aot_c_typed_direct_calls.c` 新增 route proof：destination 为 bool scalar local、
  `functionSlot + 1u` 与 `functionSlot + 2u` 均为已写入 i64 scalar local，callee 命中新
  recognizer 后发 direct typed call。`backend_aot_c_lowering_calls.c` 新增
  `zr_aot_static_i64_bool_two_arg_direct_call` writer，调用侧生成
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`，并允许 destination
  sync elision。RED/GREEN：RED 为 bool shared-library smoke 中新增
  `smaller(7, 9)` case 缺少 `TZrInt64` 参数 bool thunk declaration/definition 且仍走
  `ZrLibrary_AotRuntime_CallStaticDirect` + `SyncBoolLocal`；实现 recognizer、writer 与
  route 后 focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 8/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 8/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-i64-two-arg-less-bool-typed-thunk.md`。
  文件规模：bool thunk 主文件 506 physical / 440 non-empty lines，typed direct-call
  726 physical / 649 non-empty lines，call lowering 691 physical / 655 non-empty lines，
  bool smoke 226 physical / 209 non-empty lines。备注：本切片仍不关闭 07-S5；inline
  structs、`in`/`out` writeback、deopt/dynamic bridges、broader ABI、dynamic value access
  helpers、runtime-failure-capable division/modulo policy 与 08-12 仍待后续。

- 2026-06-23 04:01:46 +08:00 · M1.5 / 07-S5 static two-arg bool inequality
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_bool_thunks.c` 新增二参 bool
  `left != right` recognizer，复用 compact bool compare helper 覆盖
  `LOGICAL_NOT_EQUAL_BOOL/FUNCTION_RETURN` 形态；thunk 发出
  `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`。调用侧复用既有 bool two-arg direct-call
  writer 与 route proof，继续在 destination、`functionSlot + 1u`、`functionSlot + 2u`
  均为已写入 bool scalar locals 时发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`，并允许 destination
  sync elision。bool shared-library smoke 新增 `different(true, false)` 验证结果 42，
  并拒绝 `CallStaticDirect`、`CallStackValue` 与 typed destination sync。
  RED/GREEN：RED 为 typed call contract 缺少
  `backend_aot_c_try_get_bool_arg0_arg1_not_equal_return(`；smoke 同步证明旧生成物仍走
  `zr_aot_bool_compare_exec` / `SZrTypeValue` frame path；补 compare helper、not-equal wrapper、
  two-arg can-emit gate 与 exact writer 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 7/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、value SemIR 4/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 7/0、u64 smoke 14/0、
  f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、
  generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、power smoke 1/0。
  产出：`tests/acceptance/2026-06-23-aot-m1-5-static-bool-two-arg-not-equal-typed-thunk.md`。
  文件规模：bool thunk 主文件 446 physical / 388 non-empty lines，typed call contract
  695 physical / 657 non-empty lines，bool smoke 198 physical / 183 non-empty lines，
  bool smoke support 211 physical / 191 non-empty lines。
  备注：本切片仍不关闭 07-S5；inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、broader ABI 与 08-12 仍待后续。

- 2026-06-23 03:46:11 +08:00 · M1.5 / 07-S5 typed direct-call bool smoke
  support split · 状态：支持子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `tests/parser/aot_c_typed_direct_call_bool_smoke_support.h`，把 bool
  typed direct-call shared-library smoke 的项目写入、binary/AOT 生成、生成 C 断言、Unix
  shared-library 编译和 runtime entry 执行集中到 `SZrAotTypedDirectCallBoolSmokeCase`
  harness；`test_aot_c_typed_direct_call_bool_shared_library_smoke.c` 只保留 6 个具体 case
  与 `RUN_TEST` 列表。RED/GREEN：这是 support split，无新增行为 RED；拆分后 focused
  GREEN 为 bool typed direct-call smoke 6/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-typed-direct-call-bool-smoke-support-split.md`。
  文件规模：bool smoke 主文件从 914 physical / 847 non-empty lines 降至 170 physical /
  157 non-empty lines；新增 support header 211 physical / 191 non-empty lines。
  备注：本切片不关闭 07-S5，也不新增 typed thunk 行为；为后续 bool inequality 等小切片留出
  测试承载空间。

- 2026-06-23 03:39:28 +08:00 · M1.5 / 07-S5 static two-arg bool equality
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_bool_thunks.c` 新增二参 bool
  `left == right` recognizer，覆盖 `LOGICAL_EQUAL_BOOL/FUNCTION_RETURN` 形态；
  thunk 发出 `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`。调用侧复用既有
  bool two-arg direct-call writer 与 route proof，继续在 destination、
  `functionSlot + 1u`、`functionSlot + 2u` 均为已写入 bool scalar locals 时发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`，并允许
  destination sync elision。bool shared-library smoke 新增 `same(true, true)` 验证结果 42，
  并拒绝 `CallStaticDirect`、`CallStackValue` 与 typed destination sync。
  RED/GREEN：RED 为 typed call contract 缺少
  `backend_aot_c_try_get_bool_arg0_arg1_equal_return(`；smoke 同步证明旧生成物仍走
  `zr_aot_bool_compare_exec` / `SZrTypeValue` frame path；补 recognizer、two-arg
  can-emit gate 与 exact writer 后 focused GREEN 为 typed call contracts 4/0、
  bool typed direct-call smoke 6/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 4/0、typed call contracts 4/0、generic numeric contracts 1/0、
  global contracts 7/0、logical contracts 4/0、power contracts 2/0、frame setup 1/0、
  return 1/0、value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 6/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、
  logical smoke 4/0、power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-two-arg-equal-typed-thunk.md`。
  文件规模：bool thunk 主文件 423 physical / 368 non-empty lines，typed call contract
  692 physical / 654 non-empty lines，bool smoke 914 physical / 847 non-empty lines。
  备注：本切片仍不关闭 07-S5；bool inequality、inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、broader ABI 与 08-12 仍待后续。

- 2026-06-23 03:24:18 +08:00 · M1.5 / 07-S5 static one-arg bool logical-not
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_bool_thunks.c` 新增一参 bool
  `!flag` recognizer，覆盖 `LOGICAL_NOT_BOOL/FUNCTION_RETURN` 形态；thunk 发出
  `return (TZrBool)!zr_aot_arg0;`。调用侧复用既有 bool one-arg direct-call writer
  与 route proof，继续在 destination 和 `functionSlot + 1u` 均为已写入 bool scalar
  locals 时发 `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA)`，并允许
  destination sync elision。bool shared-library smoke 新增 `invert(false)` 验证结果 42，
  并拒绝 `CallStaticDirect`、`CallStackValue` 与 typed destination sync。
  RED/GREEN：RED 为 typed call contract 缺少
  `backend_aot_c_try_get_bool_arg0_logical_not_return(`；smoke 同步证明旧生成物仍走
  `zr_aot_bool_not_exec` / `SZrTypeValue` frame path；补 recognizer、can-emit gate 与
  exact writer 后 focused GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 5/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 5/0、
  u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-one-arg-logical-not-typed-thunk.md`。
  文件规模：bool thunk 主文件 375 physical / 325 non-empty lines，typed call contract
  689 physical / 651 non-empty lines，bool smoke 761 physical / 705 non-empty lines。
  备注：本切片仍不关闭 07-S5；inline structs、`in`/`out` writeback、deopt/dynamic
  bridges、broader ABI 与 08-12 仍待后续。

- 2026-06-23 03:04:46 +08:00 · M1.5 / 07-S5 static two-arg bool logical-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_bool_thunks.c` 新增二参 bool
  `left || right` recognizer，覆盖单条 `LOGICAL_OR/FUNCTION_RETURN` 与当前编译器生成的
  `GET_STACK/SET_STACK/JUMP_IF_BOOL_FALSE/JUMP/GET_STACK/SET_STACK/FUNCTION_RETURN`
  七指令短路形态；thunk 发出 `return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);`。
  调用侧复用既有 bool two-arg direct-call writer 与 route proof，继续在 destination、
  `functionSlot + 1u`、`functionSlot + 2u` 均为已写入 bool scalar locals 时发
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`，并允许 destination
  sync elision。bool shared-library smoke 新增 `either(false, true)` 验证结果 42，并拒绝
  `CallStaticDirect`、`CallStackValue` 与 typed destination sync。
  RED/GREEN：RED 为 typed call contract 缺少
  `backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(`；首版实现误用不存在的
  `JUMP_IF_BOOL_TRUE`，WSL GCC build 暴露真实指令集只提供 `JUMP_IF_BOOL_FALSE` 加普通
  `JUMP` 的 OR 短路形态；随后 contract 要求保留精确 `&&`/`||` 返回模板，最终 focused
  GREEN 为 typed call contracts 4/0、bool typed direct-call smoke 4/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 4/0、
  u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-23-aot-m1-5-static-bool-two-arg-logical-or-typed-thunk.md`。
  文件规模：bool thunk 主文件 333 physical / 288 non-empty lines，typed direct-call
  route 670 physical / 598 non-empty lines，call lowering 652 physical / 618 non-empty
  lines，typed call contract 686 physical / 648 non-empty lines，bool smoke 611 physical /
  566 non-empty lines。备注：本切片仍不关闭 07-S5；inline structs、`in`/`out` writeback、
  deopt/dynamic bridges、broader ABI 与 08-12 仍待后续。

- 2026-06-23 02:48:44 +08:00 · M1.5 / 07-S5 static two-arg bool logical-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_bool_thunks.c` 新增二参 bool
  `left && right` recognizer，覆盖单条 `LOGICAL_AND/FUNCTION_RETURN` 与当前编译器生成的
  `GET_STACK/SET_STACK/JUMP_IF_BOOL_FALSE/.../FUNCTION_RETURN` 短路形态；thunk 发出
  `return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);`。`backend_aot_c_lowering_calls.c`
  新增 bool two-arg direct-call writer，`backend_aot_c_typed_direct_calls.c` 证明
  destination、`functionSlot + 1u`、`functionSlot + 2u` 均为已写入 bool scalar locals
  后发 `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)`，并继续允许
  destination sync elision。bool shared-library smoke 新增 `both(true, false)` 验证结果 42，
  并拒绝 `CallStaticDirect`、`CallStackValue` 与 typed destination sync。
  RED/GREEN：RED 为 typed call contract 缺少 bool two-arg can-emit/writer；第一版
  GREEN 前 smoke 暴露真实 callee 是六指令短路形态，补短路 recognizer 后 focused GREEN 为
  typed call contracts 4/0、bool typed direct-call smoke 3/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、generic numeric contracts 1/0、global contracts 7/0、
  logical contracts 4/0、power contracts 2/0、frame setup 1/0、return 1/0、
  value SemIR 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 3/0、
  u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、typed scalar 1/0、
  value-type 1/0、generic numeric smoke 1/0、global smoke 9/0、logical smoke 4/0、
  power smoke 1/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-bool-two-arg-logical-and-typed-thunk.md`。
  文件规模：bool thunk 主文件 244 physical / 209 non-empty lines，typed direct-call
  route 670 physical / 598 non-empty lines，call lowering 652 physical / 618 non-empty
  lines，typed call contract 681 physical / 643 non-empty lines，bool smoke 458 physical /
  424 non-empty lines。备注：较宽 `ninja` rebuild 曾因共享工作树重新配置并输出 unrelated
  parser/type_inference/module-init warnings；最终 focused build 中本次 bool thunk 源无新增 warning。
  本切片仍不关闭 07-S5；inline structs、`in`/`out` writeback、deopt/dynamic bridges、
  broader ABI 与 08-12 仍待后续。

- 2026-06-22 19:29:45 +08:00 · M1.5 / 07-S5 static one-arg u64 bitwise-xor-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 在一参 u64
  bitwise-constant recognizer 上新增 `BITWISE_XOR` 包装，覆盖
  `func toggle(value: uint): uint { return value ^ 21; }` 的
  `SET_STACK/GET_CONSTANT/TO_UINT/BITWISE_XOR/FUNCTION_RETURN` 形态；thunk 继续使用
  一参 u64 ABI 并发出 `return (TZrUInt64)(zr_aot_arg0 ^ (TZrUInt64)21);`。调用侧复用现有
  u64 one-arg direct-call writer 与 route proof，并沿用 destination sync elision。
  RED/GREEN：RED 为 typed call contract 缺少
  `backend_aot_c_try_get_u64_arg0_bitwise_xor_constant_return(`；补 wrapper、can-emit gate
  与 writer 分支后 GREEN 为 focused build/relink 后 typed call contracts 4/0、u64 typed
  direct-call smoke 14/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 14/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-bitwise-xor-const-typed-thunk.md`。
  文件规模：u64 thunk 主文件 767 physical / 686 non-empty lines，typed call contract
  654 physical / 616 non-empty lines，u64 smoke 347 physical / 318 non-empty lines。
  备注：u64 一参 add/subtract/multiply/AND/OR/XOR-constant 与二参 add/subtract/multiply/
  AND/OR/XOR 窄直调已覆盖；除法/取模失败通道、inline structs、in/out 写回、
  deopt/dynamic bridges 与 07-S5 完整验收仍待后续。

- 2026-06-22 19:19:44 +08:00 · M1.5 / 07-S5 static one-arg u64 bitwise-or-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 在既有一参 u64
  bitwise-constant recognizer 上新增 `BITWISE_OR` 包装，覆盖
  `func flags(value: uint): uint { return value | 10; }` 的
  `SET_STACK/GET_CONSTANT/TO_UINT/BITWISE_OR/FUNCTION_RETURN` 形态；thunk 继续使用
  一参 u64 ABI 并发出 `return (TZrUInt64)(zr_aot_arg0 | (TZrUInt64)10);`。调用侧复用现有
  u64 one-arg direct-call writer 与 route proof，并沿用 destination sync elision。
  RED/GREEN：RED 为 typed call contract 缺少
  `backend_aot_c_try_get_u64_arg0_bitwise_or_constant_return(`；补 wrapper、can-emit gate
  与 writer 分支后 GREEN 为 focused build/relink 后 typed call contracts 4/0、u64 typed
  direct-call smoke 13/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 13/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-bitwise-or-const-typed-thunk.md`。
  文件规模：u64 thunk 主文件 752 physical / 672 non-empty lines，typed call contract
  651 physical / 613 non-empty lines，u64 smoke 323 physical / 296 non-empty lines。
  备注：u64 一参 add/subtract/multiply/bitwise-and/bitwise-or-constant 与二参
  add/subtract/multiply/AND/OR/XOR 窄直调已覆盖；u64 one-arg XOR constant、除法/取模失败通道、
  inline structs、in/out 写回、deopt/dynamic bridges 与 07-S5 完整验收仍待后续。

- 2026-06-22 19:06:21 +08:00 · M1.5 / 07-S5 static one-arg u64 bitwise-and-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增一参 u64
  bitwise-constant recognizer，覆盖当前 `func mask(value: uint): uint { return value & 58; }`
  生成的 `SET_STACK/GET_CONSTANT/TO_UINT/BITWISE_AND/FUNCTION_RETURN` 形态；thunk 继续使用
  一参 u64 ABI 并发出 `return (TZrUInt64)(zr_aot_arg0 & (TZrUInt64)58);`。调用侧复用现有
  u64 one-arg direct-call writer 与 route proof，并沿用 destination sync elision。
  RED/GREEN：RED1 为 typed call contract 缺少
  `backend_aot_c_try_get_u64_arg0_bitwise_and_constant_return(`；实现后 RED2 为 u64 smoke
  证明实际 SemIR 带 `TO_UINT` 常量转换、未生成期望 thunk；补五指令形态后 GREEN 为 focused
  build/relink 后 typed call contracts 4/0、u64 typed direct-call smoke 12/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 12/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-bitwise-and-const-typed-thunk.md`。
  文件规模：u64 thunk 主文件 737 physical / 658 non-empty lines，typed call contract
  648 physical / 610 non-empty lines，u64 smoke 299 physical / 274 non-empty lines。
  备注：u64 一参 add/subtract/multiply/bitwise-and-constant 与二参 add/subtract/multiply/
  AND/OR/XOR 窄直调已覆盖；u64 one-arg OR/XOR constant、除法/取模失败通道、inline structs、
  in/out 写回、deopt/dynamic bridges 与 07-S5 完整验收仍待后续。

- 2026-06-22 18:49:12 +08:00 · M1.5 / 07-S5 static two-arg u64 bitwise-xor
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.c` 新增二参 u64
  bitwise-xor recognizer，并把 AND/OR/XOR 共用的二参 bitwise 形态校验收敛为私有
  `backend_aot_c_try_get_u64_arg0_arg1_bitwise_return()`；识别
  `func toggle(left: uint, right: uint): uint { return left ^ right; }` 的 `BITWISE_XOR`
  + `FUNCTION_RETURN` 形态，操作数允许 arg0/arg1 任一顺序，thunk 继续使用二参 u64 ABI
  并发出 `return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);`。调用侧复用现有 u64
  two-arg direct-call writer 与 route proof，并沿用 destination sync elision。
  RED/GREEN：RED 为 typed call contract 缺少 u64 `^` 返回表达式；GREEN 为 focused
  build/relink 后 typed call contracts 4/0、u64 typed direct-call smoke 11/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 11/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-bitwise-xor-typed-thunk.md`。
  文件规模：u64 shape 源 405 physical / 368 non-empty lines，u64 thunk 主文件
  584 physical / 524 non-empty lines，u64 smoke 275 physical / 252 non-empty lines。
  备注：u64 二参 add/subtract/multiply/AND/OR/XOR 窄直调已覆盖；除法/取模失败通道、
  inline structs、in/out 写回、deopt/dynamic bridges 与 07-S5 完整验收仍待后续。

- 2026-06-22 18:41:03 +08:00 · M1.5 / 07-S5 static two-arg u64 bitwise-or
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.c` 新增二参 u64
  bitwise-or recognizer，识别 `func combine(left: uint, right: uint): uint { return left | right; }`
  的 `BITWISE_OR` + `FUNCTION_RETURN` 形态；操作数允许 arg0/arg1 任一顺序，thunk
  继续使用二参 u64 ABI 并发出
  `return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);`。调用侧复用现有 u64 two-arg
  direct-call writer 与 route proof，继续生成
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)` 并沿用
  destination sync elision。RED/GREEN：RED 为 typed call contract 缺少 u64 `|`
  返回表达式；GREEN 为 focused build/relink 后 typed call contracts 4/0、u64 typed
  direct-call smoke 10/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 10/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、
  value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-bitwise-or-typed-thunk.md`。
  文件规模：u64 shape 源 428 physical / 390 non-empty lines，u64 thunk 主文件
  579 physical / 519 non-empty lines，u64 smoke 250 physical / 229 non-empty lines。
  备注：这仍是窄 u64 二参 `|` 覆盖；u64 `^`、除法/取模失败通道、inline structs、
  in/out 写回、deopt/dynamic bridges 与 07-S5 完整验收仍待后续。

- 2026-06-22 18:31:51 +08:00 · M1.5 / 07-S5 static two-arg u64 bitwise-and
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunk_shapes.c` 新增二参 u64
  bitwise-and recognizer，识别 `func mask(left: uint, right: uint): uint { return left & right; }`
  的 `BITWISE_AND` + `FUNCTION_RETURN` 形态；操作数允许 arg0/arg1 任一顺序，thunk
  继续使用二参 u64 ABI 并发出
  `return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);`。调用侧复用现有 u64 two-arg
  direct-call writer 与 route proof，继续生成
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)` 并沿用
  destination sync elision。RED/GREEN：RED 为 typed call contract 缺少 u64 `&`
  返回表达式；GREEN 为 focused build/relink 后 typed call contracts 4/0、u64 typed
  direct-call smoke 9/0。测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、
  call contracts 4/0、typed call contracts 4/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 9/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、
  value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-bitwise-and-typed-thunk.md`。
  文件规模：u64 shape 源 392 physical / 358 non-empty lines，u64 thunk 主文件
  574 physical / 514 non-empty lines，u64 smoke 225 physical / 206 non-empty lines。
  备注：这仍是窄 u64 二参 `&` 覆盖；u64 `|`/`^`、除法/取模失败通道、inline structs、
  in/out 写回、deopt/dynamic bridges 与 07-S5 完整验收仍待后续。

- 2026-06-22 18:17:42 +08:00 · M1.5 / 07-S5 typed call contract split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `tests/parser/test_aot_c_typed_call_contracts.c` 与
  `zr_vm_aot_c_typed_call_contracts_test`，把 i64/bool/u64/f64 typed thunk source
  contract 从 `test_aot_c_call_contracts.c` 拆出；原 call contracts 现在只保留
  quickened dynamic、generic direct、static direct、meta boundary 四类 VM/调用边界契约。
  `tests/CMakeLists.txt` 将新目标纳入 language pipeline 与 smoke pipeline 清单。文件规模：
  `test_aot_c_call_contracts.c` 从 923 physical / 866 non-empty lines 降到
  386 physical / 348 non-empty lines；新 typed call contract 文件为
  635 physical / 597 non-empty lines。RED/GREEN：RED 为新 CMake 目标指向尚不存在的
  `test_aot_c_typed_call_contracts.c` 并在配置阶段失败；GREEN 为 focused build/relink 后
  call contracts 4/0、typed call contracts 4/0、u64 typed direct-call smoke 8/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 4/0、
  typed call contracts 4/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 8/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-typed-call-contract-split.md`。
  备注：本切片不新增 typed thunk 行为；其目的只是为后续 07-S5 typed direct-call
  扩展解除测试文件过大与职责混杂风险，07-S5 完整验收仍未完成。

- 2026-06-22 16:56:34 +08:00 · M1.5 / 07-S5 typed u64 thunk shape split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_u64_thunk_shapes.{h,c}`，把 u64 二参
  add/multiply/subtract return 形态识别从 `backend_aot_c_typed_u64_thunks.c`
  拆出；主 u64 thunk 文件只保留类型检查、常量/一参 recognizer、can-emit gate 与
  thunk definition writer，并通过新头文件复用二参 recognizer。文件规模从拆分前
  `backend_aot_c_typed_u64_thunks.c` 908 physical / 820 non-empty lines 收敛到
  569 physical / 509 non-empty lines；新 shape 源文件为 356 physical / 326 non-empty lines，
  新头文件为 10 physical / 7 non-empty lines。RED/GREEN：RED 为 call contracts
  读取新 shape 文件时得到 NULL；GREEN 为 focused target build/relink 后 call contracts
  8/0、u64 typed direct-call smoke 8/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 8/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 8/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-typed-u64-thunk-shape-split.md`。
  备注：本切片不新增 typed thunk 行为；`test_aot_c_call_contracts.c` 已到
  923 physical / 866 non-empty lines，后续再扩展 call contract 前应先拆分该测试。

- 2026-06-22 16:39:43 +08:00 · M1.5 / 07-S5 static two-arg u64 multiply
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增二参 u64
  multiply recognizer，识别 `func product(left: uint, right: uint): uint { return left * right; }`
  的 direct unsigned/signed multiply、`MUL_SIGNED_LOAD_STACK`、窄 `TO_INT`，
  以及当前 parameter-copy + `TO_INT` + signed-multiply + `FUNCTION_RETURN` 形态；
  乘法接受 arg0/arg1 任一操作数顺序，thunk 继续使用二参 u64 ABI 并发出
  `return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1);`。调用侧复用 u64 two-arg
  direct-call writer 与 route proof，继续生成
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)` 并沿用
  destination sync elision。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_u64_arg0_arg1_multiply_return(`；GREEN 为 focused
  target build/relink 后 call contracts 8/0、u64 typed direct-call smoke 8/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 8/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-multiply-typed-thunk.md`。
  备注：仅覆盖 u64 二参 multiply-return；更广 u64 表达式、除法/取模策略、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 16:29:39 +08:00 · M1.5 / 07-S5 static one-arg u64 multiply-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增一参 u64
  multiply-constant recognizer，识别 `func scale(value: uint): uint { return value * 21; }`
  的 const-op 或 `GET_CONSTANT` / optional parameter-copy + `TO_INT` + `MUL_*`
  + `FUNCTION_RETURN` 形态；乘法接受 argument/constant 任一侧，但只接受 unsigned
  常量或非负 signed 常量；thunk 继续使用一参 u64 ABI 并发出
  `return (TZrUInt64)(zr_aot_arg0 * (TZrUInt64)K);`。调用侧复用 u64 one-arg
  direct-call writer 与 route proof，继续生成
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_u64_arg0_multiply_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、u64 typed direct-call smoke 7/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 7/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-multiply-const-typed-thunk.md`。
  备注：仅覆盖 u64 一参 multiply-constant；更广 u64 表达式、多参非加/减/乘法、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 16:18:35 +08:00 · M1.5 / 07-S5 u64 typed direct-call smoke support
  split 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：新增 `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`，
  把 u64 typed direct-call smoke 的项目写入、binary/AOT C 生成、生成物 needle scan、
  shared-library 编译与 AOT runtime 执行抽成 reusable case runner；具体 u64 smoke
  收敛为 6 个 `SZrAotTypedDirectCallU64SmokeCase` 定义与 `main()`，从 909 physical /
  842 non-empty lines 降到 151 physical / 138 non-empty lines，支撑头文件为 211 physical /
  191 non-empty lines。RED/GREEN：本支撑切片无新增行为 RED；GREEN 为 target rebuild
  后 u64 typed direct-call smoke 6/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  source 19/0、call contracts 8/0、shared 8/0、call smoke 5/0、typed direct-call 5/0、
  bool smoke 2/0、u64 smoke 6/0、f64 smoke 11/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-typed-direct-call-u64-smoke-support-split.md`。
  备注：支撑重构不新增 typed thunk 行为；为后续 u64 route 扩展避免继续放大测试文件；
  07-S5 完整验收仍未完成。

- 2026-06-22 16:06:28 +08:00 · M1.5 / 07-S5 static one-arg u64 subtract-constant
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_typed_u64_thunks.c` 新增一参 u64
  subtract-constant recognizer，识别 `func dec(value: uint): uint { return value - 8; }`
  的 const-op 或 `GET_CONSTANT` / optional parameter-copy + `TO_INT` + `SUB_*`
  + `FUNCTION_RETURN` 形态；保持有序，只接受 argument 在左、非负常量在右；
  thunk 继续使用一参 u64 ABI 并发出
  `return (TZrUInt64)(zr_aot_arg0 - (TZrUInt64)K);`。调用侧复用 u64 one-arg
  direct-call writer 与 route proof，继续证明 destination 与 argument 均有 u64
  scalar-local coverage，且 argument 在 call 前已写入，再生成
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)` 并沿用 destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_u64_arg0_subtract_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、u64 typed direct-call smoke 6/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 6/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-u64-one-arg-subtract-const-typed-thunk.md`。
  备注：仅覆盖 u64 一参 subtract-constant；更广 u64 表达式、多参非加/减法、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 15:54:25 +08:00 · M1.5 / 07-S5 static one-arg f64 negate typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 negate recognizer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func negate(value: float): float { return -value; }` 的 `NEG_FLOAT` + `FUNCTION_RETURN`
  形态，并接受当前可见的参数 copy 前缀；thunk 保持一参 f64 ABI，发出
  `return (TZrFloat64)(-zr_aot_arg0);`。调用侧复用 f64 one-arg direct-call writer
  与 route gate，继续证明 destination 与 argument 均有 f64 scalar-local coverage，
  且 argument 在 call 前已写入，再生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_negate_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 11/0。
  测试结果：较宽 WSL GCC focused AOT 组重复执行后通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 11/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-negate-typed-thunk.md`。
  备注：仅覆盖 f64 一参 negate；f64 动态除法/取模、更广表达式、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 15:43:22 +08:00 · M1.5 / 07-S5 static one-arg f64 modulo-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 modulo-constant recognizer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func remainder(value: float): float { return value % 50.0; }` 的 `GET_CONSTANT` + `MOD_FLOAT`
  + `FUNCTION_RETURN` 形态，并接受当前可见的参数 copy 前缀；取模保持有序，只接受
  argument 在左、非零常量在右，零常量返回 false 不进入 direct thunk；发出
  `return (TZrFloat64)fmod(zr_aot_arg0, (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route gate，继续证明 destination 与 argument 均有 f64 scalar-local coverage，
  且 argument 在 call 前已写入，再生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`。
  `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h` 同步把 Unix shared-library 编译命令
  补上 `-lm`，使生成 C 中的 `fmod` 与既有 float/generic numeric smoke 的数学库链接策略一致。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_modulo_constant_return(`；
  首次 GREEN 尝试暴露 f64 smoke 支撑层缺少 `-lm` 导致 generated `.so` 链接 `fmod` 失败；
  修正支撑层后 focused target build/relink 通过 call contracts 8/0、f64 typed direct-call smoke 10/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 10/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-modulo-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参非零 modulo-constant；f64 动态除法/取模、更广表达式、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 15:30:30 +08:00 · M1.5 / 07-S5 static one-arg f64 divide-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 divide-constant recognizer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func halve(value: float): float { return value / 2.0; }` 的 `GET_CONSTANT` + `DIV_FLOAT`
  + `FUNCTION_RETURN` 形态，并接受当前可见的参数 copy 前缀；除法保持有序，只接受
  argument 在左、非零常量在右，零常量返回 false 不进入 direct thunk；发出
  `return (TZrFloat64)(zr_aot_arg0 / (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route gate，继续证明 destination 与 argument 均有 f64 scalar-local coverage，
  且 argument 在 call 前已写入，再生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_divide_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 9/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 9/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-divide-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参非零 divide-constant；f64 运行期除零/取模、更广表达式、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 15:18:48 +08:00 · M1.5 / 07-S5 static one-arg f64 multiply-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 multiply-constant recognizer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func scale(value: float): float { return value * 21.0; }` 的 `GET_CONSTANT` + `MUL_FLOAT`
  + `FUNCTION_RETURN` 形态，并接受当前可见的参数 copy 前缀；发出
  `return (TZrFloat64)(zr_aot_arg0 * (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route gate，继续证明 destination 与 argument 均有 f64 scalar-local coverage，
  且 argument 在 call 前已写入，再生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_multiply_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 8/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 8/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-multiply-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参 multiply-constant；f64 除法/取模、更广表达式、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 15:09:08 +08:00 · M1.5 / 07-S5 static one-arg f64 subtract-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 subtract-constant recognizer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func dec(value: float): float { return value - 8.0; }` 的 `GET_CONSTANT` + `SUB_FLOAT`
  + `FUNCTION_RETURN` 形态，并接受当前可见的参数 copy 前缀；发出
  `return (TZrFloat64)(zr_aot_arg0 - (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route gate，继续证明 destination 与 argument 均有 f64 scalar-local coverage，
  且 argument 在 call 前已写入，再生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_subtract_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 7/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 7/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-subtract-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参 subtract-constant；f64 除法/取模/乘常量、更广表达式、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:53:25 +08:00 · M1.5 / 07-S5 static one-arg f64 add-constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 add-constant recognizer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func inc(value: float): float { return value + 37.0; }` 的 `GET_CONSTANT` + `ADD_FLOAT`
  + `FUNCTION_RETURN` 形态，并接受当前可见的参数 copy 前缀；发出
  `return (TZrFloat64)(zr_aot_arg0 + (TZrFloat64)K);`。调用侧复用 f64 one-arg
  direct-call writer 与 route gate，继续证明 destination 与 argument 均有 f64 scalar-local coverage，
  且 argument 在 call 前已写入，再生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_f64_arg0_add_constant_return(`；
  GREEN 为 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 6/0。
  测试结果：较宽 WSL GCC focused AOT 组继续通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 6/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-add-const-typed-thunk.md`。
  备注：仅覆盖 f64 一参 add-constant；f64 除法/取模/乘常量/减常量、更广表达式、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:46:01 +08:00 · M1.5 / 07-S5 f64 typed direct-call smoke support split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`，把 f64 typed
  direct-call shared-library smoke 的 compile/source write/binary write/hash/AOT C emit/generated C
  needle scan/shared-library compile/AOT runtime execute 公共模板抽成
  `run_aot_c_typed_direct_call_f64_smoke()` 与 `SZrAotTypedDirectCallF64SmokeCase`；具体
  `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c` 收敛为 5 个 case 定义与
  `main()`，避免后续 f64 thunk route 继续复制 100+ 行工程构建样板。
  RED/GREEN：行为保持重构，无新增 RED 行为契约；重构后 focused f64 typed direct-call smoke
  5/0。测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 5/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-typed-direct-call-f64-smoke-support-split.md`。
  备注：支撑重构不新增 typed thunk 行为；07-S5 的 f64 除法/取模/常量表达式、inline struct、
  in/out、deopt/dynamic bridge 与完整验收仍未完成。

- 2026-06-22 14:38:18 +08:00 · M1.5 / 07-S5 static two-arg f64 multiply typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增二参 f64 multiply-return recognizer，
  在 callable return 与两个参数均为 float/double metadata、no-varargs 前提下，识别
  `func product(left: float, right: float): float { return left * right; }` 的 `MUL_FLOAT`
  后接 `FUNCTION_RETURN` 形态，并复用二参 f64 thunk writer 发出
  `return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1);`。调用侧复用 f64 two-arg
  direct-call writer 与 route gate，继续证明 destination、first argument、second argument
  均有 f64 scalar-local coverage，且两个 argument 在 call 前已写入，再生成
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)`。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_f64_arg0_arg1_multiply_return(`；GREEN 为 focused target
  build/relink 后 call contracts 8/0、f64 typed direct-call smoke 5/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 5/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-two-arg-multiply-typed-thunk.md`。
  备注：仅覆盖 f64 二参 multiply-return typed thunk；f64 除法/取模/常量表达式、更多参数/返回形态、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:30:18 +08:00 · M1.5 / 07-S5 static two-arg f64 subtract typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增二参 f64 subtract-return recognizer，
  在 callable return 与两个参数均为 float/double metadata、no-varargs 前提下，识别
  `func diff(left: float, right: float): float { return left - right; }` 的 `SUB_FLOAT`
  后接 `FUNCTION_RETURN` 形态，并复用二参 f64 thunk 声明与 writer 发出
  `return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1);`。调用侧复用 f64 two-arg
  direct-call writer 与 route gate，继续证明 destination、first argument、second argument
  均有 f64 scalar-local coverage，且两个 argument 在 call 前已写入，再生成
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)`。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_f64_arg0_arg1_subtract_return(`；首个 GREEN 命令被超时截断，
  第二次 focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 4/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 4/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-two-arg-subtract-typed-thunk.md`。
  备注：仅覆盖 f64 二参 subtract-return typed thunk；f64 乘除/常量表达式、更多参数/返回形态、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:19:22 +08:00 · M1.5 / 07-S5 static two-arg f64 add typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增二参 f64 add-return recognizer/writer，
  在 callable return 与两个参数均为 float/double metadata、no-varargs 前提下，识别
  `func sum(left: float, right: float): float { return left + right; }` 的 `ADD_FLOAT`
  后接 `FUNCTION_RETURN` 形态，并发出
  `static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1)`
  以及 `return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1);`；
  `backend_aot_c_lowering_calls.c` 新增 f64 two-arg direct-call writer，生成
  `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)`，仅在
  scalar-local consumer 证明不足时同步 `nativeDouble` destination value slot；
  `backend_aot_c_typed_direct_calls.c` 新增 f64 two-arg route gate，证明 destination、
  first argument、second argument 均有 f64 scalar-local coverage，且两个 argument 在 call 前已写入。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function)`；GREEN 为
  focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 3/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 3/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-two-arg-add-typed-thunk.md`。
  备注：仅覆盖 f64 二参 add-return typed thunk；f64 非加法表达式、更多参数/返回形态、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 14:06:21 +08:00 · M1.5 / 07-S5 static one-arg f64 identity typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_f64_thunks.c` 新增一参 f64 identity-return recognizer/writer，
  在 callable return 与单个参数均为 float/double metadata、no-varargs 前提下，识别
  `func pass(value: float): float { return value; }` 并发出
  `static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0)`
  以及 `return zr_aot_arg0;`；`backend_aot_c_lowering_calls.c` 新增 f64 one-arg
  direct-call writer，生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA)`，仅在
  scalar-local consumer 证明不足时同步 `nativeDouble` destination value slot；
  `backend_aot_c_typed_direct_calls.c` 新增 f64 one-arg route gate，证明 destination 与
  call-window argument 均有 f64 scalar-local coverage，且 argument 在 call 前已写入。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function)`；GREEN 为
  focused target build/relink 后 call contracts 8/0、f64 typed direct-call smoke 2/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 8/0、shared 8/0、
  call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 2/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  产出：`tests/acceptance/2026-06-22-aot-m1-5-static-f64-one-arg-typed-thunk.md`。
  备注：仅覆盖 f64 一参 identity-return typed thunk；f64 表达式返回、多参 f64、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 13:53:07 +08:00 · M1.5 / 07-S5 static no-arg f64 constant typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：新增 `backend_aot_c_typed_f64_thunks.{h,c}`，识别 zero-arg float/double
  constant-return callee，并发出
  `static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state)` 与
  `return (TZrFloat64)%.17g;`；`backend_aot_c_emitter.c` 接入 f64 typed thunk
  forward declarations/definitions；`backend_aot_c_lowering_calls.c` 新增 f64 no-arg
  direct-call writer，生成 `zr_aot_fD = zr_aot_typed_f64_fn_N(state)`，仅在
  scalar-local consumer 证明不足时同步 `nativeDouble` destination value slot；
  `backend_aot_c_typed_direct_calls.c` 新增 f64 no-arg route gate，并在普通 static-call
  与 no-arg 专用 helper 两处路由。RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function)`；
  中间 GREEN 前 f64 smoke 发现 thunk 已生成但 no-arg 专用路由缺少 f64 分支，仍落到
  `ZrLibrary_AotRuntime_CallStaticDirect`，补 route 后 GREEN。focused GREEN 为
  call contracts 8/0、f64 typed direct-call smoke 1/0。测试结果：较宽 WSL GCC
  focused AOT 组通过 source 19/0、call contracts 8/0、shared 8/0、call smoke 5/0、
  typed direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、f64 smoke 1/0、arithmetic 5/0、
  bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、
  logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-f64-no-arg-typed-thunk.md`。
  备注：仅覆盖 f64 no-arg constant-return typed thunk；f64 参数、f64 表达式返回、
  inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 13:31:10 +08:00 · M1.5 / 07-S5 static two-arg u64 subtract typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_u64_thunks.c` 新增二参 u64 subtract-return recognizer/writer
  分支，在 callable return 与两个参数均为 unsigned integer metadata、no-varargs 前提下，
  识别 `func diff(left: uint, right: uint): uint { return left - right; }` 的直接二参减法返回；
  recognizer 覆盖 `SUB_UNSIGNED`/`SUB_UNSIGNED_PLAIN_DEST`、`SUB_SIGNED`/
  `SUB_SIGNED_PLAIN_DEST`，并接受保持 operand 顺序的 `TO_INT` 转换/参数 copy 后再
  signed subtract 的窄形态；writer 发出
  `return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1);`。调用侧复用已落地的 u64 two-arg
  direct-call writer 与 route gate，继续证明 destination、first argument、second argument
  均为已写入 u64 C local 后才直调，并沿用 scalar-local destination sync elision。
  RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_u64_arg0_arg1_subtract_return(`；GREEN 为 focused target build/relink 后
  call contracts 7/0、u64 typed direct-call smoke 5/0。测试结果：较宽 WSL GCC focused
  AOT 组通过 source 19/0、call contracts 7/0、shared 8/0、call smoke 5/0、typed
  direct-call 5/0、bool smoke 2/0、u64 smoke 5/0、arithmetic 5/0、bitwise 6/0、
  typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、logical 4/0、
  power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-subtract-typed-thunk.md`。
  备注：仅覆盖 u64 二参 subtract-return typed thunk；更广 u64 表达式、多参非加/减法、
  f64、inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 13:15:44 +08:00 · M1.5 / 07-S5 static two-arg u64 add typed thunk direct-call
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_typed_u64_thunks.c` 新增二参 u64 add-return recognizer/writer，
  在 callable return 与两个参数均为 unsigned integer metadata、no-varargs 前提下，识别
  `func sum(left: uint, right: uint): uint { return left + right; }` 的直接二参加法返回；
  recognizer 覆盖 `ADD_UNSIGNED`/`ADD_UNSIGNED_PLAIN_DEST`、当前可见的 signed add
  与 `ADD_SIGNED_LOAD_STACK` 形态，并接受 `TO_INT` 转换/参数 copy 后再 signed add 的
  窄形态；writer 发出
  `static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1)`
  以及 `return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);`。
  `backend_aot_c_lowering_calls.c` 新增 u64 two-arg direct-call writer，生成
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA, zr_aot_uB)`，并只在 scalar-local
  result proof 无法满足后续 consumer 时同步 destination stack slot；
  `backend_aot_c_typed_direct_calls.c` 新增 route gate，证明 destination、first argument
  与 second argument 均为已写入 u64 C local 后才走直调。RED/GREEN：RED 为 call
  contracts 缺少
  `backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function)`；GREEN 为
  focused target build/relink 后 call contracts 7/0、u64 typed direct-call smoke 4/0。
  测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 7/0、shared
  8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 4/0、arithmetic
  5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、global 9/0、
  logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。产出：
  `tests/acceptance/2026-06-22-aot-m1-5-static-u64-two-arg-typed-thunk.md`。备注：仅覆盖
  u64 二参 add-return typed thunk；更广 u64 表达式、多参非加法、f64、inline struct、
  in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 12:47:02 +08:00 · M1.5 / 07-S5 static one-arg u64 add-constant typed thunk direct-call
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_typed_u64_thunks.c` 将一参 u64 typed thunk 从 identity-return 扩展到
  `func inc(value: uint): uint { return value + 1; }` 的非负常量加法返回；recognizer 覆盖
  当前 SemIR 的参数 copy、`GET_CONSTANT`、`TO_INT`、`ADD_SIGNED`/`ADD_SIGNED_PLAIN_DEST`、
  `FUNCTION_RETURN` 序列，并保留 callable return / parameter metadata / no-varargs 守卫；
  writer 发出 `return (TZrUInt64)(zr_aot_arg0 + (TZrUInt64)%llu);`，继续复用一参 u64
  direct-call 路由和 scalar-local destination sync 规则；call contracts 固定 add-constant helper、
  `TO_INT` 与 signed-add opcode 形状；u64 shared-library smoke 新增 `inc(seed)`，验证 runtime
  结果 42，并继续拒绝 `CallStaticDirect`、`CallStackValue` 与不必要的 typed-destination sync。
  RED/GREEN：RED 为 call contracts 缺少 `backend_aot_c_try_get_u64_arg0_add_constant_return(`；
  中间 GREEN 前 u64 smoke 暴露当前 `uint + 1` 下沉为 `TO_INT` + `ADD_SIGNED` 而不是 unsigned
  add opcode，修正 recognizer 后 GREEN；focused GREEN 为 call contracts 7/0、u64 typed direct-call
  smoke 3/0。测试结果：较宽 WSL GCC focused AOT 组通过 source 19/0、call contracts 7/0、
  shared 8/0、call smoke 5/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 3/0、
  arithmetic 5/0、bitwise 6/0、typed scalar 1/0、value-type 1/0、generic numeric 1/0、
  global 9/0、logical 4/0、power smoke 1/0、frame setup 1/0、return 1/0、value SemIR 4/0。
  备注：标准 target relink 被并行 `type_inference` 未解析符号阻断；u64 smoke 使用同一 WSL GCC
  build 对象和 `--allow-shlib-undefined` 手动重链接后运行通过。此切片只覆盖一参 u64 非负常量加法返回；
  u64 多参/更广表达式、f64、inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 11:49:26 +08:00 · M1.5 / 07-S5 static one-arg u64 identity typed thunk direct-call
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_typed_u64_thunks.c` 新增一参 u64 identity-return recognizer/writer，
  识别 `func pass(value: uint): uint { return value; }`，发出
  `static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0)` 并直接
  `return zr_aot_arg0`；`backend_aot_c_lowering_calls.c` 新增 u64 one-arg direct-call writer，
  生成 `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)`；`backend_aot_c_typed_direct_calls.c`
  证明调用窗口参数槽是已写入 u64 C local 后选择该 route，并继续用 scalar-local result proof
  跳过 destination value-slot sync；`backend_aot_c_scalar_stack_copy.c` 修正参数暂存 copy 的
  static type 选择，目标槽静态类型不匹配时回看 source/local static type，确保调用参数需要物化
  value-slot 时从 u64 source local 写入，而不是从未同步的 frame slot 复制 0。RED/GREEN：
  RED 为 call contracts 缺少 `backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function)`；
  中间 GREEN 前 u64 smoke 暴露参数暂存 copy 将 `uint` 实参同步为 0（结果 5 而非 42），
  修正 source-local type fallback 后 GREEN；focused GREEN 为 source contracts 19/0、
  call contracts 7/0、u64 typed direct-call smoke 2/0。测试结果：较宽 WSL GCC focused AOT 组通过
  source 19/0、call contracts 7/0、typed direct-call 5/0、bool smoke 2/0、u64 smoke 2/0、
  arithmetic 5/0、bitwise 6/0、call smoke 5/0、shared 8/0、value-type 1/0、power 2/0 + 1/0、
  generic numeric 1/0 + 1/0、global 9/0、logical 4/0 + 4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0。产出：u64 identity recognizer/writer、u64 one-arg direct-call writer、
  typed route proof、u64 shared-library smoke 扩展、scalar stack-copy source-local fallback contract、
  acceptance 记录。备注：仅覆盖一参 u64 identity-return；u64 表达式返回/多参、f64、inline struct、
  in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 11:20:28 +08:00 · M1.5 / 07-S5 static no-arg u64 constant typed thunk direct-call
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `backend_aot_c_typed_u64_thunks.{h,c}`，识别零参数、返回 unsigned integer
  metadata 的常量返回 callee，并发出 `static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state)`；
  `backend_aot_c_emitter.c` 接入 u64 typed thunk 声明与定义；`backend_aot_c_lowering_calls.c`
  新增 `backend_aot_write_c_static_direct_u64_no_arg_function_call()`，调用侧可写
  `zr_aot_uD = zr_aot_typed_u64_fn_N(state)`，并继续只在后续 consumer 需要 frame value-slot 时同步；
  `backend_aot_c_typed_direct_calls.c` 在 static no-arg 分派中优先检查 u64 route，避免 `uint`
  返回误落回 i64/fallback 路径。RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function)`；中间 GREEN 前暴露
  `uint` surface 当前表现为 `UINT32/U32` metadata 且常量可为非负 signed literal，以及 no-arg router
  的 i64-first 顺序会绕过 u64 direct-call；最终 GREEN 为 call contracts 7/0、u64 typed direct-call smoke
  1/0。测试结果：较宽 WSL GCC focused AOT 组通过 typed direct-call 5/0、bool smoke 2/0、u64 smoke
  1/0、arithmetic 5/0、bitwise 6/0、call smoke 5/0、shared 8/0、value-type 1/0、power 2/0 + 1/0、
  source 19/0、generic numeric 1/0 + 1/0、global 9/0、logical 4/0 + 4/0、typed scalar 1/0、return
  1/0、frame setup 1/0。产出：u64 typed thunk recognizer/writer、u64 no-arg direct-call writer、
  u64 shared-library smoke、call source contract、acceptance 记录。备注：仅覆盖 no-arg u64 constant-return；
  u64 参数/表达式返回、f64、inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 10:56:26 +08:00 · M1.5 / 07-S5 static one-arg bool identity typed thunk direct-call 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_typed_bool_thunks.c` 新增一参 bool identity return recognizer，
  识别 `func pass(flag: bool): bool { return flag; }` 形态并发出
  `static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0)`；
  `backend_aot_c_lowering_calls.c` 新增 bool one-arg direct-call writer，生成
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA)`；`backend_aot_c_typed_direct_calls.c`
  在调用窗口证明 `functionSlot + 1` 是已写入 bool C local 后选择该 typed route，并继续只在必要时同步
  destination value-slot。RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function)`；GREEN 为 call
  contracts 6/0、bool typed direct-call smoke 2/0。测试结果：较宽 WSL GCC focused AOT 组继续通过
  typed direct-call 5/0、bool smoke 2/0、arithmetic 5/0、bitwise 6/0、call smoke 5/0、shared 8/0、
  value-type 1/0、power 2/0 + 1/0、source 19/0、generic numeric 1/0 + 1/0、global 9/0、
  logical 4/0 + 4/0、typed scalar 1/0、return 1/0、frame setup 1/0。产出：bool one-arg thunk
  recognizer/writer、bool one-arg direct-call writer、typed route proof、bool shared-library smoke 扩展、
  call source contract、acceptance 记录。备注：仅覆盖 bool 一参 identity-return；bool 非 identity 表达式、
  多参 bool、u64/f64、inline struct、in/out、deopt/dynamic bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 10:44:07 +08:00 · M1.5 / 07-S5 static no-arg bool typed thunk direct-call 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `backend_aot_c_typed_bool_thunks.{h,c}`，识别零参数、返回 bool 常量的 typed callee，
  并发出 `static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state)` forward decl/definition；
  `backend_aot_c_emitter.c` 接入 bool typed thunk 声明与定义；`backend_aot_c_lowering_calls.c`
  新增 `backend_aot_write_c_static_direct_bool_no_arg_function_call()`，调用侧可写
  `zr_aot_bD = zr_aot_typed_bool_fn_N(state)`，仅在后续 consumer 需要 frame value-slot 时同步；
  `backend_aot_c_typed_direct_calls.{h,c}` 承接 bool no-arg 与既有 i64 no/one/two-arg static typed
  direct-call 路由选择，避免继续扩大超大 `backend_aot_c_function_body.c`。RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function)`；GREEN 为 call
  contracts 6/0、bool typed direct-call smoke 1/0、既有 typed direct-call smoke 5/0。测试结果：
  较宽 WSL GCC focused AOT 组通过 call contracts 6/0、typed direct-call smoke 5/0、bool smoke
  1/0、arithmetic smoke 5/0、bitwise smoke 6/0、call smoke 5/0、shared smoke 8/0、value-type
  1/0、power 2/0 + 1/0、source 19/0、generic numeric 1/0 + 1/0、global 9/0、logical 4/0 + 4/0、
  typed scalar 1/0、return 1/0、frame setup 1/0。产出：bool typed thunk 模块、typed direct-call
  routing 模块、bool shared-library smoke、call source contract、acceptance 记录。备注：仅覆盖
  bool no-arg constant-return direct thunk；bool 参数、u64/f64、inline struct、in/out、deopt/dynamic
  bridge 与 07-S5 完整验收仍未完成。

- 2026-06-22 10:11:42 +08:00 · M1.5 / 07-S5 typed i64 thunk definition writer helper
  consolidation 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_c_typed_i64_thunks.c` 新增 no-arg、one-arg、two-arg i64 thunk definition
  writer helpers，`backend_aot_write_c_typed_i64_thunks()` 现在把现有 direct-return 定义模板统一交给
  helper 写出；生成 C 表面保持不变，source contract 锁定 helper 名称。RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少 `backend_aot_c_write_i64_no_arg_thunk_definition(`；GREEN 为
  call contracts 5/0、typed direct-call smoke 5/0、arithmetic typed direct-call smoke 5/0、bitwise typed
  direct-call smoke 6/0。测试结果：较宽 WSL GCC focused AOT 组通过 call contracts 5/0、typed direct-call
  smoke 5/0、arithmetic smoke 5/0、bitwise smoke 6/0、call smoke 5/0、shared smoke 8/0、value-type smoke
  1/0、power contracts/smoke 2/0 + 1/0、source contracts 19/0、generic numeric contracts/smoke 1/0 + 1/0、
  global smoke 9/0、logical contracts/smoke 4/0 + 4/0、typed scalar 1/0、return contracts 1/0、frame setup
  contracts 1/0。产出：typed i64 thunk definition writer helper 合并、call contract 断言、acceptance
  记录。备注：仅为支撑重构，不新增 thunk 行为，不关闭 07-S5；为 bool/u64/f64 或更宽 typed return ABI
  铺垫。

- 2026-06-22 10:00:14 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-xor-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 识别扩展到 `return arg0 ^ signed-constant`；共享 bitwise constant
  recognizer 现在通过 `backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return()`
  接入 `BITWISE_XOR`，继续接受可证明参数转存前缀 `GET_STACK` / `SET_STACK`
  后接 `GET_CONSTANT` + bitwise op + `FUNCTION_RETURN`，并允许 constant
  operand 位于左右任一侧。生成 C 会为
  `flip(value:int): int { return value ^ 6; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 ^ (TZrInt64)6)`；调用侧复用 one-arg i64 direct-call
  gate 与 scalar-local-only destination stack-sync elision · RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(` source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke
  6/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、
  typed direct-call smoke 5/0、typed direct-call arithmetic smoke 5/0、typed
  direct-call bitwise smoke 6/0、call smoke 5/0、shared smoke 8/0、value-type
  1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-xor-const-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 bitwise-xor constant 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为
  791 physical / 708 non-empty lines，bitwise smoke 为 191 physical /
  177 non-empty lines，bitwise support header 为 197 physical / 178 non-empty lines。

- 2026-06-22 09:52:35 +08:00 · M1.5 / 07-S5 typed direct-call bitwise
  smoke support split · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：抽出
  `tests/parser/aot_c_typed_direct_call_bitwise_smoke_support.h`，把 Unix
  shared-library 编译/执行、AOT C 生成物断言、项目路径构造、binary/hash/blob
  准备等重复 helper 从 bitwise smoke 主文件移出；主文件保留 5 个具体 bitwise
  typed thunk case 和 `main()`，覆盖 two-arg OR/XOR、one-arg NOT、one-arg
  AND-constant、one-arg OR-constant。主文件从 753 physical / 697 non-empty
  lines 降到 162 physical / 150 non-empty lines，新增 support header 为
  197 physical / 178 non-empty lines · RED/GREEN：无新增行为 RED；这是测试支撑
  拆分 · 测试结果：WSL GCC focused
  `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test` 5/0 · 产出：
  `tests/parser/aot_c_typed_direct_call_bitwise_smoke_support.h`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-direct-call-bitwise-smoke-support-split.md` ·
  备注：此切片不改变 production AOT 行为、不新增 thunk 形态，也不关闭 07-S5；它只为
  后续 bitwise typed direct-call 扩展保留测试文件空间。

- 2026-06-22 09:42:53 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-or-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 识别扩展到 `return arg0 | signed-constant`；`backend_aot_c_typed_i64_thunks.c`
  将 bitwise constant recognizer 泛化为按 opcode 匹配 `BITWISE_AND` /
  `BITWISE_OR`，继续接受可证明参数转存前缀 `GET_STACK` / `SET_STACK` 后接
  `GET_CONSTANT` + bitwise op + `FUNCTION_RETURN`，并允许 constant operand 位于
  左右任一侧。生成 C 会为 `flags(value:int): int { return value | 10; }`
  发出 `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 | (TZrInt64)10)`；调用侧复用 one-arg i64 direct-call
  gate 与 scalar-local-only destination stack-sync elision · RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(` source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke
  5/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、
  typed direct-call smoke 5/0、typed direct-call arithmetic smoke 5/0、typed
  direct-call bitwise smoke 5/0、call smoke 5/0、shared smoke 8/0、value-type
  1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-or-const-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 bitwise-or constant 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为
  775 physical / 693 non-empty lines，bitwise smoke 为 753 physical /
  697 non-empty lines；继续扩展 bitwise smoke 前应先抽测试支撑 helper。

- 2026-06-22 09:34:08 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-and-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed
  thunk 识别扩展到 `return arg0 & signed-constant`；`backend_aot_c_typed_i64_thunks.c`
  现在识别可证明的参数转存前缀 `GET_STACK` / `SET_STACK` 后接 `GET_CONSTANT` +
  `BITWISE_AND` + `FUNCTION_RETURN`，并允许 constant operand 位于左右任一侧。
  生成 C 会为 `maskBy(value:int): int { return value & 47; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 & (TZrInt64)47)`；调用侧复用 one-arg i64 direct-call
  gate 与 scalar-local-only destination stack-sync elision · RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(` source contract；
  初次 GREEN 前的 bitwise smoke 又暴露编译器会先把参数槽 0 转存到局部槽 1，补
  可证明参数转存前缀后 GREEN；最终 GREEN 为 call contracts 5/0、typed
  direct-call bitwise shared-library smoke 4/0 · 测试结果：WSL GCC broader AOT
  focused group 通过 call contracts 5/0、typed direct-call smoke 5/0、typed
  direct-call arithmetic smoke 5/0、typed direct-call bitwise smoke 4/0、call
  smoke 5/0、shared smoke 8/0、value-type 1/0、power 2/0+1/0、source 19/0、
  generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、
  return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-and-const-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 bitwise-and constant 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为
  751 physical / 671 non-empty lines，bitwise smoke 为 604 physical /
  559 non-empty lines。

- 2026-06-22 09:21:19 +08:00 · M1.5 / 07-S5 typed i64 thunk recognizer
  helper consolidation 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：为 typed i64 thunk 模块补
  `backend_aot_c_try_get_i64_arg0_unary_return()` 与
  `backend_aot_c_try_get_i64_arg0_arg1_binary_return()` 两个共享识别 helper；
  一元 helper 集中校验 one-arg i64 返回形态并服务 `NEG_SIGNED` /
  `BITWISE_NOT`，二元 helper 集中校验 two-arg i64 简单 operand1 返回形态并服务
  `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` 与 `BITWISE_AND` / `BITWISE_OR` /
  `BITWISE_XOR`。`ADD` / `MUL` 仍保留各自 LOAD_STACK 变体逻辑，避免扩大行为面 ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_unary_return(` source contract；GREEN 为
  call contracts 5/0、typed direct-call arithmetic shared-library smoke 5/0、
  typed direct-call bitwise shared-library smoke 3/0 · 测试结果：WSL GCC broader
  AOT focused group 通过 call contracts 5/0、typed direct-call smoke 5/0、
  typed direct-call arithmetic smoke 5/0、typed direct-call bitwise smoke 3/0、
  call smoke 5/0、shared smoke 8/0、value-type 1/0、power 2/0+1/0、source
  19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar
  1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-i64-thunk-recognizer-helper-consolidation.md` ·
  备注：此切片不新增 thunk 行为形态、不声明 07-S5 完成；它把
  `backend_aot_c_typed_i64_thunks.c` 从 762 physical / 682 non-empty lines
  降到 670 physical / 600 non-empty lines，为继续扩展 typed→typed 直调返回形态
  降低重复识别风险。

- 2026-06-22 09:09:06 +08:00 · M1.5 / 07-S5 static one-arg i64
  bitwise-not typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed thunk 识别扩展到
  `return ~arg0`；`backend_aot_c_typed_i64_thunks.c` 现在识别 `BITWISE_NOT` +
  `FUNCTION_RETURN`，并要求 unary operand 为参数槽 0、return slot 为 bitwise
  result。生成 C 会为 `invert(value:int): int { return ~value; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(~zr_aot_arg0)`；调用侧复用 one-arg i64 direct-call gate 与
  scalar-local-only destination stack-sync elision · RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_bitwise_not_return(` source contract；GREEN 为
  call contracts 5/0、typed direct-call bitwise shared-library smoke 3/0 ·
  测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、typed
  direct-call smoke 5/0、typed direct-call arithmetic smoke 5/0、typed direct-call
  bitwise smoke 3/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-bitwise-not-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 bitwise-not 返回；一般表达式返回、inline
  struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍属后续
  07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为 762 physical /
  682 non-empty lines，bitwise smoke 为 455 physical / 421 non-empty lines。

- 2026-06-22 08:59:28 +08:00 · M1.5 / 07-S5 static two-arg i64
  bitwise-xor typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别扩展到
  `return arg0 ^ arg1`；`backend_aot_c_typed_i64_thunks.c` 现在识别
  `BITWISE_XOR` + `FUNCTION_RETURN`，并要求左右 operand 分别为参数槽 0/1、
  return slot 为 bitwise result。生成 C 会为
  `toggle(left:int, right:int): int { return left ^ right; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1)`；调用侧复用 two-arg i64
  direct-call gate 与 scalar-local-only destination stack-sync elision ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(` source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke
  2/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、
  typed direct-call smoke 5/0、typed direct-call arithmetic smoke 5/0、
  typed direct-call bitwise smoke 2/0、call smoke 5/0、shared smoke 8/0、
  value-type 1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、
  global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  产出：`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-bitwise-xor-typed-thunk.md` ·
  备注：此切片只覆盖 two-arg i64 bitwise-xor 返回；一般表达式返回、inline
  struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍属后续
  07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为 723 physical /
  647 non-empty lines，bitwise smoke 为 306 physical / 283 non-empty lines。

- 2026-06-22 08:51:23 +08:00 · M1.5 / 07-S5 static two-arg i64
  bitwise-or typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk 识别扩展到
  `return arg0 | arg1`；`backend_aot_c_typed_i64_thunks.c` 现在识别
  `BITWISE_OR` + `FUNCTION_RETURN`，并要求左右 operand 分别为参数槽 0/1、
  return slot 为 bitwise result。生成 C 会为
  `join(left:int, right:int): int { return left | right; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 | zr_aot_arg1)`；调用侧复用 two-arg i64
  direct-call gate 与 scalar-local-only destination stack-sync elision ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(` source contract；
  GREEN 为 call contracts 5/0、typed direct-call bitwise shared-library smoke
  1/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、
  typed direct-call smoke 5/0、typed direct-call arithmetic smoke 5/0、
  typed direct-call bitwise smoke 1/0、call smoke 5/0、shared smoke 8/0、
  value-type 1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、
  global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 ·
  产出：`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c`、
  `tests/CMakeLists.txt`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-bitwise-or-typed-thunk.md` ·
  备注：此切片只覆盖 two-arg i64 bitwise-or 返回；一般表达式返回、inline
  struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍属后续
  07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为 683 physical /
  611 non-empty lines，新 bitwise smoke 为 156 physical / 144 non-empty lines。

- 2026-06-22 08:41:44 +08:00 · M1.5 / 07-S5 typed direct-call arithmetic
  smoke support split 支撑切片 · 状态：支撑切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：将 typed direct-call arithmetic
  shared-library smoke 的通用 Unix shared-library 编译/读写/hash/compile helper
  抽到 `tests/parser/aot_c_typed_direct_call_arithmetic_smoke_support.h`，
  保留原测试文件只承载具体 arithmetic thunk 用例与 `main()`；主 smoke 从 863
  physical / 786 non-empty lines 降到 753 physical / 697 non-empty lines，
  支撑头文件为 116 physical / 93 non-empty lines · RED/GREEN：此为测试支撑拆分，
  不新增生产行为 RED；GREEN 为
  `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test` 5/0 ·
  产出：`tests/parser/aot_c_typed_direct_call_arithmetic_smoke_support.h`、
  `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-direct-call-arithmetic-smoke-support-split.md` ·
  备注：此切片不扩大 typed thunk 覆盖面、不声明 07-S5 完成；它只降低继续执行
  07-S5 typed direct-call 子切片时的测试文件增长风险。

- 2026-06-22 08:34:57 +08:00 · M1.5 / 07-S5 static two-arg i64
  bitwise-and typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64
  typed thunk 识别扩展到 `return arg0 & arg1`；`backend_aot_c_typed_i64_thunks.c`
  现在识别 `BITWISE_AND` + `FUNCTION_RETURN`，并要求左右 operand 分别为参数槽
  0/1、return slot 为 bitwise result。生成 C 会为
  `mask(left:int, right:int): int { return left & right; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)`，直接返回
  `(TZrInt64)(zr_aot_arg0 & zr_aot_arg1)`；调用侧复用 two-arg i64
  direct-call gate 与 scalar-local-only destination stack-sync elision ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(` source
  contract；GREEN 为 call contracts 5/0、typed direct-call arithmetic
  shared-library smoke 5/0 · 测试结果：WSL GCC broader AOT focused group 通过
  call contracts 5/0、typed direct-call smoke 5/0、typed direct-call arithmetic
  smoke 5/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、power 2/0+1/0、
  source 19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、
  typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-bitwise-and-typed-thunk.md` ·
  备注：此切片只覆盖 two-arg i64 bitwise-and 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。变更后 `backend_aot_c_typed_i64_thunks.c` 为 643
  physical / 575 non-empty lines，typed direct-call arithmetic smoke 为 863
  physical / 786 non-empty lines；后续继续增长该 smoke 前应优先拆分。

- 2026-06-22 08:20:53 +08:00 · M1.5 / 07-S5 static one-arg i64
  negate typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64
  typed thunk 识别扩展到 `return -arg0`；`backend_aot_c_typed_i64_thunks.c`
  现在识别 `NEG_SIGNED` + `FUNCTION_RETURN`，并要求 unary operand 为参数槽 0、
  return slot 为 negation result。生成 C 会为
  `negate(value:int): int { return -value; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(-zr_aot_arg0)`；调用侧复用 one-arg i64 direct-call gate 与
  scalar-local-only destination stack-sync elision · RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_negate_return(` source contract；GREEN 为
  call contracts 5/0、typed direct-call arithmetic shared-library smoke 4/0 ·
  测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、typed
  direct-call smoke 5/0、typed direct-call arithmetic smoke 4/0、call smoke
  5/0、shared smoke 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic
  numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 产出：`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-negate-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 unary negation 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。

- 2026-06-22 08:05:24 +08:00 · M1.5 / 07-S5 static one-arg i64
  subtract-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64
  typed thunk 识别从 add/multiply constant 扩展到
  `return arg0 - signed-constant`；`backend_aot_c_typed_i64_thunks.c`
  现在识别 `SUB_SIGNED_CONST` / `SUB_SIGNED_CONST_PLAIN_DEST` /
  `SUB_SIGNED_LOAD_STACK_CONST`，也保留 materialized constant 的
  `GET_CONSTANT` + `SUB_SIGNED_LOAD_CONST` / `SUB_SIGNED` 形态，随后
  `FUNCTION_RETURN`。生成 C 会为 `decBy(value:int): int { return value - 8; }`
  发出 `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 - (TZrInt64)8)`；调用侧复用 one-arg i64
  direct-call gate 与 scalar-local-only destination stack-sync elision ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_subtract_constant_return(` source
  contract；GREEN 为 call contracts 5/0、typed direct-call arithmetic
  shared-library smoke 3/0 · 测试结果：WSL GCC broader AOT focused group
  通过 call contracts 5/0、typed direct-call smoke 5/0、typed direct-call
  arithmetic smoke 3/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-subtract-const-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 减 signed constant 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。

- 2026-06-22 07:54:45 +08:00 · M1.5 / 07-S5 typed i64 thunk module split
  支撑切片 · 状态：支撑切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：将 no-arg / one-arg / two-arg i64 typed thunk
  识别、forward declaration 与 thunk definition 发射从
  `backend_aot_c_emitter.c` 拆到
  `backend_aot_c_typed_i64_thunks.{h,c}`；`backend_aot_c_emitter.c`
  只保留调用面并降到 421 行，新模块为 419 行。`backend_aot_c_call_contracts_test`
  的 source contract 改为检查新模块，确保既有 constant / identity /
  add-constant / multiply-constant / add / subtract / multiply recognizer
  仍被约束 · RED/GREEN：本切片为防止主 emitter 继续膨胀的结构性拆分；
  拆分后 focused GREEN 为 call contracts 5/0、typed direct-call arithmetic
  shared-library smoke 2/0。首次验证尝试因 unrelated WSL semantic 构建超时
  中断，随后重跑 focused 与 broader AOT group 均通过 · 测试结果：WSL GCC
  broader AOT focused group 通过 call contracts 5/0、typed direct-call smoke
  5/0、typed direct-call arithmetic smoke 2/0、call smoke 5/0、shared smoke
  8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、
  global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup
  1/0 · 产出：`backend_aot_c_typed_i64_thunks.h`、
  `backend_aot_c_typed_i64_thunks.c`、`backend_aot_c_emitter.c`、
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-i64-thunk-module-split.md` ·
  备注：这是支撑性模块拆分，不新增 typed thunk 覆盖面；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。

- 2026-06-22 07:37:35 +08:00 · M1.5 / 07-S5 static one-arg i64
  multiply-constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64
  typed thunk 识别从 identity / add-constant 扩展到
  `return arg0 * signed-constant`；`backend_aot_c_emitter.c` 现在识别
  `MUL_SIGNED_CONST` / `MUL_SIGNED_CONST_PLAIN_DEST` /
  `MUL_SIGNED_LOAD_STACK_CONST`，也保留 materialized constant 的
  `GET_CONSTANT` + `MUL_SIGNED_LOAD_CONST` / `MUL_SIGNED` 形态，随后
  `FUNCTION_RETURN`。生成 C 会为 `scale(value:int): int { return value * 21; }`
  发出 `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 * (TZrInt64)21)`；调用侧复用 one-arg i64
  direct-call gate 与 scalar-local-only destination stack-sync elision ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_multiply_constant_return(` source
  contract；GREEN 为 call contracts 5/0、typed direct-call arithmetic
  shared-library smoke 2/0 · 测试结果：WSL GCC broader AOT focused group
  通过 call contracts 5/0、typed direct-call smoke 5/0、typed direct-call
  arithmetic smoke 2/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-multiply-const-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 乘 signed constant 返回；一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。

- 2026-06-22 07:28:09 +08:00 · M1.5 / 07-S5 static two-arg i64
  multiply typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk
  识别从加/减法扩展到 `return arg0 * arg1`；`backend_aot_c_emitter.c`
  现在识别 `MUL_SIGNED` / `MUL_SIGNED_PLAIN_DEST` / `MUL_SIGNED_LOAD_STACK`
  + `FUNCTION_RETURN` 的两参数乘法返回，并生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)` 直接返回
  `(TZrInt64)(zr_aot_arg0 * zr_aot_arg1)`。调用侧复用 two-arg i64
  direct-call gate 与 scalar-local-only destination stack-sync elision；
  新增独立 arithmetic shared-library smoke，避免继续扩张既有 800+ 行
  typed direct-call smoke · RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test`
  缺少 `backend_aot_c_try_get_i64_arg0_arg1_multiply_return(` source
  contract；GREEN 为 call contracts 5/0、typed direct-call arithmetic
  shared-library smoke 1/0 · 测试结果：WSL GCC broader AOT focused group
  通过 call contracts 5/0、typed direct-call smoke 5/0、typed direct-call
  arithmetic smoke 1/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c`、
  `tests/CMakeLists.txt`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-multiply-typed-thunk.md` ·
  备注：此切片只覆盖 two-arg i64 乘法返回；一般表达式返回、inline struct、
  in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍属后续 07-S5 工作。

- 2026-06-22 07:13:54 +08:00 · M1.5 / 07-S5 static two-arg i64
  subtract typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk
  识别从加法扩展到 `return arg0 - arg1`；`backend_aot_c_emitter.c` 现在识别
  `SUB_SIGNED` / `SUB_SIGNED_PLAIN_DEST` + `FUNCTION_RETURN` 的两参数减法返回，
  并生成 `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)` 直接返回
  `(TZrInt64)(zr_aot_arg0 - zr_aot_arg1)`。调用侧复用上一切片的 two-arg
  i64 direct-call gate 与 scalar-local-only destination stack-sync elision ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_try_get_i64_arg0_arg1_subtract_return(` source contract；GREEN
  为 call contracts 5/0、typed direct-call shared-library smoke 5/0 · 测试结果：
  WSL GCC broader AOT focused group 通过 call contracts 5/0、typed direct-call
  smoke 5/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、power 2/0+1/0、
  source 19/0、generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、
  typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-subtract-typed-thunk.md` ·
  备注：此切片只覆盖 two-arg i64 减法返回；一般表达式返回、inline struct、
  in/out 写回、deopt/dynamic bridge 与更广 typed return ABI 仍属后续 07-S5 工作。

- 2026-06-22 07:04:09 +08:00 · M1.5 / 07-S5 static two-arg i64
  typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：two-arg i64 typed thunk
  识别新增 `return arg0 + arg1` 形态；`backend_aot_c_emitter.c` 现在识别
  `ADD_SIGNED` / `ADD_SIGNED_PLAIN_DEST` / `ADD_SIGNED_LOAD_STACK` +
  `FUNCTION_RETURN` 的两参数加法返回，并生成
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0, zr_aot_arg1)` 直接 C thunk。
  函数体调用分发新增两参数门控，证明 `functionSlot + 1` 与
  `functionSlot + 2` 均为调用前已写入的 i64 scalar local 后，输出
  `zr_aot_static_i64_two_arg_direct_call`，直接调用 typed thunk 并保留
  scalar-local-only destination stack-sync elision · RED/GREEN：RED 为
  `zr_vm_aot_c_call_contracts_test` 缺少
  `backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function)`
  source contract；GREEN 为 call contracts 5/0、typed direct-call shared-library
  smoke 4/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts
  5/0、typed direct-call smoke 4/0、call smoke 5/0、shared smoke 8/0、
  value-type 1/0、power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、
  global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup
  1/0 · 产出：`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.{h,c}`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-two-arg-typed-thunk.md` ·
  备注：此切片只覆盖 two-arg i64 参数加法返回；一般多参数 ABI、一般表达式返回、
  inline struct、in/out 写回、deopt/dynamic bridge 与更广 typed return ABI
  仍属后续 07-S5 工作。

- 2026-06-22 06:38:57 +08:00 · M1.5 / 07-S5 static one-arg i64
  arg+constant typed thunk direct-call 切片 · 状态：子切片完成、07-S5 部分完成、
  M1.5/07 部分完成、08-12 未开始 · 完成项目：one-arg i64 typed thunk 识别从
  identity return 扩展到 `return arg0 + signed-constant`；`backend_aot_c_emitter.c`
  现在识别两指令 `ADD_SIGNED_LOAD_STACK_CONST` + `FUNCTION_RETURN` 的 SemIR
  标量加法形态，也保留旧 `ADD_SIGNED_CONST` / materialized-constant 形态。
  生成 C 会为 `inc(value:int): int { return value + 1; }` 发出
  `zr_aot_typed_i64_fn_N(state, zr_aot_arg0)`，直接返回
  `(TZrInt64)(zr_aot_arg0 + (TZrInt64)1)`；调用侧复用既有 one-arg i64
  typed direct-call route，并在 scalar-local-only 证明成立时继续不写
  typed-destination stack slot · RED/GREEN：RED 为 call contracts 缺少
  `backend_aot_c_try_get_i64_arg0_add_constant_return(` source shape；实现旧
  bytecode-only 形态后，typed direct-call smoke 又暴露 SemIR
  `ADD_SIGNED_LOAD_STACK_CONST` 未生成 typed thunk；GREEN 为 call contracts 5/0、
  typed direct-call shared-library smoke 3/0 · 测试结果：WSL GCC broader AOT
  focused group 通过 call contracts 5/0、typed direct-call smoke 3/0、call smoke 5/0、
  shared smoke 8/0、value-type 1/0、power 2/0+1/0、source 19/0、generic numeric
  1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 产出：`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-add-const-typed-thunk.md` ·
  备注：此切片只覆盖 one-arg i64 参数 + signed constant 返回；multi-arg、
  一般表达式返回、inline struct、in/out 写回、deopt/dynamic bridge 与更广的
  typed return ABI 仍属后续 07-S5 工作。

- 2026-06-22 06:21:30 +08:00 · M1.5 / 07-S5 static i64 typed direct-call
  stack-sync elision 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：no-arg / one-arg typed direct-call writers now accept
  `syncStackSlot`; function-body static call routing uses
  `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(functionIr, destinationSlot, instructionIndex)`
  to skip the temporary destination `SZrTypeValue` lookup and
  `ZR_VALUE_FAST_SET(zr_aot_typed_destination, ...)` when all reachable later
  consumers can read the i64 scalar local. The fallback path still syncs the
  destination stack slot for frame-backed consumers. The typed direct-call smoke
  now rejects `zr_aot_static_i64_*_direct_call_sync_stack_slot`,
  `SZrTypeValue *zr_aot_typed_destination`, and typed-destination
  `ZR_VALUE_FAST_SET` in the scalar-only no-arg and one-arg examples ·
  RED/GREEN：RED 为 `zr_vm_aot_c_call_contracts_test` 缺少 `TZrBool syncStackSlot`
  source contract；GREEN 为 call contracts 5/0、typed direct-call shared-library
  smoke 2/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts 5/0、
  typed direct-call smoke 2/0、call smoke 5/0、shared smoke 8/0、value-type 1/0、
  power 2/0+1/0、source 19/0、generic numeric 1/0+1/0、global 9/0、
  logical 4/0+4/0、typed scalar 1/0、return 1/0、frame setup 1/0 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-typed-direct-call-stack-sync-elision.md` ·
  备注：此切片只移除已证明 scalar-local-only 的 no-arg/one-arg i64 typed direct-call
  destination 栈同步；multi-arg calls, non-identity/general returns, inline structs,
  in/out writeback, and deopt/dynamic bridges remain later 07-S5 work.

- 2026-06-22 05:56:00 +08:00 · M1.5 / 07-S5 static one-arg i64 typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：C AOT emitter now recognizes typed single-parameter
  i64 identity-return functions and emits `zr_aot_typed_i64_fn_N(struct SZrState *,
  TZrInt64)` forward declarations/definitions. Function-body call routing proves the
  first call argument slot as an i64 scalar local with `backend_aot_c_scalar_locals_i64_written_before()`,
  then emits `zr_aot_static_i64_one_arg_direct_call`, assigns the destination
  `zr_aot_sN` directly from `zr_aot_typed_i64_fn_N(state, zr_aot_sArg)`, and syncs
  the current destination stack slot with `ZR_VALUE_FAST_SET` for remaining
  frame-backed consumers. The typed direct-call shared-library smoke now executes both
  `answer(): int { return 42; }` and `echo(value: int): int { return value; }`
  without generated `CallStaticDirect` / `CallStackValue` in those direct typed paths ·
  RED/GREEN：RED 为 call contracts 缺少 one-arg typed thunk predicate/writer/source
  shape，typed direct-call smoke 缺少 generated one-arg thunk/direct call；GREEN 为
  call contracts 5/0、typed direct-call shared-library smoke 2/0 · 测试结果：WSL GCC
  broader AOT focused group 通过 call contracts 5/0、typed direct-call smoke 2/0、
  call smoke 5/0、shared smoke 8/0、value-type 1/0、power 2/0+1/0、source 19/0、
  generic numeric 1/0+1/0、global 9/0、logical 4/0+4/0、typed scalar 1/0、return 1/0、
  frame setup 1/0 · 产出：`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.{h,c}`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-one-arg-typed-thunk.md` ·
  备注：this advances typed parameter ABI only for the narrow one-i64-arg identity
  return case. Multi-arg calls, non-identity/general returns, inline structs, in/out
  writeback, deopt/dynamic bridge boxing, and removing the temporary destination
  stack-slot sync remain later 07-S5 work.

- 2026-06-22 05:42:18 +08:00 · M1.5 / 07-S5 static no-arg i64 typed thunk
  direct-call 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：C AOT emitter now recognizes typed no-arg
  constant-i64-return functions and emits `zr_aot_typed_i64_fn_N` forward declarations/definitions;
  call lowering routes eligible static no-arg i64 calls directly to the typed thunk,
  writes `zr_aot_sN`, and syncs the current destination stack slot with
  `ZR_VALUE_FAST_SET` so remaining frame-backed consumers still observe the value.
  The dedicated shared-library smoke executes `answer(): int { return 42; }`
  through the direct typed thunk path and rejects `CallStaticDirect` /
  `CallStackValue` in generated source · RED/GREEN：RED 为 call contracts 缺少
  typed thunk predicate/writer/source shape，typed direct smoke 缺少 generated
  thunk/direct call；GREEN 为 call contracts 5/0、typed direct-call shared-library
  smoke 1/0、aggregate shared-library smoke 8/0 · 测试结果：focused WSL GCC
  validation 通过 `zr_vm_aot_c_call_contracts_test`、`zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`
  和 `zr_vm_aot_c_shared_library_smoke_test` · 产出：
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-i64-no-arg-typed-thunk.md` ·
  备注：此切片只覆盖 static no-arg constant i64 typed-to-typed 调用；typed 参数 ABI、
  一般返回、inline struct、in/out 写回、deopt/dynamic bridge 仍未完成。

- 2026-06-22 04:43:15 +08:00 · M1.5 / 07-S5 AOT MethodInfo table publication / module descriptor 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：public AOT ABI bumped to version 3 and `ZrAotCompiledModule` now exposes `methodInfos` / `methodInfoCount`; generated C emits `static const SZrAotMethodInfo *const zr_aot_method_infos[]` pointing at each `zr_aot_method_info_N`; the generated module descriptor publishes that table and count so shared-library consumers can reach non-null per-function signature descriptors through `module->methodInfos[0]->signature` · RED/GREEN：RED 为 shared-library smoke compile failure because `ZrAotCompiledModule` lacked `methodInfos` / `methodInfoCount` fields；GREEN 为 frame setup contracts 1/0、source contracts 19/0、shared-library smoke 8/0 · 测试结果：WSL GCC broader AOT focused group 通过 call contracts 4/0、call shared-library smoke 5/0、shared-library smoke 8/0、value-type shared-library smoke 1/0、power contracts/smoke 2/0+1/0、source contracts 19/0、generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical contracts/smoke 4/0+4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 · 产出：`zr_vm_common/include/zr_vm_common/zr_aot_abi.h`、`zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`、`tests/parser/test_aot_c_frame_setup_contracts.c`、`tests/parser/test_aot_c_source_contracts.c`、`tests/parser/test_aot_c_shared_library_smoke.c`、`tests/acceptance/2026-06-21-aot-m1-5-method-info-table-publication.md` · 备注：本切片只发布 MethodInfo/signature 描述符表，供后续 typed-to-typed direct C call/return routing 使用；actual direct call/return lowering、in/out writeback、deopt/dynamic bridge 仍未完成。

- 2026-06-22 04:31:28 +08:00 · M1.5 / 07-S5 AOT MethodInfo native
  signature descriptor 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：public AOT ABI now defines `SZrAotSignatureType`
  and `SZrAotSignature`; the C backend emits per-function
  `zr_aot_signature_%u_types` and `zr_aot_signature_%u` constants from
  `SZrFunctionTypedTypeRef` return/parameter metadata, and
  `SZrAotMethodInfo.signature` now points at the generated descriptor instead of
  `ZR_NULL`. The value typed-call shared-library smoke locks a one-parameter callee
  descriptor with non-null return/parameter type pointers, giving later typed-to-typed
  native signature routing a real MethodInfo signature source · RED/GREEN：RED 为
  `zr_vm_aot_c_frame_setup_contracts_test` 缺少 `SZrAotSignatureType` /
  `SZrAotSignature` ABI definitions and generated signature constants；GREEN 为 frame
  setup contracts 1/0、typed scalar 1/0、call shared-library smoke 5/0 · 测试结果：
  WSL GCC broader AOT focused group 通过 call contracts 4/0、call shared-library smoke
  5/0、shared-library smoke 8/0、value-type shared-library smoke 1/0、power
  contracts/smoke 2/0+1/0、source contracts 19/0、generic numeric contracts/smoke
  1/0+1/0、global smoke 9/0、logical contracts/smoke 4/0+4/0、typed scalar 1/0、
  return contracts 1/0、frame setup contracts 1/0 · 产出：`zr_aot_abi.h`、
  `backend_aot_c_emitter.c`、`test_aot_c_frame_setup_contracts.c`、
  `test_aot_c_typed_scalar.c`、`test_aot_c_call_shared_library_smoke.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-method-info-signature-descriptor.md` ·
  备注：this is the metadata/signature descriptor foundation for 07-S5 boundary
  marshaling; actual direct typed-to-typed C call/return lowering, in/out writeback,
  and deopt/dynamic bridge boxing remain open.

- 2026-06-22 04:08:25 +08:00 · M1.5 / 07-S5 TO_BOOL bool scalar-local
  declaration follow-up 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、
  08-12 未开始 · 完成项目：`backend_aot_c_scalar_locals_kind_from_conversion_opcode()`
  now maps `ZR_INSTRUCTION_OP_TO_BOOL` to `ZR_AOT_SCALAR_LOCAL_KIND_BOOL`, so the
  generic primitive conversion local-sync boundary added in the previous slice has a real
  bool scalar-local declaration path for `TO_BOOL` destinations. Source contracts now lock
  this conversion-kind proof alongside the `ConvertGenericTo*()` boundary sync markers ·
  RED/GREEN：RED 为 `zr_vm_aot_c_source_contracts_test` 缺少
  `case ZR_INSTRUCTION_OP_TO_BOOL:` 和 bool kind return；GREEN 为 source contracts
  19/0、aggregate shared-library smoke 8/0 · 测试结果：WSL GCC broader AOT focused group
  通过 call contracts 4/0、call shared-library smoke 5/0、shared-library smoke 8/0、
  value-type shared-library smoke 1/0、power contracts/smoke 2/0+1/0、source contracts
  19/0、generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical
  contracts/smoke 4/0+4/0、typed scalar 1/0、return contracts 1/0、frame setup
  contracts 1/0 · 产出：`backend_aot_c_scalar_locals.c`、
  `tests/parser/test_aot_c_source_contracts.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-to-bool-conversion-local-declaration.md` ·
  备注：`backend_aot_c_scalar_locals.c` remains oversized; this slice intentionally made the
  one-line proof correction in place because extracting scalar-local proof modules is a
  broader refactor and the file was already over threshold before this task.

- 2026-06-22 04:01:40 +08:00 · M1.5 / 07-S5 generic primitive conversion
  bool/i64/u64/f64 scalar-local sync boundary helper 切片 · 状态：子切片完成、07-S5
  部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：generic
  `TO_BOOL` / `TO_INT` / `TO_UINT` / `TO_FLOAT` boundary emitters now receive
  `const SZrAotExecIrFunction *functionIr` and share post-conversion scalar-local
  restoration. Generated C keeps the existing `zr_aot_convert_generic_to_*`
  runtime helper guards, then conditionally emits
  `zr_aot_convert_generic_sync_bool_local_boundary`,
  `zr_aot_convert_generic_sync_i64_local_boundary`,
  `zr_aot_convert_generic_sync_u64_local_boundary`, or
  `zr_aot_convert_generic_sync_f64_local_boundary` with
  `ZrLibrary_AotRuntime_SyncBoolLocal()` / `SyncSignedIntLocal()` /
  `SyncUnsignedIntLocal()` / `SyncFloatLocal()` when the destination slot is a
  matching scalar local. Function-body dispatch now passes `functionIr` for the
  four generic primitive conversion opcodes while typed numeric conversions remain on their direct
  scalar-local result-skip/cast implementation · RED/GREEN：RED 为
  `zr_vm_aot_c_source_contracts_test` 缺少 generic conversion scalar-local include/sync
  markers；GREEN 为 source contracts 19/0、aggregate shared-library smoke 8/0 ·
  测试结果：WSL GCC broader AOT focused group 通过 call contracts 4/0、call shared-library
  smoke 5/0、shared-library smoke 8/0、value-type shared-library smoke 1/0、power
  contracts/smoke 2/0+1/0、source contracts 19/0、generic numeric contracts/smoke
  1/0+1/0、global smoke 9/0、logical contracts/smoke 4/0+4/0、typed scalar 1/0、
  return contracts 1/0、frame setup contracts 1/0 · 产出：
  `backend_aot_c_lowering_generic_conversion.c`、`backend_aot_c_emitter.h`、
  `backend_aot_c_function_body.c`、`tests/parser/test_aot_c_source_contracts.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-conversion-local-sync-boundary-helper.md` ·
  备注：generic conversion local-sync coverage combines source-contract/codegen shape with
  the existing aggregate shared-library generic primitive conversion runtime smoke.

- 2026-06-22 03:50:11 +08:00 · M1.5 / 07-S5 generic power i64/u64/f64
  scalar-local sync boundary helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：generic `POW` boundary emitter now receives
  `const SZrAotExecIrFunction *functionIr` and restores matching i64/u64/f64 scalar
  locals after `ZrLibrary_AotRuntime_GenericPower(state, &frame, ...)`. Generated C keeps the
  existing `zr_aot_generic_power_boundary` runtime helper guard, then conditionally emits
  `zr_aot_generic_power_sync_i64_local_boundary`,
  `zr_aot_generic_power_sync_u64_local_boundary`, or
  `zr_aot_generic_power_sync_f64_local_boundary` with
  `ZrLibrary_AotRuntime_SyncSignedIntLocal()` /
  `SyncUnsignedIntLocal()` / `SyncFloatLocal()` when the destination slot is a matching
  scalar local. Function-body dispatch now passes `functionIr` for generic `POW` while
  typed `POW_SIGNED` / `POW_UNSIGNED` / `POW_FLOAT` paths remain on their direct
  scalar-local result-skip implementation · RED/GREEN：RED 为 `zr_vm_aot_c_power_contracts_test`
  缺少 generic power scalar-local include/sync markers；GREEN 为 power contracts 2/0、
  power shared-library smoke 1/0 · 测试结果：WSL GCC broader AOT focused group 通过 call
  contracts 4/0、call shared-library smoke 5/0、shared-library smoke 8/0、
  value-type shared-library smoke 1/0、power contracts/smoke 2/0+1/0、source contracts
  19/0、generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical
  contracts/smoke 4/0+4/0、typed scalar 1/0、return contracts 1/0、frame setup
  contracts 1/0 · 产出：`backend_aot_c_lowering_generic_power.c`、
  `backend_aot_c_emitter.h`、`backend_aot_c_function_body.c`、
  `tests/parser/test_aot_c_power_contracts.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-power-local-sync-boundary-helper.md` ·
  备注：generic power local-sync coverage is source-contract/codegen-level; current smoke
  still validates shared-library compileability for the helper boundary rather than a
  fully executable generic power scalar-local program.

- 2026-06-22 03:40:06 +08:00 · M1.5 / 07-S5 generic numeric i64/u64/f64
  scalar-local sync boundary helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07
  部分完成、08-12 未开始 · 完成项目：generic `ADD` / `SUB` / `MUL` / `DIV` /
  `MOD` / `NEG` boundary emitters now receive `const SZrAotExecIrFunction *functionIr`
  and share one helper for post-`ZrLibrary_AotRuntime_GenericNumeric*()` scalar-local
  restoration. Generated C keeps the existing `zr_aot_arith_exec_generic_numeric_*_boundary`
  helper guards, then conditionally emits `zr_aot_generic_numeric_sync_i64_local_boundary`,
  `zr_aot_generic_numeric_sync_u64_local_boundary`, or
  `zr_aot_generic_numeric_sync_f64_local_boundary` with
  `ZrLibrary_AotRuntime_SyncSignedIntLocal()` /
  `SyncUnsignedIntLocal()` / `SyncFloatLocal()` when the destination slot is a matching
  scalar local · 产出：`zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `tests/parser/test_aot_c_generic_numeric_contracts.c`、
  `tests/parser/test_aot_c_source_contracts.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-numeric-local-sync-boundary-helper.md` ·
  RED/GREEN：RED generic numeric contracts failed on missing `functionIr` and sync markers;
  GREEN focused generic numeric contracts 1/0, generic numeric shared-library smoke 1/0,
  and source contracts 19/0. A broad pass first exposed a stale source-contract expectation
  for the concrete helper-call string after generic numeric wrappers were collapsed through
  a shared `%s` helper; the contract was updated to lock the helperized call shape plus sync
  markers · 验证：broader WSL GCC 组通过 call contracts 4/0、call shared-library smoke 5/0、
  shared-library smoke 8/0、value-type shared-library smoke 1/0、power contracts/smoke
  2/0+1/0、source contracts 19/0、generic numeric contracts/smoke 1/0+1/0、global smoke
  9/0、logical contracts/smoke 4/0+4/0、typed scalar 1/0、return contracts 1/0、frame setup
  contracts 1/0 · 备注：i64/u64/f64 generic numeric local-sync coverage is locked at
  source-contract/codegen level; the existing generic numeric smoke covers shared-library
  compileability for the float `MOD` helper boundary. 07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 03:23:56 +08:00 · M1.5 / 07-S5 CallStackValue u64/f64 scalar-local sync
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic/direct `CallStackValue` lowering now threads `const SZrAotExecIrFunction *functionIr`
  from function-body dispatch into `backend_aot_write_c_direct_function_call()` /
  `backend_aot_write_c_dynamic_function_call()` and the shared core helper. Direct and dynamic
  paths can therefore emit guarded post-call scalar-local restoration for i64/bool/u64/f64 via
  `zr_aot_direct_function_call_sync_*_local_boundary` /
  `zr_aot_direct_dynamic_function_call_sync_*_local_boundary` markers and
  `ZrLibrary_AotRuntime_SyncSignedIntLocal()` / `SyncBoolLocal()` /
  `SyncUnsignedIntLocal()` / `SyncFloatLocal()` helper calls when the destination slot is a
  scalar local. The generated helper boundary keeps the old `CallStackValue` result copy as the
  single VM-frame write and rejects the old `zr_aot_direct_call_result` payload-read template for
  these restored locals · 产出：`zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_call_shared_library_smoke.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-call-stack-value-u64-f64-local-sync.md` ·
  RED/GREEN：RED call contracts failed on missing direct/dynamic u64/f64 `CallStackValue` local-sync
  markers and helper calls. Runtime smoke attempts with typed `fn()` return locals exposed the
  existing front-end inference boundary where unresolved callable returns are still typed as
  `object`, so callable-return typing was deferred out of this AOT codegen slice. GREEN focused
  call contracts 4/0 and call shared-library smoke 5/0 · 验证：broader WSL GCC 组通过 call
  contracts 4/0、call shared-library smoke 5/0、shared-library smoke 8/0、value-type
  shared-library smoke 1/0、power contracts/smoke 2/0+1/0、source contracts 19/0、generic
  numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical contracts/smoke 4/0+4/0、typed
  scalar 1/0、return contracts 1/0、frame setup contracts 1/0 · 备注：u64/f64 direct/dynamic
  `CallStackValue` coverage is locked at the generated-C contract level; executable smoke covers
  the currently expressible StackValue local-assignment path. 07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 03:03:12 +08:00 · M1.5 / 07-S5 static direct-call u64/f64 scalar-local sync
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  static direct call lowering 在 `ZrLibrary_AotRuntime_CallStaticDirect(...)` 后可为 u64/f64 destination
  scalar local 生成 `zr_aot_direct_static_function_call_sync_u64_local_boundary` /
  `zr_aot_direct_static_function_call_sync_f64_local_boundary` 与
  `ZrLibrary_AotRuntime_SyncUnsignedIntLocal()` / `ZrLibrary_AotRuntime_SyncFloatLocal()` helper guard，
  不再为这些结果恢复展开 `zr_aot_direct_call_result` payload 回读模板；scalar-local analysis 新增
  non-tail call result write 识别，并只在 stack-copy source 是 call-result destination 时，把 destination
  scalar kind 回传给 source temporary，避免 broad copy propagation 误伤 inline-struct/value-type 槽 · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、`tests/parser/test_aot_c_call_shared_library_smoke.c`、
  `tests/parser/test_aot_c_source_contracts.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-direct-call-u64-f64-local-sync.md` · RED/GREEN：
  RED 先由 call contracts 捕获缺少 u64/f64 direct-call sync 查询，随后由 call shared-library smoke 捕获
  call-result temporary 未从 typed-local copy 获得 u64/f64 标量声明；一次 broad bidirectional stack-copy
  传播尝试导致 value-type inline-struct 路径回退，收窄为 call-result-only propagation 后修复 · 验证：
  focused `zr_vm_aot_c_call_contracts_test` 4/0、`zr_vm_aot_c_call_shared_library_smoke_test` 4/0；
  broader WSL GCC 组通过 call contracts 4/0、call shared-library smoke 4/0、shared-library smoke 8/0、
  value-type shared-library smoke 1/0、power contracts/smoke 2/0+1/0、source contracts 19/0、
  generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical contracts/smoke 4/0+4/0、
  typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 · 备注：
  `backend_aot_c_scalar_locals.c` 已超大，本切片只做同一 proof boundary 的局部延伸；后续应抽出
  result-skip/liveness proof 模块。07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 02:28:40 +08:00 · M1.5 / 07-S5 typed power scalar-local result-skip 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `POW_SIGNED` / `POW_UNSIGNED` / `POW_FLOAT` 在 destination 可经 result-skip proof
  跳过 value-slot 写回、左右源槽已由匹配 scalar local 写入时，生成
  `zr_aot_arith_exec_*_power_scalar_local` 本地标量块，直接写 `zr_aot_sN` / `zr_aot_uN` /
  `zr_aot_fN`；未证明路径继续保留旧 frame/value fallback 与 `ZR_VALUE_FAST_SET`。scalar-local
  analysis 现在记录 power destination，并只为“实际作为 typed power operand 的 GET_CONSTANT
  destination”补 scalar-local declaration，避免 runtime boundary 仍需 value-slot 的常量被错误跳过 ·
  产出：`tests/parser/test_aot_c_power_contracts.c`、
  `tests/parser/test_aot_c_power_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_power.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-power-scalar-local-only.md` · RED/GREEN：
  RED 为 power contracts 缺少 scalar-local helper/include；GREEN 为 power contracts 2/0、
  power smoke 1/0；随后回归发现 broad immediate-constant declaration 会破坏 typed scalar/call
  shared-library 路径，收窄为 power operand-only 后 focused call shared-library smoke 3/0、
  typed scalar 1/0、power contracts 2/0、power smoke 1/0 全部通过 · 验证：
  broader focused group 通过 call contracts 4/0、call shared-library smoke 3/0、
  aggregate shared-library smoke 8/0、power contracts 2/0、power smoke 1/0、source contracts 19/0、
  generic numeric contracts/smoke 1/0+1/0、global smoke 9/0、logical contracts/smoke 4/0+4/0、
  typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 · 备注：07-S5 仍部分完成，
  08-12 未开始。

- 2026-06-22 01:51:29 +08:00 · M1.5 / 07-S5 string equality bool-result scalar-local path 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `LOGICAL_EQUAL_STRING` / `LOGICAL_NOT_EQUAL_STRING` 在 bool destination 可经 result-skip proof
  跳过 value-slot 写回时，生成 `zr_aot_string_logical_bool_scalar_local` 本地 bool 结果块，复用现有
  string operand validation 与 `memcmp` 比较，但直接写 `zr_aot_bN`，不再为该结果写
  `SZrTypeValue *zr_aot_destination` 或 `ZR_VALUE_FAST_SET`；未证明路径继续保留
  `zr_aot_string_logical_equal` / `zr_aot_string_logical_not_equal` fallback 与 destination value-slot 写回 · 产出：
  `tests/parser/test_aot_c_logical_contracts.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-string-equality-bool-local-only.md` ·
  RED/GREEN：logical contracts 先因缺少 `backend_aot_c_write_string_bool_scalar_local` 失败 1/4；
  实现后 logical contracts 4/0、logical shared-library smoke 4/0、source contracts 19/0、
  aggregate shared-library smoke 8/0；广域聚焦组拆分执行通过 call contracts 4/0、
  call shared-library smoke 3/0、aggregate shared-library smoke 8/0、power contracts 2/0、
  power shared-library smoke 1/0、source contracts 19/0、generic numeric contracts 1/0、
  generic numeric shared-library smoke 1/0、global shared-library smoke 9/0、logical contracts 4/0、
  logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 ·
  生成物检查：`string_equality_project/bin/aot_c/src/main.c` 的 string equality blocks 含
  `zr_aot_string_logical_bool_scalar_local`，并直接写 `zr_aot_b3`/`zr_aot_b4` 等 bool local；
  该局部结果路径未命中 `ZR_VALUE_FAST_SET` · 静态检查：限定本次触及文件的
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示；全仓 `git diff --check` 当前仍被既有
  `docs/plans/lsp/index.md:201 new blank line at EOF` 绊住 · 备注：string object/source 仍按现有
  frame-slot 读取；本切片只移除 bool 结果 value-slot 双写，07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 01:42:03 +08:00 · M1.5 / 07-S5 bool binary logical scalar-local opcode path 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `LOGICAL_AND` / `LOGICAL_OR` opcode lowering 在 destination 可经 bool result-skip proof 跳过
  value-slot 写回、左右源槽已由 bool local 写入时，生成 `zr_aot_bool_binary_scalar_local` 本地标量块，
  直接写 `zr_aot_bN = (TZrBool)((zr_aot_bL &&/|| zr_aot_bR) != 0u)`；未证明路径继续保留
  `zr_aot_bool_logical_and` / `zr_aot_bool_logical_or` fallback、bool tag check 与 `ZR_VALUE_FAST_SET`，
  并按 declared bool destination 记录 scalar-local 写入 · 产出：
  `tests/parser/test_aot_c_logical_contracts.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-bool-binary-logical-local-only.md` ·
  RED/GREEN：logical contracts 先因缺少 `backend_aot_c_write_bool_binary_scalar_local` 失败 1/4；
  实现后 logical contracts 4/0、logical shared-library smoke 4/0、source contracts 19/0、
  aggregate shared-library smoke 8/0；广域聚焦组拆分执行通过 call contracts 4/0、
  call shared-library smoke 3/0、aggregate shared-library smoke 8/0、power contracts 2/0、
  power shared-library smoke 1/0、source contracts 19/0、generic numeric contracts 1/0、
  generic numeric shared-library smoke 1/0、global shared-library smoke 9/0、logical contracts 4/0、
  logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；
  一条长命令串曾因 244s 工具超时中断，随后拆分重跑同一组测试通过 · 静态检查：
  generator 源中含 `zr_aot_bool_binary_scalar_local`、bool result-skip proof、左右 bool written-before proof，
  function-body 调用传入 `instructionIndex`，scalar-local analysis 覆盖 `LOGICAL_AND` / `LOGICAL_OR`；
  fallback `zr_aot_bool_logical_and` / `zr_aot_bool_logical_or` 和 `ZR_VALUE_FAST_SET` 保留 ·
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：源码 `&&`/`||` 通常先降为短路分支，
  本切片锁定 opcode 直降生成器形态；07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 01:24:18 +08:00 · M1.5 / 07-S5 typed bool logical scalar-local direct path 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  typed `LOGICAL_EQUAL_BOOL` / `LOGICAL_NOT_EQUAL_BOOL` / `LOGICAL_NOT_BOOL` 在 destination 可经
  bool result-skip proof 跳过 value-slot 写回、源槽已由 bool local 写入时，生成
  `zr_aot_bool_compare_scalar_local` / `zr_aot_bool_not_scalar_local` 本地标量块，直接写
  `zr_aot_bN`，不再生成 `SZrTypeValue *zr_aot_destination`、source `ZrCore_Stack_GetValue`、
  bool tag check 或 `ZR_VALUE_FAST_SET`；未证明路径继续保留 `zr_aot_bool_*_exec` fallback，
  并通过 declared bool destination 约束避免 generic conversion fallback 误用未声明 bool local · 产出：
  `tests/parser/test_aot_c_source_contracts.c`、
  `tests/parser/test_aot_c_logical_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_logical.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-typed-bool-logical-local-only.md` ·
  RED/GREEN：source contracts 先因缺少 `zr_aot_bool_compare_scalar_local` 失败 18/1；
  中间实现暴露 generic conversion fallback 会误声明 `zr_aot_b6`，随后加上 declared bool destination
  约束；最终 source contracts 19/0、logical shared-library smoke 4/0、aggregate shared-library smoke 8/0；
  广域聚焦组通过 call contracts 4/0、call shared-library smoke 3/0、aggregate shared-library smoke 8/0、
  power contracts 2/0、power shared-library smoke 1/0、source contracts 19/0、
  generic numeric contracts 1/0、generic numeric shared-library smoke 1/0、
  global shared-library smoke 9/0、logical contracts 4/0、logical shared-library smoke 4/0、
  typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 · 生成物检查：
  `bool_logical_project/bin/aot_c/src/main.c` 含 `zr_aot_bool_compare_scalar_local` 与
  `zr_aot_bool_not_scalar_local`，且 `zr_aot_bool_compare_exec` / `zr_aot_bool_not_exec` 无命中；
  `generic_conversion_project/bin/aot_c/src/main.c` 保留 `zr_aot_bool_not_exec` fallback 且无
  `zr_aot_b6` 误用 · `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:59:24 +08:00 · M1.5 / 07-S5 generic logical bool local sync boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic `LOGICAL_NOT` / `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL` helper 写回 bool destination 后，
  scalar-local 恢复不再展开 `const SZrTypeValue *zr_aot_bool_sync =
  ZrCore_Stack_GetValue(frame.slotBase + destinationSlot)`、bool tag 检查和 nativeBool payload 回读模板；
  `backend_aot_c_write_bool_local_sync_from_slot()` 现在只生成
  `zr_aot_generic_logical_sync_bool_local_boundary` marker 与
  `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, destinationSlot, &zr_aot_bN)` guard，
  复用 `aot_runtime_sync.c` 的 source-slot 校验与 no-op-on-type-mismatch 语义 · 产出：
  `tests/parser/test_aot_c_logical_contracts.c`、
  `tests/parser/test_aot_c_logical_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-logical-local-sync-boundary-helper.md` ·
  RED/GREEN：logical contracts 先因缺少
  `zr_aot_generic_logical_sync_bool_local_boundary` 失败 2/4；实现后 logical contracts 4/0、
  logical shared-library smoke 4/0；广域聚焦组通过 call contracts 4/0、call shared-library smoke 3/0、
  aggregate shared-library smoke 8/0、power contracts 2/0、power shared-library smoke 1/0、
  source contracts 19/0、generic numeric contracts 1/0、generic numeric shared-library smoke 1/0、
  global shared-library smoke 9/0、logical contracts 4/0、logical shared-library smoke 4/0、
  typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 · 生成物检查：
  `generic_equality_project/bin/aot_c/src/main.c` 含
  `zr_aot_generic_logical_sync_bool_local_boundary` 和
  `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, ...)`，
  `zr_aot_bool_sync` 无命中 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:48:45 +08:00 · M1.5 / 07-S5 COPY_STACK scalar-local sync boundary helpers + sync module split 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `SET_STACK` / `COPY_STACK` fallback helper 之后的 scalar-local 恢复不再展开
  `const SZrTypeValue *zr_aot_direct_stack_copy_sync_destination =
  ZrCore_Stack_GetValue(frame.slotBase + destinationSlot)`、tag 检查和 native payload 回读模板；
  bool/i64/u64/f64 本地同步分别只生成
  `zr_aot_direct_stack_copy_sync_bool_local_boundary` /
  `zr_aot_direct_stack_copy_sync_i64_local_boundary` /
  `zr_aot_direct_stack_copy_sync_u64_local_boundary` /
  `zr_aot_direct_stack_copy_sync_f64_local_boundary` marker 与
  `ZrLibrary_AotRuntime_SyncBoolLocal()` /
  `ZrLibrary_AotRuntime_SyncSignedIntLocal()` /
  `ZrLibrary_AotRuntime_SyncUnsignedIntLocal()` /
  `ZrLibrary_AotRuntime_SyncFloatLocal()` guard；local-sync helper 从 values runtime 拆入
  `aot_runtime_sync.c`，集中处理 runtime state/frame-slot/source-slot 校验、typed payload 回读、
  type mismatch no-op 兼容旧 generated sync 语义 · 产出：
  `tests/parser/test_aot_c_source_contracts.c`、`tests/parser/test_aot_c_call_contracts.c`、
  `tests/parser/test_aot_c_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_sync.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-copy-stack-local-sync-boundary-helpers.md` ·
  RED/GREEN：source/call/shared-library 合约先因缺少 `aot_runtime_sync.c` 和
  `zr_aot_direct_stack_copy_sync_*_local_boundary` helper-only 形态失败；实现后
  source contracts 19/0、aggregate shared-library smoke 8/0、call contracts 4/0；广域聚焦组通过
  call contracts 4/0、call shared-library smoke 3/0、aggregate shared-library smoke 8/0、
  power contracts 2/0、power shared-library smoke 1/0、source contracts 19/0、
  generic numeric contracts 1/0、generic numeric shared-library smoke 1/0、
  global shared-library smoke 9/0、logical shared-library smoke 4/0、typed scalar 1/0、
  return contracts 1/0、frame setup contracts 1/0 · 生成物检查：
  `numeric_arithmetic_project/bin/aot_c/src/main.c` 含
  `zr_aot_direct_stack_copy_sync_i64_local_boundary` 和
  `ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, ...)`，
  `zr_aot_direct_stack_copy_sync_destination` 无命中；`aot_runtime_values.c` 不再含 local-sync helper ·
  备注：`aot_runtime_values.c` 降至 877 行，新增 `aot_runtime_sync.c` 109 行；07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:33:42 +08:00 · M1.5 / 07-S5 static direct call scalar-local sync boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  static direct call lowering 在 `ZrLibrary_AotRuntime_CallStaticDirect(...)` 后不再展开
  `const SZrTypeValue *zr_aot_direct_call_result = ZrCore_Stack_GetValue(frame.slotBase + slot)`、
  tag 检查和 native payload 回读模板；需要恢复 scalar local 时只生成
  `zr_aot_direct_static_function_call_sync_i64_local_boundary` /
  `zr_aot_direct_static_function_call_sync_bool_local_boundary` marker 与
  `ZrLibrary_AotRuntime_SyncSignedIntLocal()` / `ZrLibrary_AotRuntime_SyncBoolLocal()` guard；
  runtime helper 集中处理 frame/source-slot 校验、signed-int 回读、bool 回读归一化，并保留旧 generated
  bool-sync 语义：source 非 bool 时不失败且不改写 bool local · 产出：
  `tests/parser/test_aot_c_call_contracts.c`、`tests/parser/test_aot_c_call_shared_library_smoke.c`、
  `tests/parser/test_aot_c_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-static-direct-call-local-sync-boundary-helper.md` ·
  RED/GREEN：call contracts 先因缺
  `zr_aot_direct_static_function_call_sync_i64_local_boundary` 失败；第一次广域回归暴露
  `SyncBoolLocal` 对非 bool static-direct 结果误报 `unsupported AOT local sync`，修正为 no-op 后
  call contracts 4/0、call shared-library smoke 3/0、aggregate shared-library smoke 8/0、
  power contracts 2/0、power shared-library smoke 1/0、source contracts 19/0、
  generic numeric contracts 1/0、generic numeric shared-library smoke 1/0、global shared-library smoke 9/0、
  logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0 ·
  生成物检查：`numeric_arithmetic_project/bin/aot_c/src/main.c` 含 `SyncSignedIntLocal` helper marker/call，
  `zr_aot_direct_call_result` 无命中 · 备注：`aot_runtime_values.c` 944 行，低于 1000 行但已接近拆分阈值；
  07-S5 仍部分完成，08-12 未开始。

- 2026-06-22 00:10:45 +08:00 · M1.5 / 07-S5 generic POW boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic `POW` C lowering 现在只生成 `zr_aot_generic_power_boundary` marker 与
  `ZrLibrary_AotRuntime_GenericPower(state, &frame, destinationSlot, leftSlot, rightSlot)` helper guard；
  runtime helper 负责 frame/slot 校验、`ZrCore_Value_GetMeta(state, leftValue, ZR_META_POW)` 查询、
  no-meta/null 目标写回、meta function 存在时的 `unsupported AOT generic power meta dispatch` 失败，
  生成器不再展开 `SZrMeta *zr_aot_meta`、destination/left/right `SZrTypeValue *` locals、
  direct `ZrCore_Value_GetMeta(state, zr_aot_left, ZR_META_POW)`、direct
  `ZrCore_Value_ResetAsNull(zr_aot_destination)` 或 generated debug-error 模板 · 产出：
  `tests/parser/test_aot_c_power_contracts.c`、`tests/parser/test_aot_c_power_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_power.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-power-boundary-helper.md` ·
  验证：RED 为 power contracts 缺少 `zr_aot_generic_power_boundary`；GREEN 为
  power contracts 2/0、power shared-library smoke 1/0、source contracts 19/0、generic numeric contracts 1/0、
  generic numeric shared-library smoke 1/0、shared-library smoke 8/0、call shared-library smoke 3/0、
  global shared-library smoke 9/0、logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；生成的 `aot_c_typed_power_smoke.c` 只含
  `zr_aot_generic_power_boundary` 与
  `ZrLibrary_AotRuntime_GenericPower(state, &frame, 11, 9, 10)`，旧 generated meta lookup/reset/debug
  模板无命中 · 备注：`aot_runtime_values.c` 增至 877 行，低于模块化阈值；构建仍只出现既有
  `project.c` const-qualifier warnings；07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 23:57:30 +08:00 · M1.5 / 07-S5 generic numeric boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  generic `ADD` / `SUB` / `MUL` / `DIV` / `MOD` / `NEG` C lowering 现在只生成
  `zr_aot_arith_exec_generic_numeric_binary_boundary` / `zr_aot_arith_exec_generic_numeric_unary_boundary`
  marker 与 `ZrLibrary_AotRuntime_GenericNumeric*()` helper guard；runtime helper 负责 frame/slot
  校验、numeric tag 分支、float/signed/unsigned 提取、`DIV`/`MOD` 零除失败、generic float `MOD`
  的 `fmod(leftFloat, rightFloat)` 路径、结果 `ZR_VALUE_FAST_SET` 写回和 unsupported primitive
  failure，生成器不再展开 `SZrTypeValue *zr_aot_destination/left/right` locals、tag-branch ladder、
  generated zero guard 或 direct arithmetic expression 模板 · 产出：
  `tests/parser/test_aot_c_source_contracts.c`、`tests/parser/test_aot_c_generic_numeric_contracts.c`、
  `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-numeric-boundary-helpers.md` ·
  验证：RED 为 source contracts 缺少 `zr_aot_arith_exec_generic_numeric_binary_boundary`；
  GREEN 为 source contracts 19/0、generic numeric contracts 1/0、generic numeric shared-library
  smoke 1/0、shared-library smoke 8/0、call shared-library smoke 3/0、global shared-library smoke 9/0、
  logical shared-library smoke 4/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；
  生成的 `aot_c_generic_numeric_mod_smoke.c` 只含 boundary marker 与
  `ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)`，旧 direct `fmod(zr_aot_left_float, ...)`、
  generated `modulo by zero` debug block 和 `ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)` 模板无命中 ·
  备注：`aot_runtime_values.c` 增至 843 行，低于模块化阈值；07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 23:39:10 +08:00 · M1.5 / 07-S5 GET_SUB_FUNCTION native closure boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  zero-capture `GET_SUB_FUNCTION` C fast path 现在只生成
  `zr_aot_value_get_sub_function_native_closure_boundary` marker 与
  `ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state, &frame, destinationSlot, childFunctionIndex, callableFlatIndex, zr_aot_fn_N)`
  helper guard；runtime helper 负责 frame/function/destination 校验、child metadata 查找、destination
  ownership release、native closure 分配初始化、generated thunk 绑定、AOT shim metadata 绑定和 native
  closure value flags，captured/unresolved/missing child materialization 路径仍保持显式 unsupported failure ·
  产出：`tests/parser/test_aot_c_constant_contracts.c`、
  `tests/parser/test_aot_c_global_shared_library_smoke.c`、
  `zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-get-sub-function-native-closure-boundary-helper.md` ·
  验证：RED 为 `zr_vm_aot_c_constant_contracts_test` 缺少
  `zr_aot_value_get_sub_function_native_closure_boundary` marker；GREEN 为 constant contracts 4/0、
  source contracts 19/0、shared-library smoke 8/0、call shared-library smoke 3/0、global shared-library
  smoke 9/0、typed scalar 1/0、return contracts 1/0、frame setup contracts 1/0；生成的
  `aot_c_get_sub_function_native_closure_smoke.c` 含 helper marker/call 和 `zr_aot_fn_1` thunk，
  旧 `zr_aot_value_exec_get_sub_function_native_closure` / direct `ZrCore_ClosureNative_New` /
  `nativeFunction = zr_aot_fn` 模板无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF warning ·
  备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 23:17:41 +08:00 · M1.5 / 07-S5 generic logical boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `LOGICAL_NOT` / `JUMP_IF` / `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL` generic primitive
  logical lowering 现在只保留 `zr_aot_generic_logical_*` marker、`ZR_AOT_C_GUARD(...)`
  和必要的 bool scalar-local 回读同步；primitive truthiness/equality 的
  null/bool/signed/unsigned/float 标签判断与 unsupported primitive failure 集中到
  `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy/LogicalNot/LogicalEqual/LogicalNotEqual()`
  dedicated runtime helpers，避免复用 broader `LogicalNot` / `IsTruthy` / `LogicalEqual`
  动态语义 helper · 产出：`tests/parser/test_aot_c_logical_contracts.c`、
  `tests/parser/test_aot_c_logical_shared_library_smoke.c`、`zr_vm_library/include/zr_vm_library/aot_runtime.h`、
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`、
  `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`、
  `tests/acceptance/2026-06-21-aot-m1-5-generic-logical-boundary-helpers.md` ·
  验证：RED 为 `zr_vm_aot_c_logical_contracts_test` 缺少新 GenericPrimitive helper 调用；
  GREEN 为 logical contracts 4/0、logical shared-library smoke 4/0、source contracts 19/0、
  shared-library smoke 8/0、call shared-library smoke 3/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；刷新后的 generic truthiness/equality generated C 含
  `ZrLibrary_AotRuntime_GenericPrimitive*` helper calls，旧 direct primitive truthiness/equality
  模板和 broader helper 调用无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF warning ·
  备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 22:52:02 +08:00 · M1.5 / 07-S5 generic primitive conversion boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `TO_BOOL` / `TO_INT` / `TO_UINT` / `TO_FLOAT` generic primitive conversion lowering 现在只保留
  `zr_aot_convert_generic_to_*` marker 与 `ZrLibrary_AotRuntime_ConvertGenericTo*()` helper guard，
  不再展开 generated `SZrTypeValue *zr_aot_destination/source` locals、`ZR_VALUE_IS_TYPE_*`
  分支、`ZR_VALUE_FAST_SET(...)` 或 generated unsupported-conversion block；runtime 侧新增独立
  `ConvertGenericTo*` helper，避免误用带 meta hook 语义的 `ZrLibrary_AotRuntime_To*()` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-generic-conversion-boundary-helpers.md` ·
  RED/GREEN：source contract 先因旧 direct conversion 模板缺 helper 失败；首次复用 `ToBool()`
  后 shared-library smoke 触发崩溃，`gdb` 定位到 `ToBool()` 进入 meta-conversion 路径，与旧
  primitive conversion 语义不等价；改为专用 `ConvertGenericTo*()` helpers 后 source contracts 19/0、
  aggregate shared-library smoke 8/0、call shared-library smoke 3/0、typed scalar 1/0、return contracts 1/0、
  frame setup contracts 1/0；generic-conversion generated fixture 含 `ConvertGenericToBool` helper call，
  旧 direct conversion 模板和 `To*()` helper 复用无命中 · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 22:24:19 +08:00 · M1.5 / 07-S5 publish exports boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  export-tail publication 现在只生成 `zr_aot_publish_exports_boundary` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PublishModuleExports(state, &frame));`，
  不再展开 `zr_aot_publish_exports_direct`、直接 `ZrCore_Value_Copy(...)`、closure capture publication、
  `ZrCore_Module_AddPubExport(state, frame.module, ...)` 或 `moduleExecuted` 直写模板；runtime helper 复用
  `aot_runtime_materialize_exports()` 并在 `frame.recordHandle` 已由 07-S3 elide 时按 `frame.function`
  回查 loaded-module record · 产出：`tests/acceptance/2026-06-21-aot-m1-5-publish-exports-boundary-helper.md` ·
  RED/GREEN：return contract 先因缺 `zr_aot_publish_exports_boundary` 失败；补 lowering 与 runtime
  record fallback 后 return contracts 1/0、source contracts 19/0、aggregate shared-library smoke 8/0、
  call shared-library smoke 3/0、frame setup contracts 1/0、typed scalar 1/0；generated shared/call `.c`
  fixtures 含 publish helper marker/call 且旧 direct export publication 模板无命中 · 备注：07-S5 仍部分完成，
  08-12 未开始。

- 2026-06-21 21:57:47 +08:00 · M1.5 / 07-S5 COPY_STACK boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `SET_STACK` fallback C lowering 现在通过 `ZrLibrary_AotRuntime_CopyStack()` helper 执行 frame-slot 校验、
  inline-struct copy、object-to-inline copy、value-slot copy 与 materialized stack-value assignment，生成器只保留
  `zr_aot_value_exec_copy_stack` marker、helper guard、以及必要的 scalar-local sync，不再展开
  `FindFrameSlotLayout` / `CopyFrameSlotInline` / `CopyObjectValueToFrameSlotInline` / `AssignMaterializedStackValue`
  模板；scalar-local sync 改为从 helper-owned destination slot 重新读取 `zr_aot_direct_stack_copy_sync_destination` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-copy-stack-boundary-helper.md` · RED/GREEN：
  source contract 先因缺 `zr_aot_value_exec_copy_stack` helper-only marker 失败；补 runtime helper 与 lowering 后
  source contracts 19/0、aggregate shared-library smoke 8/0、typed scalar 1/0、call shared-library smoke 3/0；
  generated shared/call `.c` fixtures 含 `ZrLibrary_AotRuntime_CopyStack(...)` 且旧 direct stack-copy / inline-struct
  copy / materialized-copy 模板无命中；CTest 过滤仅匹配 `aot_c_typed_scalar` 并通过 1/1；`git diff --check`
  退出 0，仅有既有 LF/CRLF warning · 备注：07-S5 仍部分完成，08-12 未开始。

- 2026-06-21 21:40:05 +08:00 · M1.5 / 07-S5 RESET_STACK_NULL/RESET_STACK_NULL2 boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `ZrLibrary_AotRuntime_ResetStackNull()` / `ZrLibrary_AotRuntime_ResetStackNull2()`，并把
  `backend_aot_write_c_direct_reset_stack_null()` / `backend_aot_write_c_direct_reset_stack_null2()`
  改为只发 `zr_aot_value_exec_reset_stack_null` / `zr_aot_value_exec_reset_stack_null2` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ResetStackNull/ResetStackNull2(state, &frame, ...))`；
  generated C 不再为 reset fallback 展开 destination/first/second `SZrTypeValue *` locals、
  frame slot 校验和 direct `ZrCore_Value_ResetAsNull(...)` 模板；scalar-local skip 仍保持 marker-only
  路径；reset helper 实现拆入新的 `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c`，
  避免继续膨胀 7k+ 行 `aot_runtime.c` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null-boundary-helpers.md` · RED/GREEN：
  source contract 先翻转到 helper-only 并在 helper source 缺失时失败；补 runtime helper 与 lowering 后
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_typed_scalar_test` 1/0、`zr_vm_aot_c_call_shared_library_smoke_test` 3/0；
  generated `.c` 反查确认 refreshed shared/call fixtures 含 `ResetStackNull*` helper calls，且无旧 reset2
  `SZrTypeValue *zr_aot_first` / `ZrCore_Value_ResetAsNull(zr_aot_first)` 模板；CTest 过滤仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1；`git diff --check` 退出 0，仅有既有 LF/CRLF warning · 备注：
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续
  07-S5+；08-12 未开始。

- 2026-06-21 21:19:49 +08:00 · M1.5 / 07-S5 closure value boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_get_closure_value()` / `backend_aot_write_c_set_closure_value()` 现在只发
  `zr_aot_value_exec_get_closure_value` / `zr_aot_value_exec_set_closure_value` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetClosureValue/SetClosureValue(state, &frame, slot, closureIndex))`；
  generated C 不再展开 current-closure stack lookup、native/VM closure decode、capture get/set、direct copy
  或 setter barrier 模板；既有 runtime helpers 继续集中处理 active closure capture 解析、frame slot 校验、
  value copy/barrier 与失败上报 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-closure-value-boundary-helpers.md` · RED/GREEN：
  constant source contract 先翻转到 helper-only 并因缺
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetClosureValue(state, &frame, %u, %u))` 失败；改 C lowering 后
  `zr_vm_aot_c_constant_contracts_test` 4/0、`zr_vm_aot_c_global_shared_library_smoke_test` 8/0；generated
  closure fixture 检查确认四个 `ZrLibrary_AotRuntime_GetClosureValue/SetClosureValue(state, &frame, ..., 0)`
  helper calls 存在，旧 direct closure/capture/barrier 模板无命中；更宽 focused WSL 组通过 source contracts
  19/0、constant contracts 4/0、global smoke 8/0、aggregate shared-library smoke 8/0、return contracts 1/0、
  value SemIR contracts 4/0、control contracts 1/0；`ctest -R 'aot_c_constant|aot_c_global'` exit 0 但本 build
  无注册匹配，focused binaries 已直接运行；`git diff --check` exit 0，仅有既存 LF/CRLF warning · 备注：
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续
  07-S5+；08-12 未开始。

- 2026-06-21 21:09:08 +08:00 · M1.5 / 07-S5 CREATE_OBJECT/CREATE_ARRAY boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_create_object()` / `backend_aot_write_c_direct_create_array()` 现在只发
  `zr_aot_value_exec_create_object` / `zr_aot_value_exec_create_array` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateObject/CreateArray(state, &frame, destinationSlot))`；generated C 不再展开
  object/array allocation、destination `SZrTypeValue` lookup、ownership release、raw-object init、array type tag 或 null-reset
  模板；既有 runtime helpers 继续集中处理 destination slot 校验、对象/数组创建、旧值释放、结果写入与失败上报 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-object-array-creation-boundary-helpers.md` · RED/GREEN：
  global source contract 先翻转到 helper-only 并因缺 `zr_aot_value_exec_create_object` marker 失败；改 C lowering 后
  `zr_vm_aot_c_global_contracts_test` 7/0、`zr_vm_aot_c_global_shared_library_smoke_test` 8/0；generated
  object-array fixture 检查确认 `ZrLibrary_AotRuntime_CreateObject(state, &frame, 0)` 与
  `ZrLibrary_AotRuntime_CreateArray(state, &frame, 1)` helper calls 存在，旧 direct object/array creation 模板无命中；更宽
  focused WSL 组通过 source contracts 19/0、global contracts 7/0、global smoke 8/0、aggregate shared-library smoke 8/0、
  return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；`ctest -R 'aot_c_global'` exit 0 但本 build
  无注册匹配，focused binaries 已直接运行；`git diff --check` exit 0，仅有既存 LF/CRLF warning · 备注：
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续
  07-S5+；08-12 未开始。

- 2026-06-21 20:54:14 +08:00 · M1.5 / 07-S5 TO_STRING boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_to_string()` 现在只发 `zr_aot_value_exec_to_string` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, destinationSlot, sourceSlot))`；generated C 不再展开
  source/destination `SZrTypeValue` lookup、direct `ZrCore_Value_ConvertToString`、call-info/frame refresh、raw-object
  string init 或 destination null-reset 模板；既有 runtime helper 继续集中处理 frame slot 校验、conversion、frame refresh、
  destination relookup、string/null 结果写入与失败上报 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-to-string-boundary-helper.md` · RED/GREEN：
  global source contract 先翻转到 helper-only 并因缺
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, %u, %u))` 失败；改 C lowering 后
  `zr_vm_aot_c_global_contracts_test` 6/0、`zr_vm_aot_c_global_shared_library_smoke_test` 7/0；generated
  to-string fixture 检查确认 `ZrLibrary_AotRuntime_ToString(state, &frame, 1, 0)` helper call 存在，旧 direct string
  conversion 模板无命中；更宽 focused WSL 组通过 source contracts 19/0、global contracts 6/0、global smoke 7/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；`ctest -R 'aot_c_global'`
  exit 0 但本 build 无注册匹配，focused binaries 已直接运行；`git diff --check` exit 0，仅有既存 LF/CRLF warning · 备注：
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续
  07-S5+；08-12 未开始。

- 2026-06-21 20:42:05 +08:00 · M1.5 / 07-S5 TO_OBJECT/TO_STRUCT boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_to_object()` / `backend_aot_write_c_direct_to_struct()` 现在只发
  `zr_aot_value_exec_to_object` / `zr_aot_value_exec_to_struct` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToObject/ToStruct(state, &frame, destinationSlot, sourceSlot, typeNameConstantIndex))`；
  generated C 不再展开 destination/source `SZrTypeValue` lookup、type-name constant lookup 或 direct
  `ZrCore_Execution_ToObject/ToStruct(state, frame.callInfo, ...)` 模板；既有 runtime helper 继续集中处理
  frame slot 校验、type-name constant 校验、source/destination value lookup、core conversion call 与失败上报 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-object-struct-boundary-helpers.md` · RED/GREEN：
  global source contract 先翻转到 helper-only 并因缺
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToObject(state, &frame, %u, %u, %u))` 失败；改 C lowering 后
  `zr_vm_aot_c_global_contracts_test` 6/0、`zr_vm_aot_c_global_shared_library_smoke_test` 7/0；generated
  object/struct fixture 检查确认 `ZrLibrary_AotRuntime_ToObject(state, &frame, 1, 0, 0)` 与
  `ZrLibrary_AotRuntime_ToStruct(state, &frame, 2, 1, 0)` helper calls 存在，旧 direct core conversion 模板无命中；
  更宽 focused WSL 组通过 source contracts 19/0、global contracts 6/0、global smoke 7/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control contracts 1/0；
  `ctest -R 'aot_c_global'` 退出 0 但当前构建无注册匹配测试；`git diff --check` 退出 0，仅有既有 LF/CRLF 提示 · 备注：
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续
  07-S5+；08-12 未开始。

- 2026-06-21 20:22:27 +08:00 · M1.5 / 07-S5 TYPEOF boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_typeof()` 现在只发 `zr_aot_value_exec_typeof` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_TypeOf(state, &frame, destinationSlot, sourceSlot))`；
  generated C 不再展开 destination/source `SZrTypeValue` lookup 或 direct
  `ZrCore_Reflection_TypeOfValue(state, zr_aot_source, zr_aot_destination)` 模板；既有 runtime helper
  继续集中处理 frame slot 校验、source/destination value lookup、reflection call 与失败上报 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-typeof-boundary-helper.md` · RED/GREEN：
  global source contract 先翻转到 TYPEOF helper-only 并因缺
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_TypeOf(state, &frame, %u, %u))` 失败；改 C lowering 后
  `zr_vm_aot_c_global_contracts_test` 6/0、`zr_vm_aot_c_global_shared_library_smoke_test` 7/0；
  generated typeof fixture 检查确认 `ZrLibrary_AotRuntime_TypeOf(state, &frame, 1, 0)` helper call
  存在，旧 direct reflection 模板无命中；更宽 focused WSL 组通过 source contracts 19/0、
  global contracts 6/0、global smoke 7/0、aggregate shared-library smoke 8/0、return contracts
  1/0、value SemIR contracts 4/0、control contracts 1/0；`ctest -R 'aot_c_global'` 退出 0
  但当前构建无注册匹配测试；`git diff --check` 退出 0，仅有既有 LF/CRLF 提示 · 备注：
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge
  与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 20:13:10 +08:00 · M1.5 / 07-S5 GET_GLOBAL boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_get_global()` 现在只发 `zr_aot_value_exec_get_global`
  marker 与 `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetGlobal(state, &frame, destinationSlot))`；
  generated C 不再展开 `SZrTypeValue` destination lookup、`zr_aot_global_object`、
  `state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT`、direct global `ZrCore_Value_Copy`
  或 destination null-reset 模板；既有 runtime helper 继续集中处理 frame slot 校验、
  destination value lookup、global object copy 与 null fallback · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-get-global-boundary-helper.md` · RED/GREEN：
  global source contract 先翻转到 helper-only 并因缺 `zr_aot_value_exec_get_global` 失败；
  改 C lowering 后 `zr_vm_aot_c_global_contracts_test` 6/0、
  `zr_vm_aot_c_global_shared_library_smoke_test` 7/0；generated get-global fixture 检查确认
  `ZrLibrary_AotRuntime_GetGlobal(state, &frame, 0)` helper call 存在，旧 direct global
  value-copy 模板无命中；更宽 focused WSL 组通过 source contracts 19/0、global contracts
  6/0、global smoke 7/0、aggregate shared-library smoke 8/0、return contracts 1/0、value
  SemIR contracts 4/0、control contracts 1/0；`ctest -R 'aot_c_global'` 退出 0 但当前构建无注册匹配测试；
  `git diff --check` 退出 0，仅有既有 LF/CRLF 提示 · 备注：07-S5 仍部分完成；typed-to-typed
  native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 19:55:10 +08:00 · M1.5 / 07-S5 iterator boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_iter_init/move_next/current()` 现在只发既有 iterator
  marker 与 `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Iter*(state, &frame, ...))`；
  `backend_aot_write_c_direct_iter_move_next_jump_if_false()` 现在发
  `ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(..., &zr_aot_branch_taken)`，由 helper
  执行 move-next、destination bool 校验与 branch decision，generated C 只保留 `goto`
  分支；generated C 不再展开 `SZrTypeValue` slot lookup、cached iterator fast paths、
  `ZrCore_Object_IterInit/IterMoveNext/IterCurrent()` 或 iterator-specific unsupported
  failure 模板 · 产出：`tests/acceptance/2026-06-21-aot-m1-5-iterator-boundary-helpers.md` ·
  RED/GREEN：iterator source contract 先翻转到 helper-only 并因缺
  `ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(` 失败；补 runtime helper 与 C lowering 后
  `zr_vm_aot_c_iterator_contracts_test` 1/0、
  `zr_vm_aot_c_iterator_shared_library_smoke_test` 1/0；generated iterator fixture 检查确认
  `IterInit/IterMoveNext/IterCurrent/IterMoveNextJumpIfFalse` helper calls 与 expected
  branch `goto` 存在，旧 iterator fast/core 展开模板无命中；更宽 focused WSL 组通过
  source contracts 19/0、iterator contracts 1/0、iterator smoke 1/0、aggregate
  shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、control
  contracts 1/0；`ctest -R 'aot_c_iterator'` 退出 0 但当前构建无注册匹配测试 ·
  备注：07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、
  deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 19:35:05 +08:00 · M1.5 / 07-S5 super-array integer boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_super_array_get_int/set_int/add_int/add_int4/add_int4_const/fill_int4_const()`
  现在只发既有 super-array marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArray*(state, &frame, ...))`；
  generated C 不再展开 `SZrTypeValue` slot lookup、fast-path applicability、constant
  extraction、`ZrCore_Object_SuperArrayTry*Fast()`、
  `ZrCore_Object_SuperArrayAddInt*AssumeFast()`、`FillInt4ConstAssumeFast()` 或
  `unsupported AOT super-array integer fast path` 模板；`SuperArrayAddInt()` 的
  `ZR_INSTRUCTION_USE_RET_FLAG` discard-result 语义继续由 runtime helper 处理 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-super-array-boundary-helpers.md` ·
  RED/GREEN：super-array source contract 先翻转到 helper-only 并因缺
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayGetInt(state, &frame, %u, %u, %u))`
  失败；改 C lowering 后 `zr_vm_aot_c_super_array_contracts_test` 1/0、
  `zr_vm_aot_c_super_array_shared_library_smoke_test` 1/0；generated super-array
  fixture 检查确认 `GetInt/SetInt/AddInt/AddInt4/AddInt4Const/FillInt4Const`
  helper calls 存在，含 `ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, 65535, 0, 1)`
  discard-result case，旧 direct super-array fast/core 展开模板无命中；更宽 focused
  WSL 组通过 source contracts 19/0、super-array contracts 1/0、super-array smoke 1/0、
  aggregate shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、
  control contracts 1/0；`ctest -R 'aot_c_super_array'` 退出 0 但当前构建无注册匹配测试 ·
  备注：首个 post-fix 构建命令在外层超时后 WSL 进程仍继续并完成，随后直接运行两个二进制 GREEN；
  07-S5 仍部分完成；typed-to-typed native signature routing、in/out 写回、deopt/dynamic
  bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 19:14:29 +08:00 · M1.5 / 07-S5 scope lifecycle boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_mark_to_be_closed()` 现在只发
  `zr_aot_scope_mark_to_be_closed` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, slotIndex))`；
  `backend_aot_write_c_direct_close_scope()` 现在只发
  `zr_aot_scope_close_scope` marker 与
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CloseScope(state, &frame, cleanupCount))`；
  generated C 不再展开 to-be-closed slot lookup、`ZrCore_Closure_ToBeClosedValueClosureNew`、
  close-scope stackTop save/load、`ZrCore_Closure_CloseStackValue` 或
  `ZrCore_Closure_CloseRegisteredValues` 模板，scope lifecycle 语义集中到既有
  `ZrLibrary_AotRuntime_MarkToBeClosed/CloseScope()` runtime helpers · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-scope-boundary-helpers.md` · RED/GREEN：
  scope source contract 先翻转到 helper-only 并因缺
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, %u))`
  失败；改 C lowering 后 `zr_vm_aot_c_scope_contracts_test` 1/0、
  `zr_vm_aot_c_scope_shared_library_smoke_test` 1/0；generated scope fixture 检查确认
  `ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, 1)` 和
  `ZrLibrary_AotRuntime_CloseScope(state, &frame, 1)` 存在，旧 closure/stack 展开模板无命中；
  更宽 focused WSL 组通过 source contracts 19/0、scope contracts 1/0、scope smoke 1/0、
  aggregate shared-library smoke 8/0、return contracts 1/0、value SemIR contracts 4/0、
  control contracts 1/0；`ctest -R 'aot_c_scope'` 退出 0 但当前构建无注册匹配测试；
  `git diff --check` 退出 0，仅有既有 LF/CRLF 提示 · 备注：07-S5 仍部分完成；
  typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；
  08-12 未开始。

- 2026-06-21 18:50:41 +08:00 · M1.5 / 07-S5 ownership boundary helpers 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_own_unique/borrow/loan/return_loan/share/weak/detach/upgrade/release()`
  现在统一通过 `backend_aot_write_c_direct_ownership_helper_call()` 发射
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Own*(state, &frame, destinationSlot, sourceSlot))`；
  generated C 不再展开 ownership destination/source slot lookup，也不再直接调用
  `ZrCore_Ownership_*Value(...)`。runtime helper 继续复用既有 frame/slot/value 校验与 core
  ownership 语义 · 产出：`tests/acceptance/2026-06-21-aot-m1-5-ownership-boundary-helpers.md` ·
  RED/GREEN：ownership contract 先翻转到 helper-only ownership 契约并因缺少
  `backend_aot_write_c_direct_ownership_helper_call(` 失败；补通用 helper emitter 与 smoke
  断言后 ownership contract / ownership shared-library smoke 均 GREEN · 测试结果：
  WSL focused 组通过 source contracts 19/0、ownership contracts 1/0、
  ownership shared-library smoke 1/0、aggregate shared-library smoke 8/0、return contracts 1/0、
  value SemIR contracts 4/0；generated `aot_c_ownership_smoke.c` 含 9 个
  `ZrLibrary_AotRuntime_Own*` helper 调用，且旧 `zr_aot_value_exec_ownership_core` /
  `zr_aot_value_exec_ownership_release` / direct `ZrCore_Ownership_*Value` generated 模板无命中；
  `git diff --check` 退出 0，仅有既有 LF/CRLF 提示 · 备注： broader source-contract
  重跑时同步修正了一个既有 scalar-stack-copy 文本断言漂移；07-S5 仍部分完成；
  typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；
  08-12 未开始。

- 2026-06-21 18:29:21 +08:00 · M1.5 / 07-S5 OWN_RETURN_LOAN boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_own_return_loan()` 现在只发 `zr_aot_value_exec_ownership_return_loan`
  marker 与 `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_OwnReturnLoan(state, &frame, destinationSlot, sourceSlot))`；
  `ZrLibrary_AotRuntime_OwnReturnLoan()` 作为公开 AOT runtime helper 复用
  `aot_runtime_own_value()` 做 frame/slot/value 校验并调用 `ZrCore_Ownership_ReturnLoanValue`。
  其他 ownership lowering 在本切片保持既有 direct-core 形态 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-own-return-loan-boundary-helper.md` · RED/GREEN：
  ownership contract 先翻转到 helper-only return-loan 契约并因缺少 runtime helper 声明失败；
  补 helper 与 C lowering 后 ownership contract / ownership shared-library smoke 均 GREEN ·
  测试结果：WSL focused 组通过 source contracts 19/0、ownership contracts 1/0、
  ownership shared-library smoke 1/0、aggregate shared-library smoke 8/0、return contracts 1/0、
  value SemIR contracts 4/0；generated `aot_c_ownership_smoke.c` 含
  `ZrLibrary_AotRuntime_OwnReturnLoan(state, &frame, 1, 3)`，保留 Unique/Upgrade/Release
  direct-core 形态，且旧 `ZrCore_Ownership_ReturnLoanValue(state, zr_aot_destination, zr_aot_source)`
  展开模板无命中；`git diff --check` 退出 0，仅有既有 LF/CRLF 提示 · 备注：07-S5 仍部分完成；typed-to-typed native signature routing、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 18:14:46 +08:00 · M1.5 / 07-S5 frame-backed direct-return boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_return()` 现在只发既有 `zr_aot_direct_return` marker 与
  `ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_Return(state, &frame, sourceSlot, ZR_FALSE))`；
  `ZrLibrary_AotRuntime_Return()` 补齐并集中承接旧 generated direct-return 展开的 source/caller
  value 取址、exception-handler discard、functionTop stack guard、return escape、closure close、
  inline constructor receiver copy-back、constructor result-copy skip、caller result copy 与最终
  stackTop reset；export tail return 仍先走 direct publication 再以 `ZR_FALSE` 调用 return helper，
  scalar i64 local return 仍走 `ZrLibrary_AotRuntime_ReturnI64()` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-direct-return-boundary-helper.md` · RED/GREEN：
  return contract 先翻转到 helper-owned frame-backed return 语义并暴露 helper 缺少
  `callerResultValue` / constructor handling / inline-constructor receiver copy-back；实现后
  return contracts、source contracts 与 aggregate shared-library smoke 均 GREEN · 测试结果：
  WSL focused 组通过 source contracts 19/0、aggregate shared-library smoke 8/0、return contracts 1/0、
  frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、
  call shared-library smoke 3/0、control contracts 1/0、control shared-library smoke 1/0、
  global contracts 6/0、global shared-library smoke 7/0；generated `aot_c_pending_control_direct.c`
  含 `ZrLibrary_AotRuntime_Return(state, &frame, 0, ZR_FALSE)`，无旧 direct-return
  result/caller locals 与 generated cleanup/copy-back 展开模板；`git diff --check` 退出 0，
  仅有既有 LF/CRLF 提示 · 备注：07-S5 仍部分完成；typed-to-typed native signature routing、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 17:51:42 +08:00 · M1.5 / 07-S5 pending-control boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_set_pending_return()` / `backend_aot_write_c_set_pending_break()` /
  `backend_aot_write_c_set_pending_continue()` 现在只发既有 `zr_aot_pending_*` marker、
  初始化 `zr_aot_next_instruction`、调用
  `ZrLibrary_AotRuntime_SetPendingReturn/Break/Continue(..., &zr_aot_next_instruction)`，
  并在 helper 给出 resume instruction 时跳回 generated dispatch；旧 generated C 中的
  `SZrTypeValue *zr_aot_pending_value`、`execution_set_pending_control()`、
  `execution_resume_pending_via_outer_finally()`、`execution_jump_to_instruction_offset()`、
  `state->pendingControl.targetInstructionOffset` 与手写 resume-index 计算从生成物中移除，
  由既有 runtime helper 集中承接 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-pending-control-boundary-helper.md` · RED/GREEN：
  source contract 先翻转到 helper-only pending-control 契约并暴露旧生成器仍非 helper 形态；
  shared-library smoke 同步改为要求三类 `SetPending*` helper 并禁止旧展开模板；生成器切换后
  source contracts 与 aggregate shared-library smoke 均 GREEN · 测试结果：WSL focused 组通过
  source contracts 19/0、aggregate shared-library smoke 8/0、return contracts 1/0、
  frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、
  call shared-library smoke 3/0、control contracts 1/0、control shared-library smoke 1/0、
  global contracts 6/0、global shared-library smoke 7/0；generated
  `aot_c_pending_control_direct.c` 含三条
  `ZrLibrary_AotRuntime_SetPendingReturn/Break/Continue(...)` helper guard 与
  `goto zr_aot_fn_0_dispatch;`，无旧 pending value local、pending-control set/resume/jump
  展开模板 · 备注：07-S5 仍部分完成；typed-to-typed native signature routing、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 17:26:50 +08:00 · M1.5 / 07-S5 END_FINALLY boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_end_finally()` 现在只发 `zr_aot_end_finally_direct` marker、
  初始化 `zr_aot_next_instruction`、调用
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_EndFinally(state, &frame, handlerIndex, &zr_aot_next_instruction))`
  并在 helper 给出 resume instruction 时跳回 generated dispatch；旧 generated C 中的
  pending-control `switch`、`resumeCallInfo` / `handlerState` / `targetSlot` locals、
  pending return value copy、exception unwind、frame refresh 与 resume-index 计算搬进
  `ZrLibrary_AotRuntime_EndFinally()`；未捕获 pending exception 由 helper 继续调用
  `ZrCore_Exception_Throw()` 保留旧语义，替代后的未用
  `aot_runtime_resume_exception_in_current_frame()` 已删除 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-end-finally-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 END_FINALLY helper guard/resume dispatch 并禁止旧 generated
  switch/local 模板；runtime source contract 再锁住 helper 内未捕获异常传播；生成器与 runtime
  helper 补齐后 GREEN，control shared-library smoke 同步切到 helper-only END_FINALLY 断言后 PASS ·
  测试结果：WSL focused 组通过 source contracts 19/0、return contracts 1/0、frame setup
  contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、
  call shared-library smoke 3/0、control contracts 1/0、control shared-library smoke 1/0、
  global contracts 6/0、global shared-library smoke 7/0；generated control smoke C 含
  `ZrLibrary_AotRuntime_EndFinally(state, &frame, 0, &zr_aot_next_instruction)` 与
  resume-dispatch guard，无旧 `switch (state->pendingControl.kind)` / `resumeCallInfo` /
  `handlerState` / `targetSlot` / pending return-value copy 模板；CTest 过滤仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成；pending control、
  typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板
  仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 16:59:30 +08:00 · M1.5 / 07-S5 THROW boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_throw()` 现在只发 `zr_aot_throw_direct` marker、初始化
  `zr_aot_next_instruction`、调用
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Throw(state, &frame, sourceSlot, &zr_aot_next_instruction))`
  并在 helper 给出 resume instruction 时跳回 generated dispatch；旧 generated C 中的
  payload slot 校验、`SZrTypeValue` payload/source locals、pending-control clear、
  exception normalize、unwind、frame refresh 与 resume-index 计算搬进
  `ZrLibrary_AotRuntime_Throw()`；未捕获异常传播由 helper 调用
  `ZrCore_Exception_Throw()` 保留旧语义 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-throw-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 THROW helper guard/resume dispatch 并禁止旧 normalize/unwind
  generated 模板；runtime source contract 同步锁住 helper 内未捕获异常传播；生成器与 runtime
  helper 补齐后 GREEN；control shared-library smoke 同步切到 helper-only THROW 断言后 PASS ·
  测试结果：WSL focused 组通过 source contracts 19/0、return contracts 1/0、frame setup
  contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、
  call shared-library smoke 3/0、control contracts 1/0、control shared-library smoke 1/0、
  global contracts 6/0、global shared-library smoke 7/0；generated control smoke C 含
  `ZrLibrary_AotRuntime_Throw(state, ...)` 与 resume-dispatch guard，无旧
  `ZrCore_Exception_NormalizeThrownValue(state, ...)` THROW 模板；CTest 过滤仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1 · 备注：07-S5 仍部分完成；END_FINALLY、pending control、
  typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板
  仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 16:44:43 +08:00 · M1.5 / 07-S5 CATCH boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_catch()` 现在只发 `zr_aot_catch_direct` marker 加
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Catch(state, &frame, destinationSlot))`；旧
  generated C 中的 destination slot 校验、`SZrTypeValue *zr_aot_destination`、
  current-exception copy/clear、null reset 与 pending-control cleanup 搬进既有
  `ZrLibrary_AotRuntime_Catch()` helper · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-catch-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 CATCH helper guard 并禁止旧 exception-copy/reset generated
  模板，生成器切到 helper 后 GREEN；control shared-library smoke 同步切到 helper-only
  CATCH 断言后 PASS · 测试结果：WSL focused 组通过 source contracts 19/0、return
  contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、
  call contracts 4/0、call shared-library smoke 3/0、control contracts 1/0、
  control shared-library smoke 1/0、global contracts 6/0、global shared-library smoke 7/0；
  generated control smoke C 含 `ZrLibrary_AotRuntime_Catch(state, ...)`，无旧
  `ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException)` /
  `ZrCore_Exception_ClearCurrent(state)` / `ZrCore_Value_ResetAsNull(zr_aot_destination)`
  CATCH 模板；CTest 过滤仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1 · 备注：
  07-S5 仍部分完成；THROW、END_FINALLY、pending control、typed-to-typed native signature
  routing、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；
  08-12 未开始。

- 2026-06-21 16:30:20 +08:00 · M1.5 / 07-S5 END_TRY boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_end_try()` 现在只发 `zr_aot_end_try_direct` marker 加
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_EndTry(state, &frame, handlerIndex))`；旧 generated
  C 中的 `SZrCallInfo` 恢复、handler lookup、finally phase mutation、handler pop、
  END_TRY-specific `RunError` 与 frame/call-info 同步搬进既有
  `ZrLibrary_AotRuntime_EndTry()` 成功路径 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-end-try-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 END_TRY helper guard 并禁止旧 handler-state/finally-phase
  generated 模板，生成器与 runtime helper 同步后 GREEN；control shared-library smoke
  同步切到 helper-only END_TRY 断言后 PASS · 测试结果：WSL focused 组通过 source
  contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、
  control contracts 1/0、control shared-library smoke 1/0、global contracts 6/0、
  global shared-library smoke 7/0；generated control smoke C 含
  `ZrLibrary_AotRuntime_EndTry(state, ...)`，无旧
  `handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY` END_TRY 模板 · 备注：
  07-S5 仍部分完成；这是 END_TRY 边界模板集中化，不是最终异常控制全 helper 化；THROW、
  CATCH、END_FINALLY、pending control、typed-to-typed native signature routing、in/out
  写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 16:20:39 +08:00 · M1.5 / 07-S5 TRY boundary helper 切片 ·
  状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_try()` 现在只发 `zr_aot_try_direct` marker 加
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Try(state, &frame, handlerIndex))`；旧 generated C
  中的 `SZrCallInfo` 恢复、handler-index 检查、`execution_push_exception_handler()`、
  TRY-specific `RunError` 与 frame/call-info 同步搬进既有
  `ZrLibrary_AotRuntime_Try()` 成功路径 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-try-boundary-helper.md` · RED/GREEN：
  control contracts 先要求 TRY helper guard 并禁止旧 `execution_push_exception_handler`
  generated 模板，生成器与 runtime helper 同步后 GREEN；control shared-library smoke
  同步切到 helper-only TRY 断言后 PASS · 测试结果：WSL focused 组通过 source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR
  contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、control contracts
  1/0、control shared-library smoke 1/0、global contracts 6/0、global shared-library
  smoke 7/0；CTest 过滤
  `aot_c_(typed_scalar|call_shared_library|control_shared_library|global_(contracts|shared_library_smoke))`
  仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1；generated control smoke C 含
  `ZrLibrary_AotRuntime_Try(state, ...)`，无旧
  `execution_push_exception_handler(state, zr_aot_call_info, ...)` TRY 模板 · 备注：
  07-S5 仍部分完成；这是 TRY 边界模板集中化，不是最终异常控制全 helper 化；END_TRY、
  THROW、CATCH、END_FINALLY、pending control、typed-to-typed native signature routing、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 16:08:08 +08:00 · M1.5 / 07-S5 unsupported instruction boundary
  helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`backend_aot_write_c_unsupported_instruction_expr()` 现在复用既有
  `ZrLibrary_AotRuntime_ReportUnsupportedInstruction()`，生成的 unsupported instruction
  块只保留 `zr_aot_unsupported_instruction` marker 后通过
  `ZR_AOT_C_RETURN(...)` 进入统一 cleanup exit；不再展开 instruction/opcode locals、
  hand-written `ZrCore_Debug_RunError(state, "unsupported AOT instruction")` 或
  `ZR_AOT_C_FAIL()` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-instruction-boundary-helper.md` ·
  RED/GREEN：source contracts 先要求 cleanup-exit helper 调用并禁止旧 inline failure
  模板，生成器改写后 GREEN；shared-library smoke 的 unsupported instruction boundary
  子项同步从旧 generated-C failure 文本切到 helper cleanup-exit 断言后 PASS · 测试结果：
  WSL focused 组通过 source contracts 19/0、return contracts 1/0、frame setup contracts
  1/0、typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、call
  shared-library smoke 3/0、global contracts 6/0、global shared-library smoke 7/0；
  CTest 过滤
  `aot_c_(typed_scalar|call_shared_library|global_(contracts|shared_library_smoke))`
  仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1；generated unsupported-instruction
  boundary C 含三处 `ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, ...)`
  cleanup-exit 调用，无旧 instruction/opcode locals 或 unsupported-instruction
  `RunError` 模板 · 备注：当前完整 `zr_vm_aot_c_shared_library_smoke_test` 在 dirty checkout
  中仍有 numeric arithmetic 与 generic primitive conversion 两个执行类失败，unsupported
  instruction boundary 子项已 PASS；07-S5 仍部分完成；这是 unsupported instruction
  边界模板集中化，不是最终 deopt/dynamic bridge；typed-to-typed native signature routing、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 15:41:20 +08:00 · M1.5 / 07-S5 unsupported dynamic value access boundary
  helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess()` 新增为当前
  `GET_MEMBER`/`GET_MEMBER_SLOT`/`GET_BY_INDEX`/`SET_MEMBER`/`SET_MEMBER_SLOT`/
  `SET_BY_INDEX` 的 unsupported dynamic member/index VM 边界 helper，承接旧
  `backend_aot_write_c_unsupported_dynamic_value_access()` 在 generated C 内联的
  primary/secondary slot 检查、`SZrTypeValue *` 槽访问、operand-index local 与
  dynamic-specific failure；生成器现在只保留
  `zr_aot_value_unsupported_dynamic_value_access` marker 后发一条 helper guard · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-dynamic-value-access-boundary-helper.md` ·
  RED/GREEN：global contracts 先要求 runtime header 中的 `UnsupportedDynamicValueAccess`
  并禁止旧 dynamic value access inline failure 模板，补 helper 与 lowering 后 GREEN；
  global shared-library smoke 随后因仍期待旧 generated-C failure 文本失败，更新为
  helper-only 生成物断言后 GREEN · 测试结果：WSL focused 组通过 source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR
  contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、global contracts
  6/0、global shared-library smoke 7/0；CTest 过滤
  `aot_c_(typed_scalar|call_shared_library|global_(contracts|shared_library_smoke))`
  仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1；generated dynamic value access boundary C
  含六处 `ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess` guard，无旧 opcode local、
  primary/secondary `SZrTypeValue *` locals、operand-index local 或
  `RunError(state, "unsupported AOT dynamic value access: %s")` 模板 · 备注：07-S5 仍部分
  完成；这是 unsupported dynamic member/index access 边界模板集中化，不是最终 dynamic value
  执行；typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge、
  real dynamic value access execution helpers 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 15:25:37 +08:00 · M1.5 / 07-S5 unsupported meta value access boundary
  helper 切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 ·
  完成项目：`ZrLibrary_AotRuntime_UnsupportedMetaValueAccess()` 新增为当前
  `META_GET`/`META_SET`/super meta cached/static cached value access 的 unsupported
  VM 边界 helper，承接旧 `backend_aot_write_c_unsupported_meta_value_access()` 在
  generated C 内联的 primary/secondary slot 检查、`SZrTypeValue *` 槽访问、
  member/cache index local 与 meta-specific failure；生成器现在只保留
  `zr_aot_value_unsupported_meta_value_access` marker 后发一条 helper guard · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-meta-value-access-boundary-helper.md` ·
  RED/GREEN：global contracts 先要求 runtime header 中的 `UnsupportedMetaValueAccess`
  并禁止旧 meta value access inline failure 模板，补 helper 与 lowering 后 GREEN；
  global shared-library smoke 随后因仍期待旧 generated-C failure 文本失败，更新为
  helper-only 生成物断言后 GREEN · 测试结果：WSL focused 组通过 source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR
  contracts 4/0、call contracts 4/0、call shared-library smoke 3/0、global contracts
  6/0、global shared-library smoke 7/0；CTest 过滤
  `aot_c_(typed_scalar|call_shared_library|global_(contracts|shared_library_smoke))`
  仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1；generated meta value access boundary C
  含六处 `ZrLibrary_AotRuntime_UnsupportedMetaValueAccess` guard，无旧 opcode local、
  primary/secondary `SZrTypeValue *` locals、member/cache local 或
  `RunError(state, "unsupported AOT meta value access: %s")` 模板 · 备注：07-S5 仍部分
  完成；这是 unsupported meta value access 边界模板集中化，不是最终 meta value 执行；
  typed-to-typed native signature routing、in/out 写回、deopt/dynamic bridge、dynamic
  value access helperization 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 15:06:55 +08:00 · M1.5 / 07-S5 unsupported meta call boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `ZrLibrary_AotRuntime_UnsupportedMetaCall()` 新增为当前 unsupported meta-call VM
  边界模板，承接旧 `backend_aot_write_c_unsupported_meta_call()` 在 generated C 内联的
  destination/receiver/argument slot 检查、`SZrTypeValue *` 槽访问和 unsupported meta-call
  failure；`backend_aot_write_c_unsupported_meta_call()` 现在只保留
  `zr_aot_unsupported_meta_call` marker 后发一条 helper guard；同时修复
  `backend_aot_c_scalar_locals_record_exec_instruction_write()` 的 stack-copy scalar kind
  继承证明，让 `floatCopy` 后的 f64 `22 <- 40` 继续 local-only，恢复 focused typed-scalar
  frame-free 合约 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-unsupported-meta-call-boundary-helper.md` ·
  RED/GREEN：call contracts 中 meta-call 用例先要求 `UnsupportedMetaCall` 并禁止旧 generated-C
  receiver/destination/argument inline 模板；实现 helper 与生成器改写后 GREEN；broader validation
  暴露 typed-scalar frame setup 回归，补 stack-copy proof 后 GREEN · 测试结果：WSL focused 组通过
  source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、
  value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤
  `aot_c_typed_scalar|aot_c_call_shared_library` 仅匹配已注册 `aot_c_typed_scalar` 并通过
  1/1；generated meta-call boundary C 含三处 `ZrLibrary_AotRuntime_UnsupportedMetaCall`，
  无旧 argument/receiver/destination locals 或 `RunError(state, "unsupported AOT meta call")`；
  typed-scalar generated C 无 `zr_aot_generated_frame_setup`，保留 local-only
  `zr_aot_scalar_stack_copy_f64 dstSlot=22 srcSlot=40` · 备注：07-S5 仍部分完成；这是 unsupported
  meta-call 边界模板集中化，不是最终 meta-call 执行；typed-to-typed native signature routing、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 14:41:47 +08:00 · M1.5 / 07-S5 call stack value boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `ZrLibrary_AotRuntime_CallStackValue()` 新增为 generic/dynamic stack-value function-call
  VM 边界模板，承接旧 `backend_aot_write_c_core_function_call()` 在 generated C 内联的
  call-base/destination anchor、`ZrCore_Function_CallAndRestoreAnchor`、结果槽复制与 caller
  frame 恢复；`backend_aot_write_c_core_function_call()` 现在只保留
  `zr_aot_direct_function_call` / `zr_aot_direct_dynamic_function_call` marker 后发一条 helper
  guard · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-call-stack-value-boundary-helper.md` ·
  RED/GREEN：call contracts 中 dynamic/generic 两个用例先要求 `CallStackValue` 并禁止旧
  stack-anchor inline 模板；runtime helper 与生成器改写后 GREEN，call shared-library smoke 继续执行
  `apply(addFour, 3)` AOT C 路径 · 测试结果：WSL focused 组通过 source contracts 19/0、
  return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts
  4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤
  `aot_c_typed_scalar|aot_c_call_shared_library` 仅匹配已注册 `aot_c_typed_scalar` 并通过
  1/1；generated call-smoke C 含 `ZrLibrary_AotRuntime_CallStackValue(state, ...)`，
  旧 `CallAndRestoreAnchor`/`SZrFunctionStackAnchor zr_aot_call_anchor`/destination anchor restore
  均无命中 · 备注：07-S5 仍部分完成；这是 VM 边界模板集中化，不是最终 typed-to-typed
  C signature ABI；typed-to-typed 直参直返、in/out 写回、deopt/dynamic bridge 与剩余边界模板
  仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 14:25:45 +08:00 · M1.5 / 07-S5 static direct call boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `ZrLibrary_AotRuntime_CallStaticDirect()` 新增为 static direct-call VM 边界模板，承接
  当前静态直调的 call-frame 准备、VALUE 参数源槽物化、callee thunk 调用、`PostCall` 与
  caller frame 恢复；`backend_aot_write_c_static_direct_function_call()` 现在保留
  `zr_aot_direct_static_function_call` marker 后只发一条 helper guard，并传入 destination slot、
  function slot、argument count、callee index 与 callee thunk，不再在 generated C 展开
  `SZrCallInfo`、`SZrFunction` callable metadata、`PreCallPrepared...`、参数源物化循环或
  `PostCall` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-static-direct-call-boundary-helper.md` ·
  RED/GREEN：call contracts 先要求 `CallStaticDirect` 并禁止旧 static direct-call inline
  prepared-call 模板；runtime helper 与生成器改写后 GREEN，call shared-library smoke 继续执行
  `apply(addFour, 3)` AOT C 路径 · 测试结果：WSL focused 组通过 source contracts 19/0、
  return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts
  4/0、call contracts 4/0、call shared-library smoke 3/0；CTest 过滤
  `aot_c_typed_scalar|aot_c_call_shared_library` 仅匹配已注册 `aot_c_typed_scalar` 并通过
  1/1；generated static direct-call 片段只含
  `ZrLibrary_AotRuntime_CallStaticDirect(state, ...)`，旧 `PreCallPrepared...`、
  `zr_aot_materialize_argument_source_slot`、`PostCall(state, zr_aot_call_info, 1)` 均无命中 ·
  备注：07-S5 仍部分完成；这是 VM 边界模板集中化，不是最终 typed-to-typed C signature ABI；
  typed-to-typed 直参直返、in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；
  08-12 未开始。

- 2026-06-21 14:12:31 +08:00 · M1.5 / 07-S5 call inline struct boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `ZrLibrary_AotRuntime_CallInlineStruct()` 新增为 value SemIR typed inline-struct call
  边界模板，承接当前 VM call-frame 准备、VALUE 参数源槽物化、callee thunk 调用、`PostCall`
  与 caller frame 恢复；`backend_aot_try_write_c_value_semir_call_typed_exec()` 现在只生成
  一条 helper guard，并传入 destination/callee/argument/layout/thunk 信息，不再在 generated C
  内联 `SZrCallInfo`、call-base 清零循环、callable `SZrTypeValue`、`PreCallPrepared...`
  或 `PostCall` 模板 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-call-inline-struct-boundary-helper.md` ·
  RED/GREEN：value SemIR contracts 先要求 `CallInlineStruct` 并禁止旧 prepared-call 模板；
  source contracts 随后暴露仍期待旧边界 internals，更新为 helper-only 合同后 GREEN · 测试结果：
  WSL focused 组通过 source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0、value SemIR contracts 4/0、call contracts 4/0、call shared-library smoke
  3/0；CTest 过滤 `aot_c_typed_scalar|aot_c_call_shared_library` 仅匹配已注册
  `aot_c_typed_scalar` 并通过 1/1；generated C typed-call 片段只含
  `ZrLibrary_AotRuntime_CallInlineStruct(state, ...)`，旧 `PreCallPrepared...`、
  `zr_aot_materialize_argument_source_slot`、`SZrFunction *zr_aot_metadata_function;` 和
  `ZrCore_Function_PostCall(state, zr_aot_call_info, 1)` 均无命中 · 备注：07-S5 仍部分完成；
  这次是边界模板集中化，不是最终 typed-to-typed C signature ABI；typed-to-typed 直参直返、
  in/out 写回、deopt/dynamic bridge 与剩余边界模板仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 13:52:19 +08:00 · M1.5 / 07-S5 inline struct return boundary helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `ZrLibrary_AotRuntime_ReturnInlineStruct()` 进入 dedicated return boundary 模块，
  value SemIR typed inline-struct return 现在只发 helper guard + `ZR_AOT_C_RETURN(1)`，
  不再在 generated C 内联 `SZrCallInfo`、`state->stackTop.valuePointer = zr_aot_return_source + 1`
  或直接写 `zr_aot_skip_drop_slot = sourceSlot`；typed-call 与 static direct-call 在进入
  `PreCallPreparedResolvedVmFunctionWithArgumentSource` 前物化 VALUE 参数源槽，修复 scalar
  local 与 value-frame/dense slot 不同步导致的动态/静态调用回归；scalar stack-copy 快速路径新增
  source written-before / parameter / explicit static source guard，避免把 struct/object 临时槽提前当
  i64 读取；direct stack-copy fallback 在 inline-struct source/destination type layout 一致时改走
  `ZrCore_Function_CopyFrameSlotInline()`，再 fallback 到 object-value conversion · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-inline-struct-return-boundary-helper.md` ·
  RED/GREEN：return/value SemIR/source/call contracts 先要求 `ReturnInlineStruct`、参数源物化、
  标量 stack-copy eligibility guard 与 inline-frame fallback；shared-library smoke 先在 value typed
  call fixture 暴露 `dstSlot=3 srcSlot=2` 被误降为 i64，再暴露 inline-struct fallback 误走 object
  conversion，补 guard 与 `CopyFrameSlotInline` 后 GREEN · 测试结果：focused 命令通过
  return contracts 1/0、value SemIR contracts 4/0、source contracts 19/0、call contracts 4/0、
  call shared-library smoke 3/0；broader focused WSL 组通过 source contracts 19/0、return
  contracts 1/0、frame setup contracts 1/0、typed scalar 1/0、value SemIR contracts 4/0、
  call contracts 4/0、call shared-library smoke 3/0；CTest 过滤
  `aot_c_typed_scalar|aot_c_call_shared_library` 仅匹配已注册 `aot_c_typed_scalar` 并通过 1/1；
  generated C grep 显示 `ReturnInlineStruct`、`zr_aot_materialize_argument_source_slot` 和
  `ZrCore_Function_CopyFrameSlotInline`，旧 inline return stackTop/skip-drop 与误降的
  `zr_aot_scalar_stack_copy_i64 dstSlot=3 srcSlot=2` 均无命中 · 备注：07-S5 仍部分完成；
  typed-to-typed 签名路由、in/out 写回、deopt/dynamic bridge 模板和剩余 value-frame fallback
  仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 12:20:30 +08:00 · M1.5 / 07-S5 return boundary module split
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  将 `ZrLibrary_AotRuntime_ReturnI64()` 从 7k+ 行的 `aot_runtime.c` 拆入
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c`，并新增
  `aot_runtime_internal.h` 作为 runtime 私有窄接口；该内部接口只暴露 opaque
  `SZrLibraryAotRuntimeState`、`aot_runtime_get_state_from_global()` 和
  `aot_runtime_fail()`，让 return marshaling 模板脱离 loader/export/call 等大文件责任。
  `aot_runtime.c` 保留旧的 frame-backed `ZrLibrary_AotRuntime_Return()` 与共享 runtime state/error
  实现，不再包含 `ReturnI64` 实现体 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-return-boundary-module-split.md` ·
  RED/GREEN：return contract 先因缺 `aot_runtime/aot_runtime_internal.h` 与
  `aot_runtime/aot_runtime_return.c` 失败；补私有头、拆出 ReturnI64 实现、把 shared helper
  从 file-local static 改成 runtime 内部链接后 GREEN · 测试结果：source contracts 19/0、
  return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：focused generated C 对 entry setup、
  frame/context/marker、direct return 内联 `SZrCallInfo`/caller `SZrTypeValue`、closure/ownership
  维护和 `frame.` 均无命中；return grep 仍只显示 `ReturnI64(state, zr_aot_s23/s48)` 后
  `ZR_AOT_C_RETURN(1)`。备注：07-S5 仍部分完成；此切片只完成 i64 return boundary 的模块边界，
  typed→typed 直返、入参解包、in/out 写回、deopt/dynamic 边界和 value-frame fallback 仍待后续 07-S5+。

- 2026-06-21 12:08:43 +08:00 · M1.5 / 07-S5 i64 return boundary runtime helper
  切片 · 状态：子切片完成、07-S5 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `ZrLibrary_AotRuntime_ReturnI64(state, value)` 作为 native→VM 的 i64 返回打包模板；
  `backend_aot_write_c_direct_return_i64_local()` 现在只发
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s%u));` 和
  `ZR_AOT_C_RETURN(1)`，不再在 generated C 函数体内声明 `SZrCallInfo`、构造
  `SZrTypeValue *zr_aot_caller_result_value`、丢弃 exception handlers、关闭 closure、copy-back
  constructor receiver、释放旧 ownership payload 或手写 result slot 字段。runtime helper 仍集中执行这些
  VM 边界动作，并用 `ZrCore_Value_InitAsInt()` 写回调用方结果 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-i64-return-boundary-helper.md` ·
  RED/GREEN：return source contract 先因缺 `ZrLibrary_AotRuntime_ReturnI64` 失败；typed-scalar
  generated-product 同步禁止 direct return block 内联 `SZrCallInfo`、`SZrTypeValue`、closure/ownership
  维护，要求只保留 `ReturnI64` 边界调用；补 runtime helper、header/API、return emitter 后 GREEN ·
  测试结果：source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar
  1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 · 检查：focused generated C
  grep 对 `zr_aot_generated_frame_setup`、frame/context/stack/marker、`SZrCallInfo`、direct
  `SZrTypeValue` caller result、closure/ownership 维护均无命中；return block grep 只显示
  `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s23/s48)` 后 `ZR_AOT_C_RETURN(1)`。备注：07-S5
  仍部分完成；此切片只模板化 focused i64 native→VM return boundary，typed→typed 直返、入参解包、
  in/out 写回、deopt/dynamic 边界和 value-frame fallback 仍待后续 07-S5+；08-12 未开始。

- 2026-06-21 11:59:37 +08:00 · M1.5 / 07-S4 descriptor-free pure scalar empty entry setup
  切片 · 状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 现在在 `!includeStackFrameSetup` 时于任何 generated-C
  `fprintf` 前直接返回；descriptor-free zero-byte pure scalar 函数入口不再输出
  `/* zr_aot_generated_frame_setup */` 或 function-scope `zr_aot_call_info`。`callInfo` 获取被
  移到 `backend_aot_write_c_direct_return_i64_local()` 的返回打包边界，该模板局部声明
  `SZrCallInfo *zr_aot_call_info = ZR_NULL;` 并在返回前读取 `state->callInfoList` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-empty-pure-scalar-entry-setup.md` ·
  RED/GREEN：typed-scalar generated-product 先因新增禁止
  `/* zr_aot_generated_frame_setup */` 而失败；补 early return before emit、direct-return
  boundary-local callInfo 与 source contract 钉住顺序后 GREEN · 测试结果：source contracts
  19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配
  已注册的 `aot_c_typed_scalar` 并通过 1/1 · 检查：focused generated C grep 对
  `zr_aot_generated_frame_setup`、frame/context/stack/marker 禁止项无命中；prologue grep 显示
  `static TZrInt64 zr_aot_fn_0` 后直接进入 `zr_aot_scalar_locals_begin`，`SZrCallInfo` 只出现在
  `zr_aot_direct_return_i64_local` block 内。备注：07-S4 仍部分完成；此切片只清空纯标量 entry
  setup，返回打包仍是 VM 边界 marshaling，后续 07-S5+ 继续收敛 typed→typed 直返与 SZrValue
  边界模板；08-12 未开始。

- 2026-06-21 11:38:18 +08:00 · M1.5 / 07-S4 descriptor-free pure scalar minimal prologue
  切片 · 状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  descriptor-free 且 zero-byte-frame 的纯标量 generated C frame setup 现在只发
  `SZrCallInfo *zr_aot_call_info = state->callInfoList;` 后直接进入 scalar locals；
  不再生成 `ZrAotGeneratedModuleContext zr_aot_context`、`ResolveGeneratedModuleContext`、
  stack frame setup locals、`ZrCore_Function_CheckStackAndGc`、native-frame null 填充循环或
  `ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted` public/generated helper 调用。
  `zr_vm_library` 新增私有 `aot_runtime_mark_record_executed()`，在 AOT record-entry 与
  full context/shim 边界统一保留 `executedVia` 标记，不把执行模式标记塞回纯标量函数体 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-pure-scalar-minimal-prologue.md` ·
  RED/GREEN：frame-setup source contract 先因缺 `TZrBool includeStackFrameSetup` 和
  `if (!includeStackFrameSetup) {` 失败；typed-scalar generated-product 同步禁止 full
  context/stack setup 字符串。第一版省略 context 后暴露功能回归，`executedVia` 从
  `ZR_LIBRARY_EXECUTED_VIA_AOT_C` 退为 `NONE`；补临时 generated marker 后 GREEN，随后契约继续
  收紧为禁止 public/generated marker，并把标记迁移到 runtime record-entry boundary 后 GREEN · 测试结果：
  source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、typed scalar 1/0；
  CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 · 检查：focused generated C
  grep 对 frame/context/stack/marker 禁止项无命中；prologue grep 显示 `zr_aot_generated_frame_setup`
  后只有 `zr_aot_call_info` local，下一行即 `zr_aot_scalar_locals_begin`；无
  `ZrAotGeneratedFrame frame`、无 `frame.*`、无 `ResolveGeneratedModuleContext`。
  备注：07-S4 仍部分完成；此切片只压缩 descriptor-free pure scalar entry prologue。return
  boundary、call/cleanup/export/value-frame fallback 和后续 SZrValue 边界模板仍按后续 07-S5+
  收敛；08-12 未开始。

- 2026-06-21 11:21:56 +08:00 · M1.5 / 07-S4 frame descriptor elision 切片 ·
  状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  新增 `backend_aot_c_frame_descriptor.*`，把 generated C function body 是否仍需
  `ZrAotGeneratedFrame frame` 的保守证明从超大 `backend_aot_c_function_body.c`
  中拆出；`backend_aot_write_c_function_body()` 只在
  `backend_aot_c_function_body_needs_frame_descriptor()` 判定需要时声明
  `ZrAotGeneratedFrame frame = {0};`，并把 `includeFrameDescriptor` 传给
  `backend_aot_write_c_frame_setup()`；frame setup 只在该标志为真时生成
  `frame.function`、`frame.callInfo`、`frame.slotBase`、
  `frame.generatedFrameSlotCount` 四个 descriptor assignment。证明当前只接受已知
  frame-free 的 scalar SemIR、可跳过 value-slot 写入的 immediate scalar constants、
  scalar-local-only stack copy、直接跳转/标量 local branch predicate 和 scalar-local i64
  return；exports、cleanup、exception handlers、缺失 ExecIR、未知 opcode 或仍会发
  `frame.` 的 fallback 均保守保留 descriptor · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-descriptor-elision.md` ·
  RED/GREEN：frame-setup source contract 先因缺 `TZrBool includeFrameDescriptor`
  失败；typed-scalar generated-product 同步新增 `ZrAotGeneratedFrame frame = {0};`
  与 4 个 descriptor setup assignment 的禁止断言。补 descriptor proof、conditional
  frame declaration/setup、scalar SemIR frame-free probe 和 stricter stack-copy
  local-only predicate 后 GREEN · 测试结果：source contracts 19/0、return contracts
  1/0、frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：focused typed scalar generated C grep 只剩
  `.registerFrameBytes = 0u,` 与 `value SemIR lowering frameByteSize=0`；`grep -o
  'frame\.[A-Za-z0-9_]*'` 无匹配；scoped `git diff --check` 退出 0，仅报告既有
  CRLF/LF 提示 · 备注：07-S4 尚未完成；本切片只在 body 证明完全 frame-free 时省略
  descriptor。带 exports、cleanup、exceptions、inline value frames、未知 opcode 或
  frame-backed fallback 的函数仍保留 descriptor path。08-12 未开始。

- 2026-06-21 10:56:36 +08:00 · M1.5 / 07-S4 frame byte-slot prologue elision 切片 ·
  状态：子切片完成、07-S4 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 新增
  `backend_aot_c_frame_setup_register_frame_bytes()`，setup-time byte-frame size 现在只统计
  `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT` 且拥有有效 type-layout/positive byte size 的 frame
  slots；当计算结果为 0 时不再生成 `TZrSize zr_aot_frame_byte_size`、
  `zr_aot_frame_byte_slot_count` 或 `zr_aot_frame_byte_size = (TZrSize)0u;`。focused pure scalar
  生成物的 byte-slot prologue 已收窄为零；`backend_aot_c_value_semir_register_frame_bytes()`
  同步把 value SemIR generated comment 从 raw frame layout 收窄为 inline-struct-only
  `frameByteSize=0` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-byte-slot-prologue-elision.md` ·
  RED/GREEN：frame-setup source contract 先暴露 contract/implementation drift（缺
  `backend_aot_c_frame_setup_register_frame_bytes(` 和 inline-struct filter），随后新增
  `if (frameByteSize > 0u) {` 条件发射契约；typed-scalar generated-product 同步新增旧
  byte-frame local/zero assignment 禁止断言；source-contract 再要求 value SemIR summary 使用
  `valueFrameBytes` 而非 `frameLayout->frameByteSize`。补 inline-struct-only helper、
  条件发射与 value SemIR summary 后 GREEN ·
  测试结果：source contracts 19/0、return contracts 1/0、frame setup contracts 1/0、
  typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1 ·
  检查：generated C grep 确认 focused typed scalar 无 `zr_aot_frame_byte_size` /
  `zr_aot_frame_byte_slot_count`，且 value SemIR comment 为 `frameByteSize=0`；剩余
  `frame.*` setup 字段仍为 `frame.function`、
  `frame.callInfo`、`frame.slotBase`、`frame.generatedFrameSlotCount` · 备注：07-S4 尚未完成；
  本切片只移除 pure scalar byte-frame 零值 prologue，`ZrAotGeneratedFrame frame` 与 4 个
  frame setup 字段仍待后续边界替换。08-12 未开始。

- 2026-06-21 10:26:35 +08:00 · M1.5 / 07-S3 skip-drop slot cleanup gating 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_function_body()` 新增 `needsSkipDropSlot`，并将其保守绑定到
  `needsFrameCleanup`；只有 inline-struct cleanup 可能发射时才声明
  `TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;`。focused pure scalar
  生成物不再含 `zr_aot_skip_drop_slot` 或 `zr_aot_frame_started`，剩余 `frame.*` setup 字段仍为
  `frame.function`、`frame.callInfo`、`frame.slotBase`、`frame.generatedFrameSlotCount` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-skip-drop-slot-cleanup-gating.md` · RED/GREEN：
  frame-setup source contract 先因缺 `TZrBool needsSkipDropSlot`、`needsSkipDropSlot = needsFrameCleanup;`
  和 `if (needsSkipDropSlot) {` 失败；typed-scalar generated-product 同步新增旧 skip-drop
  declaration 禁止断言。补条件发射后 GREEN · 测试结果：source contracts 19/0、return contracts 1/0、
  frame setup contracts 1/0、typed scalar 1/0；CTest 过滤套件仅匹配已注册的 `aot_c_typed_scalar`
  并通过 1/1 · 检查：generated C grep 确认 focused typed scalar 无 `zr_aot_skip_drop_slot`/
  `zr_aot_frame_started`；相关代码文件 scoped `git diff --check` 退出 0，仅报告既有 CRLF/LF
  提示 · 备注：07-S3 尚未完成；本切片只移除 pure scalar 不会被读取的 skip-drop local，
  `ZrAotGeneratedFrame frame` 与 4 个 frame setup 字段仍待后续边界替换。08-12 未开始。

- 2026-06-21 10:17:48 +08:00 · M1.5 / 07-S3 empty frame cleanup guard elision 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_frame_cleanup_would_emit()` 公开 frame cleanup emitter 的 inline-struct/drop
  判定；`backend_aot_write_c_function_body()` 只在 cleanup 确实可能发射时声明
  `zr_aot_frame_started`、setup 后置 true 赋值以及 exit guard。focused pure scalar 生成物不再含
  空的 `if (zr_aot_frame_started) { }` cleanup guard · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-empty-frame-cleanup-guard-elision.md` ·
  RED/GREEN：frame-setup source contract 先因缺 `TZrBool needsFrameCleanup`、
  `backend_aot_c_frame_cleanup_would_emit(` 和 `if (needsFrameCleanup) {` 失败；
  typed-scalar generated-product 先因旧 `zr_aot_frame_started` declaration/assignment/guard
  失败；补 cleanup predicate 与条件发射后 GREEN · 测试结果：
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：generated C grep 确认 focused typed scalar 无
  `zr_aot_frame_started`；剩余 `frame.*` 仍为 `callInfo`、`function`、
  `generatedFrameSlotCount` 和 `slotBase`；相关代码文件 scoped `git diff --check`
  退出 0，仅报告既有 CRLF/LF 提示 · 备注：07-S3 尚未完成；本切片只删除纯标量空 cleanup
  guard，`ZrAotGeneratedFrame frame` 与 4 个 frame setup 字段仍待后续边界替换。08-12 未开始。

- 2026-06-21 10:09:03 +08:00 · M1.5 / 07-S3 MethodInfo registerFrameBytes narrowing 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_c_emitter.c` 新增
  `backend_aot_c_method_info_register_frame_bytes()`，MethodInfo 的
  `.registerFrameBytes` 现在只统计 `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT`
  且拥有有效 `typeLayoutId`/`byteSize` 的 frame slot 末端；纯标量 C local 不再计入
  MethodInfo byte-frame requirement。focused pure scalar 生成物从
  `.registerFrameBytes = 6272u,` 收窄为 `.registerFrameBytes = 0u,` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-method-info-register-frame-bytes-narrowing.md` ·
  RED/GREEN：frame-setup source contract 先因缺
  `backend_aot_c_method_info_register_frame_bytes(` 与 inline-struct slot filter 失败；
  typed-scalar generated-product 先因缺 `.registerFrameBytes = 0u,` 且仍有旧
  `6272u` descriptor 失败；补 MethodInfo byte-frame 计算后 GREEN · 测试结果：
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：generated C grep 确认
  `registerFrameBytes = 0u`；focused generated C 剩余 `frame.*` 仍为 `callInfo`、
  `function`、`generatedFrameSlotCount` 和 `slotBase`；相关代码文件 scoped
  `git diff --check` 退出 0，仅报告既有 CRLF/LF 提示 · 备注：本切片只收窄
  MethodInfo descriptor。当前生成 prologue/comment 仍保留旧 `frameByteSize=6272`
  与 4 个 frame setup 字段，后续 07-S3/07-S4 继续处理 prologue 坍塌。08-12 未开始。

- 2026-06-21 09:46:32 +08:00 · M1.5 / 07-S3 frame export-context gating 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 新增 `includeExportContext` 参数；
  `backend_aot_write_c_function_body()` 复用既有 `publishExports` predicate 传入 setup。
  `frame.module`、`frame.moduleExecuted`、`frame.functionTable`、`frame.functionCount`、
  `frame.functionThunks` 和 `frame.functionThunkCount` 现在只在 root/export publication
  可能发生时生成；focused non-export typed scalar 生成物不再含这些 export-context frame 字段 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-frame-export-context-gating.md` ·
  RED/GREEN：frame setup contract 先因 helper 缺少 `TZrBool includeExportContext` 失败；
  typed-scalar generated-product 针对旧非条件 export setup 字段加负断言；补 helper signature、
  function-body call-site 和条件发射后 GREEN · 测试结果：
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：generated C grep 确认 focused typed scalar 无
  `frame.recordHandle`、`frame.module*`、`frame.functionTable`、`frame.functionCount`、
  `frame.functionThunks`、`frame.functionThunkCount`；剩余 `frame.*` 为 `callInfo`、
  `function`、`generatedFrameSlotCount` 和 `slotBase`；相关代码文件 scoped `git diff --check`
  退出 0，仅报告既有 CRLF/LF 提示 · 备注：07-S3 尚未完成；剩余 4 个 frame 字段仍支撑
  return、cleanup、stack/frame-slot fallback 等旧边界路径，返回边界仍有 `SZrTypeValue`
  marshaling。08-12 未开始。

- 2026-06-21 09:38:15 +08:00 · M1.5 / 07-S3 frame recordHandle setup elision 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 不再发
  `frame.recordHandle = zr_aot_context.recordHandle;`；focused typed scalar 生成物新增
  `frame.recordHandle` 负断言并通过，当前生成 C 中不再出现该字段。源码扫描确认 active C 后端
  已无需要 `frame.recordHandle` 的 runtime helper 发射，剩余 prologue frame 字段仍由 return/export/call/
  cleanup 等旧边界路径读取 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-record-handle-setup-elision.md` · RED/GREEN：
  `zr_vm_aot_c_frame_setup_contracts_test` 先因 frame setup source 仍含
  `frame.recordHandle = zr_aot_context.recordHandle;` 失败；`zr_vm_aot_c_typed_scalar_test`
  先因 generated C 仍含 `frame.recordHandle` 失败；删除 setup assignment 后二者 GREEN · 测试结果：
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：generated C grep 确认无 `frame.recordHandle`，
  剩余 focused generated `frame.*` 为 `callInfo`、`function`、`functionCount`、`functionTable`、
  `functionThunkCount`、`functionThunks`、`generatedFrameSlotCount`、`module`、`moduleExecuted`
  和 `slotBase`；相关文件 scoped `git diff --check` 退出 0 · 备注：07-S3 尚未完成；
  MethodInfo 尚未替代全部 descriptor state，typed scalar prologue 仍有其他 fat-frame 字段，
  返回边界仍有 `SZrTypeValue` marshaling。08-12 未开始。

- 2026-06-21 09:23:10 +08:00 · M1.5 / 07-S3 fail macro function-index local 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_function_body()` 现在为每个 generated function 发射
  `const TZrUInt32 zr_aot_function_index = Nu;`；`ZR_AOT_C_FAIL()` 改为报告该
  function-local 常量和固定 `UINT32_MAX` instruction index，不再读取
  `frame.functionIndex` 或 `frame.currentInstructionIndex`；`backend_aot_write_c_frame_setup()`
  删除 `frame.functionIndex = zr_aot_context.resolvedFunctionIndex` setup assignment。
  focused typed scalar 生成物仍保留 unsupported-instruction block 内部的局部
  `zr_aot_function_index`，但不再含 frame-backed `frame.functionIndex`、
  `frame.currentInstructionIndex`、`frame.lastObservedInstructionIndex`、`frame.lastObservedLine`、
  `frame.observationMask`、`frame.publishAllInstructions` 或 `state->debugHookSignal` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-fail-macro-function-index-local.md` · RED/GREEN：
  `zr_vm_aot_c_typed_scalar_test` 先因缺少 generated product
  `const TZrUInt32 zr_aot_function_index = 0u;` 失败；
  `zr_vm_aot_c_source_contracts_test` 先因 `ZR_AOT_C_FAIL()` 仍使用 frame-backed
  function/current-instruction 字段失败；`zr_vm_aot_c_frame_setup_contracts_test` 先因
  setup 源码仍发 `frame.functionIndex = ...` 失败；补 function-local index、改 fail macro、
  删除 setup assignment 后全部 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；CTest 过滤
  `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts` 仅匹配已注册的
  `aot_c_typed_scalar` 并通过 1/1 · 检查：generated C grep 确认只剩
  `(unsigned)zr_aot_function_index`、`zr_aot_method_info_0` 与局部 function-index declarations，
  不再出现 frame function/current/observation 字段；相关文件 scoped `git diff --check`
  退出 0，仅报告既有 CRLF/LF 提示 · 备注：07-S3 尚未完成；`frame.recordHandle`、
  `frame.function`、`frame.callInfo`、`frame.slotBase`、module/function table 字段和
  `generatedFrameSlotCount` 仍在 typed scalar prologue 中，返回边界仍有 `SZrTypeValue`
  marshaling。08-12 未开始。

- 2026-06-21 09:15:34 +08:00 · M1.5 / 07-S3 MethodInfo skeleton 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` 新增轻量只读
  `SZrAotMethodInfo` ABI 类型，字段覆盖 `functionIndex`、`metadataFunction`、
  `registerFrameBytes`、`gcRootMap`、`signature` 和 `observationPolicy`；
  `backend_aot_c_emitter.c` 新增 `backend_aot_write_c_method_infos()`，在每个
  generated function forward declaration 后发射一个
  `static const SZrAotMethodInfo zr_aot_method_info_%u`。focused typed scalar
  生成物现在包含 `zr_aot_method_info_0`，其中 `.functionIndex = 0u`、
  `.registerFrameBytes = 6272u`、`.gcRootMap = ZR_NULL`、`.signature = ZR_NULL`、
  `.observationPolicy = 0u`；MethodInfo 目前只作为只读描述符骨架，不在 hot path
  读取，不改变边界执行语义 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-method-info-skeleton.md` · RED/GREEN：
  `zr_vm_aot_c_frame_setup_contracts_test` 先因 ABI/header+emitter 模板缺失
  `struct SZrFunction;` / `SZrAotMethodInfo` 失败；`zr_vm_aot_c_typed_scalar_test`
  先因 generated C 缺少 `static const SZrAotMethodInfo zr_aot_method_info_0`
  失败；补 ABI 类型与 emitter 后二者 GREEN · 测试结果：
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_return_contracts_test` 1/0；
  CTest 过滤 `aot_c_typed_scalar|frame_setup|return_contracts|source_contracts`
  仅匹配已注册的 `aot_c_typed_scalar` 并通过 1/1；`ctest -N -R
  'frame_setup|source_contracts|return_contracts|aot_c_return|aot_c_source'`
  返回 0 个已注册契约 CTest · 检查：focused generated C 含 MethodInfo 常量且仍无
  setup-time observation/default-debug 字段；相关文件 scoped `git diff --check`
  退出 0，仅报告既有 CRLF/LF 提示（全工作树 `git diff --check` 在当前大型 dirty
  worktree 中超时） · 备注：07-S3 尚未完成；`metadataFunction`/`gcRootMap`/
  `signature` 仍为 `ZR_NULL`，MethodInfo 尚未接管 typed function descriptor；
  focused typed scalar prologue 仍有 `frame.slotBase`、`state->stackTop`、
  `generatedFrameSlotCount`，返回边界仍有 `SZrTypeValue` marshaling。07-S4 仍负责把纯标量
  `registerFrameBytes` 收窄到 0；本切片记录当前 6272-byte frame requirement。
  `backend_aot_c_emitter.c` 当前 322 行、`zr_aot_abi.h` 62 行、
  `test_aot_c_frame_setup_contracts.c` 257 行；`test_aot_c_typed_scalar.c` 当前
  1038 行，本切片只追加 7 个 generated-product MethodInfo 断言，后续最小测试拆分边界仍是
  typed-scalar generated-C marker/forbidden-token 断言 helper 或更窄 regression contract。

- 2026-06-21 09:01:51 +08:00 · M1.5 / 07-S3 frame setup observation elision 切片 ·
  状态：子切片完成、07-S3 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_frame_setup()` 不再为每个 generated function 默认注入逐指令观测字段初始化：
  删除 `frame.currentInstructionIndex = 0`、`frame.lastObservedInstructionIndex = UINT32_MAX`、
  `frame.lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE`、`frame.observationMask = ...`、
  `frame.publishAllInstructions = ...` 以及 `state->debugHookSignal` / `ZR_DEBUG_HOOK_MASK_LINE`
  的 line-hook override。07-S1 已移除 typed function body 的 `zr_aot_begin_instruction`
  调用后，这些 setup-time 观测字段在当前 typed scalar hot path 中只剩 prologue 税 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-frame-setup-observation-elision.md` ·
  RED/GREEN：`zr_vm_aot_c_frame_setup_contracts_test` 先因
  `frame.currentInstructionIndex = 0;` 仍存在于 frame setup source 失败；typed-scalar
  generated-product 也先因生成 C 仍含该 setup 赋值失败；删除观测初始化后两者 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1（本 build 中 frame setup contract binary 未注册为 CTest）；
  生成 C 检查确认 focused typed scalar 不再出现 setup-time observation/default-debug 初始化 ·
  备注：07-S3 尚未完成；生成文件头部的 `ZR_AOT_C_FAIL` 宏仍保留
  `frame.currentInstructionIndex` 用于失败报告，且 typed scalar prologue 仍有 `frame.slotBase`、
  `state->stackTop`、`generatedFrameSlotCount` 和返回边界 `SZrTypeValue` marshaling。后续仍需
  MethodInfo 发射、fat-frame 字段移除、byte-frame/prologue 收窄和完整 boundary template。
  `backend_aot_c_frame_setup.c` 当前 86 行、`test_aot_c_frame_setup_contracts.c` 222 行；
  `test_aot_c_typed_scalar.c` 当前 1031 行，本切片只追加 6 个 focused generated-product
  negative assertions，后续最小测试拆分边界仍是 typed-scalar generated-C marker/forbidden-token
  断言 helper 或更窄 regression contract。

- 2026-06-21 08:52:58 +08:00 · M1.5 / 07-S2 i64 direct-return boundary local reuse 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `backend_aot_write_c_direct_return_i64_local()` 现在复用当前 frame setup 已生成的
  function-scope `zr_aot_call_info`，不再在 direct-return block 内重新声明
  `SZrCallInfo *zr_aot_call_info = frame.callInfo`；该 optimized path 仍由
  `backend_aot_c_scalar_locals_can_direct_return_i64_local()` 保守门控，因此 constructor
  函数不会进入此路径，return block 内也不再发 `frame.function` null guard 或
  `frame.function->functionName` constructor-name 分支。focused typed scalar 生成物保留
  `zr_aot_caller_result_value->value.nativeObject.nativeInt64 = zr_aot_s23/s48` 的边界写回，
  但不再出现 return-local `frame.callInfo`、`frame.function` guard、constructor-name check
  或 `zr_aot_result_slot = frame.slotBase + N` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-direct-return-i64-boundary-local-reuse.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧的
  `/* zr_aot_direct_return_i64_local */\n        SZrCallInfo *zr_aot_call_info = frame.callInfo;`
  失败；补 direct-return emitter 后 GREEN，并新增 return-local 环境符号负断言 · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `zr_vm_aot_c_return_contracts_test` 1/0；CTest `aot_c_typed_scalar` 1/1（本 build 中
  `ctest -N -R 'return|aot_c_return'` 返回 0 个已注册 return-contract CTest）；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；
  prologue/frame setup、完整 boundary marshaling template、non-i64 direct-return forms、
  byte-frame narrowing 和其他未证明 frame writes 仍待收敛。`backend_aot_c_lowering_control.c`
  当前 964 行，仍是既有 control/return emitter 边界；`tests/parser/test_aot_c_typed_scalar.c`
  当前 1025 行，本切片只给现有 focused generated-product fixture 增加负断言，暂不混入测试重组。
  后续最小拆分边界为 07-S2 typed-scalar generated-C marker/forbidden-token 断言 helper 或
  更窄的 scalar-local regression contract。

- 2026-06-21 08:41:11 +08:00 · M1.5 / 07-S2 RESET_STACK_NULL2 stateful pair proof 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `RESET_STACK_NULL2` 现在通过
  `backend_aot_c_scalar_locals_reset2_can_skip_value_slots()` 做双槽状态化证明，
  以 live bitmask 分别追踪 first/second slot，从当前 block 后缀进入 successor graph；
  两个 reset target 可在不同指令被覆盖/kill，只要任一路径在 frame/source read 前结束即可跳过
  frame-backed reset2。`backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local()`
  先确认 opcode 是实际 scalar-local consumer，再查询读槽，避免把 `GET_CONSTANT` 目标槽误判为 read ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null2-stateful-pair-skip.md` ·
  RED/GREEN：typed-scalar generated-product 先因缺少
  `/* zr_aot_reset_stack_null2_scalar_local_skip slots=3,4 */` 失败；补状态化 pair proof
  后 GREEN，并新增 `slots=6,7` marker-only/negative 断言。生成 C 现在包含 `slots=3,4`、
  `slots=5,6`、`slots=6,7`、`slots=9,10`、`slots=15,16` reset2 skip marker，
  且不再出现 `zr_aot_value_exec_reset_stack_null2` · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；prologue/frame setup、边界 local restoration、
  generic float copy/type checks、完整 typed boundary marshaling 和其他未证明的 frame writes
  仍待收敛。`backend_aot_c_scalar_locals.c` 当前 2278 行，本切片继续扩展同一
  scalar-local liveness/reset proof 边界；暂不拆分以避免将大量 private predicate 过早公开，
  后续最小拆分边界为独立 scalar-local liveness/result/reset proof 模块。

- 2026-06-21 08:20:48 +08:00 · M1.5 / 07-S2 RESET_STACK_NULL2 scalar-local skip 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  `RESET_STACK_NULL2` 现在在两个目标槽都通过
  `backend_aot_c_scalar_locals_reset_can_skip_value_slot()` 时不再发 frame-backed reset2
  block，而是发 `/* zr_aot_reset_stack_null2_scalar_local_skip slots=A,B */`。
  focused typed scalar 中分支两侧的 `slots=5,6` reset2 被证明为 local-only dead reset：
  slot 5/6 均会在后续路径被覆盖，且没有先发生 frame/source read，因此旧的
  `SZrTypeValue *zr_aot_first/second`、`frame.slotBase[5/6].value` 与双
  `ZrCore_Value_ResetAsNull` 被删除 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null2-scalar-local-skip.md` ·
  RED/GREEN：初始 RED 锁定 `slots=3,4`，该子切片当时的 single-slot 独立证明尚未携带
  paired live state，顺序覆盖时会保守失败；有效 RED 改为 `slots=5,6` 并先因缺少
  `/* zr_aot_reset_stack_null2_scalar_local_skip slots=5,6 */` 失败，补 reset2
  双槽 routing 和 marker-only emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 检查确认两处 `slots=5,6` skip marker，
  且无旧 focused `frame.slotBase[5/6]` reset2 block；`zr_vm_aot_c_source_contracts_test`
  19/0；CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；仍有其他 reset pairs、prologue/frame setup、
  边界 local restoration、generic float copy/type checks 和完整 typed boundary
  marshaling 待收敛。

- 2026-06-21 08:11:47 +08:00 · M1.5 / 07-S2 RESET_STACK_NULL scalar-local skip 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  single-slot `RESET_STACK_NULL` 现在先调用
  `backend_aot_c_scalar_locals_reset_can_skip_value_slot()`；当目标槽有 scalar C local
  覆盖、函数无 exception handlers、槽未导出，且从当前 block 后缀到 successor graph
  的所有可达路径都在任何 frame/source read 前覆盖或 kill 该槽时，生成器只发
  `/* zr_aot_reset_stack_null_scalar_local_skip slot=N */`。focused typed scalar
  中分支两侧的 `slot 21` reset 现在都被证明为 local-only dead reset，不再构造
  `SZrTypeValue *zr_aot_destination`，不再检查 `frame.slotBase`，也不再调用
  `ZrCore_Value_ResetAsNull(&frame.slotBase[21].value)` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-reset-stack-null-scalar-local-skip.md` ·
  RED/GREEN：focused generated-product 先因缺少
  `/* zr_aot_reset_stack_null_scalar_local_skip slot=21 */` 失败；补 function-body
  routing、reset dead-slot successor 扫描和 marker-only emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 检查确认两处 `slot=21` skip marker，
  且无旧 `frame.slotBase[21].value` single-slot reset；`zr_vm_aot_c_source_contracts_test`
  19/0；CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；`RESET_STACK_NULL2`、其他 reset shapes、
  prologue/frame setup、边界 local restoration、generic float copy/type checks 和完整
  typed boundary marshaling 仍待收敛。

- 2026-06-21 07:51:00 +08:00 · M1.5 / 07-S2 i64 direct-return local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的两个 i64 return sites (`slot 23` 与 `slot 48`) 现在使用
  `zr_aot_direct_return_i64_local`，直接把 `zr_aot_s23` / `zr_aot_s48` 写入 caller result
  `SZrTypeValue`；`backend_aot_write_c_direct_return_i64_local()` 不再委托 frame-backed
  direct return，也不再生成 `zr_aot_result_slot = frame.slotBase + N` /
  `zr_aot_result_value` 读取路径。新增
  `backend_aot_c_scalar_locals_can_direct_return_i64_local()` 集中校验 i64 local direct-return
  的保守条件，并让 i64 result-skip proof 在可由 local direct return 消费的
  `FUNCTION_RETURN` 边界停止；focused generated C 中 return 前的 `slot 23/48`
  result materialization 被删除，返回块直接设置 `ZR_VALUE_TYPE_INT64` 与
  `nativeInt64 = zr_aot_sN` · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-direct-return-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因缺少
  `/* zr_aot_direct_return_i64_local */` 失败；补 local-return emitter、return 边界
  result-skip 证明和 generated-product 断言后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 检查确认存在
  `nativeInt64 = zr_aot_s23` / `nativeInt64 = zr_aot_s48`，且不存在
  `zr_aot_result_slot = frame.slotBase + 23/48` 与 return 前 result frame materialization；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；
  reset-stack-null frame writes、prologue/frame setup、边界 local restoration、
  generic float copy/type checks 和完整 typed boundary marshaling 仍待收敛；本轮 WSL
  shared parser rebuild 还暴露并最小修复了既有 untracked `cfg_throw_profile.c`
  缺少 `cfg_node_throw_kind_mask()` 前置声明的 C11 编译阻塞。

- 2026-06-21 07:31:11 +08:00 · M1.5 / 07-S2 f64 primitive constant local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 f64 primitive constants (`slot 19 = 1.5`、`slot 20 = 2.0`)
  现在先发独立 `zr_aot_scalar_constant_f64_local` block，并在后续 f64 binary 可直接消费
  `zr_aot_f19/f20` 时跳过 `zr_aot_value_exec_primitive_constant` frame write；
  生成 C 中 focused f64 constants 只剩 `zr_aot_f19 = (TZrFloat64)1.5;` 与
  `zr_aot_f20 = (TZrFloat64)2;`，不再写回 `frame.slotBase[19/20].value` double payload。
  新增 `backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot()`，复用同一套可达
  consumer/result-skip 证明 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-constant-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因缺少 `zr_aot_scalar_constant_f64_local`
  standalone block 失败；补 f64 constant local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认 f64 constants 后直接进入
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 primitive constants、generic float copy/type checks、
  direct return/result frame fallback removal、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1800 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 07:24:00 +08:00 · M1.5 / 07-S2 i64 primitive constant cross-block local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的前两个 i64 primitive constants (`slot 0 = 21`、`slot 1 = 2`)
  现在只发 `zr_aot_scalar_constant_i64_local` + `zr_aot_s0 = (TZrInt64)21;` /
  `zr_aot_s1 = (TZrInt64)2;`，不再紧跟 `zr_aot_value_exec_primitive_constant`
  或写回 `frame.slotBase[0/1].value` integer payload。primitive-constant skip proof
  复用 result-skip 可达 consumer 扫描，只有后续可达路径均能读/kill scalar local 时才跳过
  frame result；`FUNCTION_RETURN` 被保守视为 frame-based direct-return 边界，返回槽仍保持
  materialized，避免直接返回读取未写 frame 的结果 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-constant-cross-block-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 `slot 0` constant-local 后仍跟
  primitive-constant frame write 失败；补 primitive constant local-only proof 并收紧 direct-return
  boundary 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认
  slots 0/1 只剩 scalar-local assignment，direct-return slots 23/48 仍在 return 前 materialize；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他
  primitive constants、generic float copy/type checks、direct return/result frame fallback removal、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1800 行，后续最小拆分边界为
  scalar result-skip/liveness proof 模块。

- 2026-06-21 06:54:52 +08:00 · M1.5 / 07-S2 numeric conversion result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 typed numeric conversion results (`TO_INT_FLOAT dstSlot=31 srcSlot=19`、
  `TO_INT_UNSIGNED dstSlot=31 srcSlot=8`、`TO_UINT_SIGNED dstSlot=31 srcSlot=2`、
  `TO_UINT_FLOAT dstSlot=31 srcSlot=19`) 现在只发 scalar-local conversion：
  `zr_aot_s31 = (TZrInt64)zr_aot_f19;`、带 `zr_aot_limit` guard 的 u64->i64 写入、
  `zr_aot_u31 = (TZrUInt64)zr_aot_s2;`、`zr_aot_u31 = (TZrUInt64)zr_aot_f19;`，
  不再构造 `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` / `zr_aot_u_result`
  或写回 `frame.slotBase[31].value` integer payload。i64/u64 conversion emitters 复用
  source written-before + matching result-skip proof，只有 source 已证明为 scalar local、目标槽有
  matching local 且后续可达 consumer 均可读/kill local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-numeric-conversion-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 `TO_INT_FLOAT` destination/result
  物化失败；补 numeric conversion result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认四个 focused conversion blocks 只剩
  direct local casts/guard；`zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar
  result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1800 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 06:47:40 +08:00 · M1.5 / 07-S2 f64 conversion result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 f64 conversion results (`TO_FLOAT_SIGNED dstSlot=31 srcSlot=2` 与
  `TO_FLOAT_UNSIGNED dstSlot=31 srcSlot=8`) 现在只发
  `zr_aot_f31 = (TZrFloat64)zr_aot_s2;` 与
  `zr_aot_f31 = (TZrFloat64)zr_aot_u8;`，不再构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_f_result` 或写回
  `frame.slotBase[31].value` double payload。f64 conversion emitter 复用 signed/u64
  source written-before + f64 result-skip proof，只有 source 已证明为 scalar local、目标槽有
  f64 local 且后续可达 consumer 均可读/kill f64 local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-conversion-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 f64 conversion destination/result
  物化失败；补 f64 conversion result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认两个 focused f64 conversion blocks 只剩
  `zr_aot_f31 = (TZrFloat64)zr_aot_s2;` 与 `zr_aot_f31 = (TZrFloat64)zr_aot_u8;`；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar
  conversion/result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1800 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 06:37:26 +08:00 · M1.5 / 07-S2 f64 binary result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 f64 binary result (`dstSlot=32 leftSlot=19 rightSlot=20`) 现在只发
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`，不再构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_f_result` 或写回
  `frame.slotBase[32].value` double payload。f64 binary emitter 复用 f64 source
  written-before + f64 result-skip proof，只有左右源已证明为 f64 locals、目标槽有
  f64 local 且后续可达 consumer 均可读 f64 local/kill 时才跳过 frame result；
  proof 新增识别 f64 binary、f64-to-int/uint conversion 与 f64 stack-copy consumers，
  未知 frame-dependent read 仍保守拒绝 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-f64-binary-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 f64 binary destination/result
  物化失败；补 f64 result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认 focused f64 binary block 只剩
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 scalar result materialization、primitive constant frame writes、
  generic float copy/type checks、direct return/result frame fallbacks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1800 行，后续最小拆分边界为
  scalar result-skip/liveness proof 模块。

- 2026-06-21 06:23:48 +08:00 · M1.5 / 07-S2 i64 compare bool result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 signed i64 compare bool results (`dstSlot=27` 与 `dstSlot=7`,
  `leftSlot=2 rightSlot=4`) 现在只发
  `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);` 与
  `zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`，不再构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` 或写回 bool frame payload。
  i64 compare emitter 复用 signed source written-before + bool result-skip proof，只有左右源
  已证明为 signed locals、目标槽有 bool local 且后续可达 consumer 均可读 bool local/kill
  时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-compare-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 compare bool destination/result
  物化失败；补 result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认两个 focused compare block 只剩 bool local
  assignment；`zr_vm_aot_c_source_contracts_test` 19/0；CTest `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar
  result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1100 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 06:06:05 +08:00 · M1.5 / 07-S2 i64 bit-not result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的 signed i64 bit-not (`dstSlot=19 sourceSlot=1`) 现在只发
  `zr_aot_s19 = ~zr_aot_s1;`，不再为 `dstSlot=19` 构造 `SZrTypeValue *zr_aot_destination`、
  `zr_aot_s_result`、source frame type-check/reload 或写回 `frame.slotBase[19].value`。
  i64 bit-not emitter 复用 signed i64 written-before + result-skip proof，只有 source
  已证明为 written scalar local、目标槽有 signed local 且后续可达 consumer 均可读 signed local
  时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-bit-not-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 bit-not destination/result
  物化失败；补 i64 bit-not result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认只剩
  `zr_aot_s19 = ~zr_aot_s1;`；`zr_vm_aot_c_source_contracts_test` 目标验证 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 scalar result materialization、primitive constant frame writes、
  generic float copy/type checks、direct return/result frame fallbacks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c`
  已超过 1100 行，后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 05:57:20 +08:00 · M1.5 / 07-S2 i64 shift result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的首个 signed i64 shift (`dstSlot=19 leftSlot=15 rightSlot=1`) 现在保留
  shift-count range guard 后只发 `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);`，
  不再为 `dstSlot=19` 构造 `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result`
  或写回 `frame.slotBase[19].value`。i64 shift emitter 复用 signed i64 result-skip proof，
  只有左右源均已证明为 written scalar locals、目标槽有 signed local 且后续可达 consumer
  均可读 signed local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-shift-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 shift destination/result
  物化失败；补 i64 shift result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认保留 range guard 且只剩
  `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);`；`zr_vm_aot_c_source_contracts_test`
  目标验证 19/0；CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：07-S2 尚未完成；其他 scalar result materialization、primitive
  constant frame writes、generic float copy/type checks、direct return/result frame fallbacks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration 仍待收敛；
  `backend_aot_c_scalar_locals.c` 已超过 1100 行，后续最小拆分边界为
  scalar result-skip/liveness proof 模块。

- 2026-06-21 05:52:10 +08:00 · M1.5 / 07-S2 i64 bitwise result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的首个 signed i64 bitwise (`dstSlot=16 leftSlot=12 rightSlot=0`) 现在只发
  `zr_aot_s16 = zr_aot_s12 & zr_aot_s0;`，不再为 `dstSlot=16` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` 或写回 `frame.slotBase[16].value`。
  i64 bitwise emitter 复用 signed i64 result-skip proof，只有左右源均已证明为 written scalar locals
  且后续可达 consumer 均可读 signed local 时才跳过 frame result · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-bitwise-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 bitwise destination/result
  物化失败；补 i64 bitwise result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认只剩
  `zr_aot_s16 = zr_aot_s12 & zr_aot_s0;`；`zr_vm_aot_c_source_contracts_test` 目标重建并执行 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：07-S2 尚未完成；其他 scalar result materialization、primitive constant frame writes、
  generic float copy/type checks、direct return/result frame fallbacks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c`
  已超过 1100 行，后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 05:43:39 +08:00 · M1.5 / 07-S2 i64 binary result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、M1.5/07 部分完成、08-12 未开始 · 完成项目：
  focused typed scalar 的首个 signed i64 binary (`dstSlot=2 leftSlot=0 rightSlot=1`) 现在只发
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`，不再为 `dstSlot=2` 构造
  `SZrTypeValue *zr_aot_destination`、`zr_aot_s_result` 或写回 `frame.slotBase[2].value`。
  i64 result-skip proof 现在识别 signed local consumers、typed signed conversions、direct signed branches、
  scalar stack-copy consumers 和 unconditional `JUMP`，未知 slot mentions 仍保守拒绝 · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-i64-binary-result-local-only.md` ·
  RED/GREEN：focused typed-scalar generated-product 先因旧 i64 binary destination/result 物化失败；
  补 i64 result-skip proof 和 i64 binary result local-only path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；生成 C 片段确认只剩
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`；`zr_vm_aot_c_source_contracts_test` 目标重建并执行 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示。
  备注：07-S2 尚未完成；其他 scalar
  result materialization、primitive constant frame writes、generic float copy/type checks、
  direct return/result frame fallbacks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration 仍待收敛；`backend_aot_c_scalar_locals.c` 已超过 1100 行，
  后续最小拆分边界为 scalar result-skip/liveness proof 模块。

- 2026-06-21 05:18:36 +08:00 · 07-S2 u64 compare bool result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  focused typed scalar 的 `unsignedSum > unsignedRight` 与 `unsignedInverted <= unsignedRight`
  u64 compare bool results 现在使用 bool result-skip 可达消费者证明；`dstSlot=14` / `dstSlot=23`
  的生成块只保留 `zr_aot_b14 = ...` / `zr_aot_b23 = ...`，不再声明
  `SZrTypeValue *zr_aot_destination`，不再创建 `zr_aot_u_result`，也不再写
  `frame.slotBase[14/23].value`。`dstSlot=23` 后续 `JUMP_IF_BOOL_FALSE` 直接消费
  `zr_aot_b23`，`dstSlot=14` 被 reset-stack-null kill；该 proof 继续保守拒绝未知
  frame-dependent reads · 产出：
  `tests/acceptance/2026-06-21-aot-m1-5-u64-compare-result-local-only.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `dstSlot=14` u64 compare destination
  指针/结果物化失败；补 bool result-skip proof 和 compare local-only result path 后 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 ·
  备注：此切片仍保留其他 scalar result materialization、更多 primitive constant frame writes、
  direct return/result frame fallbacks、generic float copy/type checks、prologue/frame setup、
  reset-stack-null frame writes 和边界 local restoration，07-S2 完整验收仍未达成；
  `backend_aot_c_scalar_locals.c` 已超过 1100 行但本次只扩展同一 scalar-local
  liveness/result-skip proof，后续最小拆分边界为提取 result-skip/liveness 证明模块。

- 2026-06-21 04:55:24 +08:00 · 07-S2 u64 bit-not result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  focused typed scalar 的 `unsignedInverted` u64 bit-not result 现在复用 u64 result-skip
  可达消费者证明；`dstSlot=33` 的生成块只保留 `zr_aot_u33 = ~zr_aot_u7;`，
  不再声明 `SZrTypeValue *zr_aot_destination`，不再创建 `zr_aot_u_result`，也不再写
  `frame.slotBase[33].value` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-bit-not-result-local-only.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `dstSlot=33` u64 bit-not destination
  指针/结果物化失败；补 bit-not local-only result path 后 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：此切片仍保留其他 scalar result materialization、更多 primitive
  constant frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 04:48:06 +08:00 · 07-S2 u64 shift result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  focused typed scalar 的 `unsignedShifted` / `unsignedShiftedBack` u64 shift result 现在复用
  u64 result-skip 可达消费者证明；`dstSlot=13` 与 `dstSlot=14` 的生成块只保留
  signed shift-count range guard 和 `zr_aot_u13 = zr_aot_u8 << zr_aot_s1;` /
  `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;`，不再声明 `SZrTypeValue *zr_aot_destination`，
  不再创建 `zr_aot_u_result`，也不再写 `frame.slotBase[13/14].value` ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-shift-result-local-only.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `dstSlot=13` u64 shift destination
  指针/结果物化失败；补 shift local-only result path 并保留 range guard 后 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：此切片仍保留其他 scalar result materialization、更多 primitive
  constant frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 04:42:16 +08:00 · 07-S2 u64 bitwise result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  focused typed scalar 的 `unsignedMasked` u64 bitwise result 现在复用 u64 result-skip
  可达消费者证明；`dstSlot=12` 的生成块只保留
  `zr_aot_u12 = zr_aot_u8 & zr_aot_u7;`，不再声明 `SZrTypeValue *zr_aot_destination`，
  不再创建 `zr_aot_u_result`，也不再写 `frame.slotBase[12].value`。该证明继续从当前
  block 后缀沿 successor graph 追踪 live value，只允许已支持的 local consumers，遇到未知
  frame-dependent read 则拒绝，路径上 slot 被覆盖后停止追踪 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-bitwise-result-local-only.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `dstSlot=12` u64 bitwise destination
  指针/结果物化失败；补 bitwise local-only result path 后 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：此切片仍保留其他 scalar result materialization、更多 primitive
  constant frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 04:33:00 +08:00 · 07-S2 u64 binary result local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  focused typed scalar 的 `unsignedSum` u64 binary result 现在在后续可达消费者均可使用
  u64 scalar local 时跳过 destination frame materialization；`dstSlot=8` 的生成块只保留
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`，不再声明 `SZrTypeValue *zr_aot_destination`，
  不再创建 `zr_aot_u_result`，也不再写 `frame.slotBase[8].value`。`backend_aot_c_scalar_locals`
  新增 u64 result-skip 证明：从当前 block 后缀沿 successor graph 追踪 live value，只允许
  已支持的 local consumers，遇到未知 frame-dependent read 则拒绝，路径上 slot 被覆盖后停止追踪 ·
  产出：`tests/acceptance/2026-06-21-aot-m1-5-u64-binary-result-local-only.md` ·
  RED/GREEN：typed-scalar generated-product 先因旧 `dstSlot=8` u64 binary destination
  指针/结果物化失败；补 u64 binary local-only result path 与可达消费者证明后 GREEN ·
  测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 退出 0 且仅报告既有
  LF/CRLF 提示 · 备注：此切片仍保留其他 scalar result materialization、更多 primitive
  constant frame writes、direct return/result frame fallbacks、generic float copy/type checks、
  prologue/frame setup、reset-stack-null frame writes 和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 04:10:01 +08:00 · 07-S2 u64 constant/conversion local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  focused typed scalar 的 `uint` 初始化链路现在不再从 signed literal 经 frame-backed
  `TO_UINT` 和 u64 binary 回读；`unsignedLeft/unsignedRight/unsignedSum` 生成
  `zr_aot_u6 = (TZrUInt64)zr_aot_s6;`、`zr_aot_u7 = (TZrUInt64)4;` 和
  `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`。`backend_aot_c_scalar_locals` 在目标槽有
  u64 scalar local 覆盖时把 signed immediate 记录为 u64 written proof，`TO_UINT`
  与 u64 binary emitters 由此跳过 source frame type-check/reload；slot 6/7 的 primitive
  constant frame write 与 `TO_UINT` frame-backed fallback 被该 focused shape 覆盖移除 ·
  RED/GREEN：typed-scalar generated-product 先因缺少 direct `zr_aot_u6` cast 失败，
  补 conversion fast path 后又暴露 slot 7 `TO_UINT` source frame check，补 signed-immediate
  u64 written proof 后又暴露 u64 binary source frame check/reload；补 u64 binary
  written-source 检查后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1；
  `git diff --check` 退出 0 且仅报告既有 LF/CRLF 提示 · 备注：此切片仍保留 scalar
  result materialization、更多 primitive constant frame writes、direct return/result frame
  fallbacks、generic float copy/type checks、prologue/frame setup、reset-stack-null frame writes
  和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 03:46:02 +08:00 · 07-S2 scalar stack-copy local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  bool/i64/u64 stack-copy 现在与 f64 一样，在 source/destination 都有 scalar local
  时直接发 C local assignment；focused typed scalar 的
  `zr_aot_b5 = (TZrBool)(zr_aot_b7 != 0u)`、`zr_aot_u9 = zr_aot_u12`、
  `zr_aot_u21 = zr_aot_u33`、`zr_aot_s13 = zr_aot_s16` 和
  `zr_aot_s18 = zr_aot_s19` 不再构造 source/destination `SZrTypeValue` 指针、
  不再做 source tag-check，也不再写 destination frame payload · RED/GREEN：
  typed-scalar generated-product 重建后先因缺少直接 bool local copy 失败，
  补 bool/i64/u64 stack-copy local-only fast path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1 · 备注：此切片仍保留 scalar result
  materialization、primitive constant frame writes、direct return/result frame fallbacks、
  generic float copy/type checks、prologue/frame setup 和边界 local restoration，
  07-S2 完整验收仍未达成。

- 2026-06-21 03:40:27 +08:00 · 07-S2 f64 stack-copy local-only 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  `backend_aot_write_c_scalar_stack_copy_f64()` 在 source/destination 都有 f64 scalar local
  时直接发 `zr_aot_fD = zr_aot_fS;`，focused typed scalar 的
  `zr_aot_f40 = zr_aot_f19` 与 `zr_aot_f22 = zr_aot_f40` 不再构造
  `SZrTypeValue *zr_aot_destination` / `const SZrTypeValue *zr_aot_source`，
  也不再检查 source float tag、release destination 或写 `ZR_VALUE_TYPE_DOUBLE` frame payload ·
  RED/GREEN：typed-scalar generated-product 重建后先因缺少直接 f64 local copy 失败，
  补 stack-copy f64 local-only fast path 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1 · 备注：此切片仍保留非 f64 stack-copy frame writes、
  scalar result materialization、primitive constant frame writes、return/result frame fallbacks
  和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 03:25:22 +08:00 · 07-S2 f64 source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  公开 `backend_aot_c_scalar_locals_f64_written_before()`，并在 f64 binary 与
  float-source conversion 中复用该 must-write 证明；focused typed scalar 的
  `zr_aot_f32 = zr_aot_f19 * zr_aot_f20`、`zr_aot_s31 = (TZrInt64)zr_aot_f19`
  和 `zr_aot_u31 = (TZrUInt64)zr_aot_f19` 不再先从 source frame reload
  `zr_aot_f19/f20` · RED/GREEN：typed-scalar generated-product 先因旧 f64 binary
  source frame type-check 失败，补 f64 written-before wrapper 和 binary/conversion emitter
  后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 ·
  备注：此切片仍保留 destination frame writes、generic float copy/type checks、
  return/result materialization 和边界 local restoration，07-S2 完整验收仍未达成。

- 2026-06-21 03:05:55 +08:00 · 07-S2 unsigned shift source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  u64 shift lowering 现在分别用 `u64_written_before` / `i64_written_before` 证明 left
  operand 和 shift count；已写入 C local 时跳过 source frame type-check/reload，只保留
  shift-count range guard。focused typed scalar 的 `zr_aot_u13` / `zr_aot_u14` 直接从
  `zr_aot_u8/zr_aot_u10` 与 `zr_aot_s1` 计算 · RED/GREEN：typed-scalar
  generated-product 先因旧 unsigned shift source frame fallback 失败，补 u64 shift emitter
  后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1；
  `git diff --check` 仅有既有换行提示 · 备注：此切片仍保留 float source fallback、
  destination frame writes 和 result materialization，07-S2 完整验收仍未达成。

- 2026-06-21 03:00:10 +08:00 · 07-S2 unsigned conversion source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  unsigned-source conversion 复用 `backend_aot_c_scalar_locals_u64_written_before()`；
  `TO_FLOAT_UNSIGNED` / `TO_INT_UNSIGNED` 对已写入 C local 的 source 跳过
  `frame.slotBase[8]` source 赋值、source type-check 和 frame reload，直接用 `zr_aot_u8`
  做 cast/wrap · RED/GREEN：typed-scalar generated-product 先因旧 unsigned conversion
  source frame fallback 失败，补 conversion emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 仅有既有换行提示 ·
  备注：此切片仍保留 float-source conversion、unsigned shift、float binary source fallback
  以及 destination frame writes，07-S2 完整验收仍未达成。

- 2026-06-21 02:51:35 +08:00 · 07-S2 unsigned source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  `backend_aot_c_scalar_locals_u64_written_before()` 公开同一套 must-write 证明；
  unsigned compare、u64 bitwise binary、u64 bit-not 对已写入 C local 的 unsigned source
  跳过 frame source type-check/reload。focused typed scalar 的 `zr_aot_b14`、`zr_aot_b23`
  和 `zr_aot_u33` 直接复用 `zr_aot_uN` · RED/GREEN：typed-scalar generated-product
  先因旧 unsigned compare source frame fallback 失败，补 compare 后又暴露 unsigned
  bitwise source fallback，补 bitwise/bit-not 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 仅有既有换行提示 ·
  备注：此切片仍保留 unsigned shift、conversion、float source fallback 以及 destination
  frame writes，07-S2 完整验收仍未达成。

- 2026-06-21 02:35:27 +08:00 · 07-S2 signed conversion source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  conversion lowering 现在接收 exec instruction index，并对 signed-source
  `TO_FLOAT_SIGNED` / `TO_UINT_SIGNED` 查询
  `backend_aot_c_scalar_locals_i64_written_before()`；已写入 C local 的 signed source
  conversion 跳过 `frame.slotBase[2]` source 赋值、source type-check 和 frame reload，
  直接使用 `zr_aot_s2` 参与 `f64/u64` cast · RED/GREEN：typed-scalar
  generated-product 先因旧 signed conversion source frame fallback 失败，补
  `backend_aot_c_scalar_conversion.c` 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  CTest 中已注册的 `aot_c_typed_scalar` 1/1；`git diff --check` 仅有既有换行提示 ·
  备注：此切片仍保留 conversion destination frame writes 和 result `SZrTypeValue`
  materialization，float/unsigned source conversion 仍待后续 written-before 证明。

- 2026-06-21 02:23:15 +08:00 · 07-S2 signed compare source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  signed i64 compare emitter 现在按源槽查询 `backend_aot_c_scalar_locals_i64_written_before()`；
  已写入 C local 的 compare 源跳过 source frame-slot type 校验和 frame reload。focused
  typed scalar 中 `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4)` 直接复用
  `zr_aot_s2/zr_aot_s4`，不再先检查/重读 `frame.slotBase[2/4]` · RED/GREEN：
  typed-scalar generated-product 先因旧 signed compare source type-check 失败，补 scalar
  SemIR compare emitter 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 ·
  备注：此切片仍保留 destination frame writes 和 result `SZrTypeValue` materialization，
  07-S2 完整验收项“typed 函数体零 `SZrTypeValue`/frame write”仍未达成。

- 2026-06-21 02:10:32 +08:00 · 07-S2 cross-block signed source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  signed i64 local-written-before 证明从当前 basic block 内扫描升级为基于 ExecIR successor
  图的保守 must-write 数据流；只有所有可达前驱都已经写入同类 local 时，后续 block 才复用
  `zr_aot_sN`。focused typed scalar 中跨 block 的 `slot 2/3` signed binary 不再发
  `frame.slotBase[2/3].value.type` source 校验，也不再连续回读到 `zr_aot_s2/s3`；
  constant signed branch 在 `slot 2` 已证明 live 时直接比较 `zr_aot_s2` 与 C literal，不再构造
  `SZrTypeValue` left source · RED/GREEN：typed-scalar generated-product 先因旧跨块
  source type-check 失败；补 dataflow 后又因 constant branch 仍构造 frame-backed left source 失败；
  删除该 fallback 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 ·
  备注：此切片仍保留 destination frame writes 和 result `SZrTypeValue` materialization，
  07-S2 完整验收项“typed 函数体零 `SZrTypeValue`/frame write”仍未达成。

- 2026-06-21 01:50:07 +08:00 · 07-S2 signed branch source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  signed compare branch 在左右操作数均已写入 i64 C local 时提前发纯 C local branch，不再构造
  `SZrTypeValue` source 指针，也不再检查 `zr_aot_left->type`/`zr_aot_right->type`。focused
  typed scalar 第一条 signed branch 现在只保留 `if (zr_aot_s2 <= zr_aot_s4) { goto ...; }` ·
  RED/GREEN：typed-scalar generated-product 先因旧 signed branch source tag-check 失败，补
  control emitter 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 ·
  备注：此切片仍保留必要 frame 写以保持尚未迁移的 consumer 可执行，07-S2 完整验收项
  “typed 函数体零 `SZrTypeValue`/frame write”仍未达成。

- 2026-06-21 01:40:20 +08:00 · 07-S2 signed shift source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  signed i64 shift emitter 改为按源槽调用 `backend_aot_c_scalar_locals_i64_written_before()`；
  已写入 C local 的 shift 左源跳过 source frame-slot type 校验和 frame reload。focused typed
  scalar 中 signed shift-left/right 直接复用 `zr_aot_s15`/`zr_aot_s16`；`slot 1` shift count
  因当前 written-before 证明仍限于基本块内，保守保留 frame fallback 和范围检查 · RED/GREEN：
  typed-scalar generated-product 先因旧 signed shift source type-check 失败，补 emitter 后
  GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test`
  19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 · 备注：此切片仍保留必要 frame 写以保持
  尚未迁移的 consumer 可执行，07-S2 完整验收项“typed 函数体零 `SZrTypeValue`/frame write”
  仍未达成。

- 2026-06-21 01:31:55 +08:00 · 07-S2 signed bitwise source-local 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  signed i64 bitwise emitter 改为按源槽调用 `backend_aot_c_scalar_locals_i64_written_before()`；
  已写入 C local 的源跳过 source frame-slot type 校验和 frame reload。focused typed scalar 中
  `zr_aot_s16 = zr_aot_s12 & zr_aot_s0` 不再重读 `frame.slotBase[12]`，而是复用
  `zr_aot_s12`；`slot 0` 因当前 written-before 证明仍限于基本块内，保守保留 frame fallback ·
  RED/GREEN：typed-scalar generated-product 先因旧 signed bitwise source type-check 失败，
  补 emitter 后 GREEN · 测试结果：`zr_vm_aot_c_typed_scalar_test` 1/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；CTest 中已注册的 `aot_c_typed_scalar` 1/1 ·
  备注：此切片仍保留必要 frame 写以保持尚未迁移的 consumer 可执行，07-S2 完整验收项
  “typed 函数体零 `SZrTypeValue`/frame write”仍未达成。

- 2026-06-21 00:38:52 +08:00 · 07-S2 signed primitive constant scalar-local first-read 切片 ·
  状态：子切片完成、07-S2 部分完成、07/M1.5 部分完成 · 完成项目：
  signed primitive constant 在有 i64 scalar-local 覆盖时先发 `zr_aot_scalar_constant_i64_local`
  赋值块；第一条 i64 binary 在 left/right 已写 C local 时去掉 source frame-slot type 校验，
  直接使用 `zr_aot_s0 * zr_aot_s1` · RED/GREEN：typed-scalar generated-product 先因缺少
  scalar constant block 失败，补 emitter 后 GREEN · 测试结果：
  `zr_vm_aot_c_typed_scalar_test` 1/0；`zr_vm_aot_c_source_contracts_test` 19/0；
  `ctest -R 'aot_c_typed_scalar'` 1/1 · 备注：此切片仍保留必要 frame 写以保持尚未迁移的
  consumer 可执行，07-S2 完整验收项“typed 函数体零 `SZrTypeValue`/frame write”仍未达成。

- 2026-06-21 00:02:31 +08:00 · 07-S1 typed 函数体 begin-instruction 环境隔离切片 ·
  状态：07-S1 完成、07/M1.5 部分完成 · 完成项目：
  `backend_aot_c_function_body.c` 删除逐指令 `backend_aot_write_c_begin_instruction()`
  发射调用，focused typed scalar 生成物禁止 `zr_aot_begin_instruction`，source-contract
  禁止 function-body 源码继续调用 begin-instruction helper。helper 本体暂保留，后续 MethodInfo/
  观测策略收敛再处理 · RED/GREEN：source-contract 先因旧调用失败，移除调用后 GREEN ·
  测试结果：`zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_typed_scalar_test` 1/0；
  CTest 过滤套件中已注册的 `aot_c_typed_scalar` 1/1 · 备注：07-S2 的 `CONST`/标量单寄存器写、
  typed 函数体零 `SZrValue` 写、零 `frame.slotBase` 写仍待推进。
