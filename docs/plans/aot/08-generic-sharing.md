---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/roslyn/mono 完善泛型共享机制
  - decision: 2026-06-20 泛型共享主线 = il2cpp 式（值类型单态化 + 引用类型共享 + 泛型字典 + hybrid deopt 兜底）
references:
  - lua/hybridclr/libil2cpp/metadata/GenericSharing.h        # IsShareable / Il2CppRGCTXData
  - lua/hybridclr/libil2cpp/metadata/GenericMethod.h
  - lua/hybridclr/libil2cpp/vm/GenericClass.cpp
  - lua/mono/mono/mini/mini-generic-sharing.c                # MRGCTX / gsharedvt（备选，未采用）
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/DependencyAnalysis/GenericDictionaryNode.cs
  - lua/roslyn/src/Compilers/Core/Portable/PEWriter/MetadataWriter.cs   # TypeSpec / MethodSpec 编码
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h            # SZrGenericType / SZrGenericDeclaration / EZrGenericParameterKind
  - zr_vm_parser/include/zr_vm_parser/semantic.h       # ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE
  - zr_vm_parser/include/zr_vm_parser/type_system.h    # SZrTypeGenericParameterInfo / elementTypes
  - zr_vm_core/include/zr_vm_core/metadata_token.h     # EZrMetadataSignatureNode.GENERIC_INST
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_*.c
---

# 08 · 泛型共享机制（值类型单态化 + 引用共享 + 泛型字典）

> 承接缺口：zr_vm 已有 AST 泛型（`SZrGenericType`/`SZrGenericDeclaration`，含 `EZrGenericParameterKind`
> 的 TYPE/CONST_INT/CONST_UINT/CONST_BOOL，变异性 `EZrGenericVariance`，约束布尔位
> `genericRequiresClass/Struct/New/Owner`）、语义层 `ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE`、
> metadata 签名节点 `GENERIC_INST`、以及 5 个 `backend_aot_c_lowering_generic_*.c` 算子降级。
> **缺**：泛型实例化收集表、约束求解、「泛型参数 → 具体 C 类型」映射、泛型字典/RGCTX 等价物。
> 本文按既定决策（il2cpp 式）补齐这套机制。

## 0. 核心决策（已确认）与三路线对标

| 路线 | 引用类型泛型 | 值类型泛型 | 运行期机制 | zr_vm 取舍 |
|------|------|------|------|------|
| **il2cpp（采用）** | 共享单份 `SZrRawObject*` 代码 | **逐具体类型单态化** | 泛型字典（lazy 填充，对标 RGCTX） | ✅ 最贴合 typed 纯 C 极致性能 + inline value layout |
| mono gsharedvt | 共享 | 共享（签名 wrapper） | MRGCTX + trampoline（重） | ❌ 与「零间接、纯 C」相悖 |
| 全单态化 | 逐类型 | 逐类型 | 无 | △ 体积爆炸；作为「小程序/无字典」可选模式保留 |

**主线一句话**：值类型与 const 泛型 → 编译期单态化（各一份特化 C）；引用类型泛型 → 共享一份
以 `SZrRawObject*` 为参数的代码，差异化的「类型相关信息」（sizeof/字段偏移/GC 位图/具体原型/
被调泛型方法地址）经**每实例的泛型字典**传入。未被静态收集到的实例 → deopt 到解释器动态实例化
（hybrid，对标 hybridclr）。

## 1. 共享性判定（IsShareable，对标 il2cpp GenericSharing::IsShareable）

编译期对每个泛型参数实参分类，决定该实例走「共享」还是「单态化」：

- **引用类型实参**（GC 对象引用）→ 可共享：在 C 里统一表示为 `SZrRawObject*`。
- **值类型实参**（标量 / inline struct / union）→ 不可共享：必须单态化为具体 `ZrLayout_*`。
- **const 泛型实参**（`EZrGenericParameterKind` 的 CONST_INT/UINT/BOOL，对标 C++ NTTP）→
  编译期常量代入，**必然单态化**（不同常量是不同实例）。
- 一个泛型实例「全部实参均为引用类型」→ 整体走共享版本；**任一**实参为值/const → 走单态化版本。
- 判定结果记入实例化表（§3），并作为生成键。

> 与不变量 A 的关系：单态化实例的每个槽仍是单一静态 C 类型；共享实例的「类型未定槽」以
> `SZrRawObject*` 出现（仍是单一静态 C 类型，只是其具体原型来自泛型字典），不破坏确定性。

## 2. 泛型字典（SZrAotGenericDictionary，对标 il2cpp RGCTX / NativeAOT GenericDictionary）

共享代码缺少「具体类型相关」信息，用每实例一份的只读字典补足，由 MethodInfo（`07`§4）携带：

```c
typedef enum EZrAotGenericSlotKind {
    ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT,   /* 具体 SZrTypeLayout*（sizeof/字段偏移/GC 位图，→ 09/11） */
    ZR_AOT_GENERIC_SLOT_PROTOTYPE,     /* 具体类型原型（构造/虚表，→ 反射 10） */
    ZR_AOT_GENERIC_SLOT_METHOD,        /* 被调泛型方法的具体实例入口（→ CALL_TYPED 04） */
    ZR_AOT_GENERIC_SLOT_BOX_TYPE,      /* 装箱目标类型（边界 marshaling 07§6） */
    ZR_AOT_GENERIC_SLOT_SIZEOF,        /* 具体类型字节大小（数组步长/分配） */
} EZrAotGenericSlotKind;

typedef struct SZrAotGenericDictionary {
    TZrUInt32          slotCount;
    const SZrAotGenericSlot *slots;    /* 编译期布局，运行期 lazy 解析填充值 */
} SZrAotGenericDictionary;
```

- **编译期**：为共享方法/类型生成字典**布局**（哪些 slot、各 slot 的 kind 与解析键），
  在使用点（`il2cpp_rgctx_*` 等价的 `ZrAot_GenericSlot_*` 访问宏）只引用 slot 序号，不内联具体类型。
- **运行期**：slot 首次访问时 lazy 解析（对标 il2cpp `Il2CppRGCTXData` 联合体 + 延迟初始化、
  mono `instantiate_info`）。解析结果缓存在该实例的字典实例里。
- **单态化实例无字典**：具体类型在编译期已知并内联，字典为空（零开销）——这是值类型走单态化的收益。

## 3. 泛型实例化收集表（编译期，衔接裁剪 12）

新增「泛型实例化收集」阶段，对标 il2cpp codegen 扫描 + NativeAOT `ExactMethodInstantiationsNode`：

- 数据结构（建议落 `type_system.h` 或独立 `type_instantiation.h`）：
  `SZrGenericInstantiation { baseToken; argCount; argTypeTokens[]; shareKind; cInstanceId; }`，
  以 (baseToken, 实参类型签名) 去重（对标 mono `MonoGenericInst` HashSet、roslyn TypeSpec/MethodSpec）。
- **收集来源**（静态可达性，衔接 `12` 可达性分析）：
  1. 源码中显式出现的泛型使用（`List<Foo>`、`f<Bar>()`）；
  2. 传递闭包：实例 A 内部用到 `B<其类型参数>` → 递归收集（对标 mono full-AOT transitive closure）；
  3. 虚/接口调用的所有覆盖实例；
  4. manifest（`12`）显式声明保留的实例（对标 link.xml 预声明动态实例）。
- 每个收集到的实例分配 `cInstanceId`，单态化实例据此发射 `ZrLayout_<cInstanceId>` 与特化函数，
  共享实例据此发射泛型字典实例。
- **未收集到的实例**（运行期才确定的泛型参数，如反射构造）→ 不在 AOT 产物中，触发 deopt（§6）。

## 4. 约束求解（编译期）

把 AST 的约束布尔位（`genericRequiresClass/Struct/New/Owner`、`genericTypeConstraints[]`、
`genericRequiredOwnershipQualifier`）形式化为一次编译期检查：

- `requiresClass` → 实参必须引用类型（且影响共享性判定 §1）；`requiresStruct` → 必须值类型（→ 单态化）。
- `genericTypeConstraints[]` → 实参原型必须实现这些协议/继承这些基类（复用 `ZrCore_ObjectPrototype_ImplementsProtocol` 的编译期版本）。
- `requiresNew` → 实参须有可达无参构造（影响裁剪保留 `12`）。
- `requiresOwner` / `genericRequiredOwnershipQualifier` → 与 using 计划的所有权种类对接（`05`§1）。
- 不满足 → 编译期报错（不变量 A 的类型完备性，对标 `01`§1）。

## 5. 生成形态（对标 il2cpp 生成 C/C++）

- **单态化值类型泛型**：每个 `cInstanceId` 发一份 `struct ZrLayout_<id>`（连续布局，`02`）+
  特化函数 `zr_fn_<base>__<id>(...)`，内部全是裸 C（`07` 寄存器模型，无字典）。
- **共享引用类型泛型**：一份 `zr_fn_<base>__shared(SZrState*, const SZrAotGenericDictionary*, /* 形参以 SZrRawObject* 表达类型未定槽 */)`，
  类型相关处用 `ZrAot_GenericSlot_TypeLayout(dict, k)` 等取信息。
- **CALL_TYPED 到泛型方法**：单态化 → 直接 C 调具体特化函数；共享 → 取字典 METHOD slot 调用（`04`§2）。
- **数组/sizeof/装箱**：值类型实例编译期常量；共享实例经字典 SIZEOF/BOX_TYPE slot。

## 6. hybrid 兜底：未预生成实例的 deopt（对标 hybridclr）

- typed 路径只覆盖**静态收集到**的实例（§3）。运行期出现未收集实例（反射 `MakeGenericType` 等）→
  在 typed 调用边界 deopt 到解释器（`04`§6 / `05`§5），由解释器**动态实例化**（dynamic 路径仍持有
  完整泛型定义元数据 `11`）。
- 这正是「全静态 il2cpp」与「可动态 mono」的折中：热路径全静态零开销，长尾动态能力由解释器承载，
  与 zr_vm 既有双路径一致（`01`§3）。
- 可选「full-AOT 模式」（对标 mono full-AOT / iOS）：manifest 声明禁止 deopt → 收集不全即编译期报错。

## 7. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 08-S1 | 泛型实例化收集表 + 去重 + 共享性判定（§1/§3） | 单测：典型程序的实例集合与 shareKind 正确、去重无遗漏 |
| 08-S2 | 约束求解编译期检查（§4） | 违约实例编译期报错；满足约束放行 |
| 08-S3 | 单态化值类型/const 泛型：发 `ZrLayout_<id>` + 特化函数（§5） | 值类型泛型 AOT 为纯 C，结果对齐解释器 |
| 08-S4 | 泛型字典布局 + lazy 解析 + 共享引用类型代码生成（§2/§5） | 多个引用类型实例共享一份代码；字典解析正确 |
| 08-S5 | 泛型 `CALL_TYPED` 单态/共享两形态（§5） | 跨泛型方法调用 AOT 与解释器一致 |
| 08-S6 | 未收集实例 deopt 到解释器动态实例化（§6） | 反射构造的泛型运行正确（经 deopt） |
| 08-S7 | full-AOT 模式：收集不全编译期报错（§6 可选） | 开关生效；缺失实例被诊断 |

## 8. 不变量校验

- **A 确定性**：单态化实例全槽单一静态类型；共享实例类型未定槽统一为 `SZrRawObject*`，具体性经字典，不引入运行期类型分支于纯标量段。
- **B 纯降级**：单态化路径零字典零 VM 调用；共享路径仅在字典 slot 首次解析处有受控 runtime 调用（纳入 `04`§4 白名单）。
- **C 单一真相**：字典 TYPE_LAYOUT/SIZEOF/GC 位图全部来自唯一 `SZrTypeLayout`（`02`/`11`），不另算偏移。
- **D 环境隔离**：泛型字典经 `SZrAotMethodInfo`（`07`）携带，不进函数体语句流；共享函数体仍是寄存器 + 纯 C。
