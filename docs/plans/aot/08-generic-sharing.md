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
  - zr_vm_parser/include/zr_vm_parser/generic_instantiation.h # 08-S1 instance table / shareKind
  - zr_vm_parser/include/zr_vm_parser/writer.h # 08-S7A requireFullAot writer option；08-S7C..08-S7K manifest generic preserve writer roots + TypeSpec/generic-instantiation/MethodSpec binding carrier
  - zr_vm_library/include/zr_vm_library/project.h # 11-S7F manifest aotMode project model
  - zr_vm_library/src/zr_vm_library/project/project_aot_options.c # 11-S7F .zrp aotMode parser
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h # 11-S7G manifest aotMode -> AOT writer option bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c # 11-S7G manifest aotMode -> requireFullAot injection helper
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.h # 08-S7C..08-S7K/11-S7N..11-S7V/12-S4H..12-S4N generic preserve writer root carrier
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c # 08-S7C..08-S7K/11-S7N..11-S7V/12-S3A..12-S3F/12-S4H..12-S4N generic preserve writer root bridge + current-module TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding
  - zr_vm_language_server_extension/schemas/zrp.schema.json # 11-S7F aotMode schema parity
  - zr_vm_parser/src/zr_vm_parser/generic_instantiation.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c # 08-S3 closed generic inline layout finalization
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c # 08-S3 exact closed layout lookup
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h # 08-S4 generic dictionary ABI
  - zr_vm_library/include/zr_vm_library/aot_runtime.h # 08-S4 lazy dictionary slot API; 11-S4E TYPE_LAYOUT/SIZEOF accepts SZrMetadataRuntime
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c # 08-S4 lazy dictionary slot resolution; 11-S4E layout slots resolve through metadata runtime
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.h # 08-S3 AOT markers/layouts
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.h # 08-S4 shared-reference codegen
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c # 11-S4E metadataRuntime-aware TYPE_LAYOUT macro/shared signature
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 08-S7E full-AOT manifest generic TypeSpec closure gate；08-S7F generic instantiation manifest diagnostics；08-S7G full-AOT generic-instantiation closure gate；08-S7K MethodSpec manifest diagnostics/full-AOT closure alternative
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c # 08-S5/S6A/S7A/S7B generic CALL_TYPED lowering
  - zr_vm_core/include/zr_vm_core/metadata_token.h     # EZrMetadataSignatureNode.GENERIC_INST
  - zr_vm_core/include/zr_vm_core/reflection.h         # 08-S6B..S6V dynamic generic carrier/object/interpreter/method-context/execution/definition-object API
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h   # 11-S5A GenericParam owner-range view consumed by 08-S6V
  - zr_vm_core/include/zr_vm_core/function.h           # 08-S6O/S6R type+method-context-aware synchronous known-VM call boundary; 08-S6U AOT-compatible flat function-graph resolver API
  - zr_vm_core/include/zr_vm_core/call_info.h          # 08-S6N/S6Q GC-visible type/method generic call-context carriers
  - zr_vm_core/src/zr_vm_core/call_info.c              # 08-S6N/S6Q call-context reset on call-info initialization
  - zr_vm_core/src/zr_vm_core/function.c               # 08-S6N/S6Q reuse reset; 08-S6O/S6R PreCall-to-Execute type/method-context injection
  - zr_vm_core/src/zr_vm_core/function_graph.c         # 08-S6U AOT-order root/constant/child graph flat-index lookup with identity dedup and bounded growth
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h # 08-S6N/S6Q hot exact-args VM call-context reset
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c             # 08-S6N/S6Q active call-info type/method-context marking
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c            # 08-S6N/S6Q compacting-GC call-context forwarding rewrite
  - zr_vm_core/src/zr_vm_core/reflection_generic_instance.c # 08-S6B..S6L AOT/deopt identity + public revalidation
  - zr_vm_core/src/zr_vm_core/reflection_generic_type_object.c # 08-S6H..S6K/10-S4Z29..Z32 type objects; 08-S6P/10-S4Z37 MethodSpec context; 08-S6V/10-S4Z43 generic method definition object
  - zr_vm_core/src/zr_vm_core/object/object_call.c     # 08-S6O/S6R type+method-context object call reusing existing pin/anchor/result path
  - zr_vm_core/src/zr_vm_core/object/object_call_internal.h # 08-S6O/S6R internal resolved-function context call boundary
  - zr_vm_core/src/zr_vm_core/reflection_interpreter_generic_instance.c # 08-S6L..S6S/10-S4Z33..Z40 reference/value instance + type/method call context + VM execution
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c # 08-S6U/10-S4Z42/11-S2E MethodDef.functionIndex -> local VM function binding
  - zr_vm_core/src/zr_vm_core/metadata_runtime_generic_params.c # 11-S5A exact GenericParam owner range view
  - zr_vm_core/src/zr_vm_core/metadata_runtime_type_node_binding.c # 08-S6K/10-S4Z32/11-S4BO local signature-node -> type record binding
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_*.c
  - tests/parser/test_generic_constraints.c               # 08-S2 constraint acceptance
  - tests/parser/test_aot_c_generic_monomorphization.c    # 08-S3 AOT acceptance
  - tests/parser/test_aot_c_generic_reference_sharing.c    # 08-S4 AOT acceptance; 11-S4E metadata-runtime layout resolver regression
  - tests/parser/test_aot_c_generic_call_typed.c           # 08-S5/08-S6A/08-S7A/S7B generic CALL_TYPED acceptance
  - tests/module/test_reflection_dynamic_generic_instance.c # 08-S6B..S6V runtime TypeSpec/MethodSpec object, context, value semantics, definition object, and VM execution coverage
  - tests/module/test_reflection_dynamic_generic_instance_assertions.h # 08-S6K focused object-graph assertion support
  - tests/module/test_reflection_dynamic_generic_instance_interpreter.h # 08-S6L..S6S focused reference/value instance, context, substitution, copy, and execution scenarios
  - tests/module/test_reflection_dynamic_generic_method_context.h # 08-S6P..S6V/10-S4Z37..Z43 MethodSpec context/call-info/GC/execution plus generic method definition object scenarios
  - tests/cli/test_cli_project_incremental.c               # 11-S7G manifest full-AOT writer option bridge
  - tests/cli/test_cli_aot_writer_options.c                # 08-S7C..08-S7K/11-S7N..11-S7V/12-S3A..12-S3F/12-S4H..12-S4N/12-S8H..12-S8I generic preserve writer roots + TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding + full-AOT gates
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
- 11-S4E 加固后，TYPE_LAYOUT/SIZEOF slot 的 lazy 解析不再读取 metadata function prototype layout cache，
  而是接收当前模块的 `SZrMetadataRuntime*` 并通过
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 读取 11 的 code-registration layout registry。
- **单态化实例无字典**：具体类型在编译期已知并内联，字典为空（零开销）——这是值类型走单态化的收益。

## 3. 泛型实例化收集表（编译期，衔接裁剪 12）

新增「泛型实例化收集」阶段，对标 il2cpp codegen 扫描 + NativeAOT `ExactMethodInstantiationsNode`：

- 数据结构（08-S1 已落 `generic_instantiation.h/.c`）：
  `SZrGenericInstantiationTable` 持有 `SZrGenericInstantiationRecord { baseToken; arguments; shareKind; cInstanceId; }`，
  以 (baseToken, 实参类型签名 + 已解析 type shape) 去重（对标 mono `MonoGenericInst` HashSet、roslyn TypeSpec/MethodSpec）。
  默认 shape 从 `EZrValueType` 推断；当编译期已知 source class/struct 区分时，调用
  `ZrParser_GenericInstantiationTable_GetOrAddResolved()` 显式传入 reference/value shape，避免把 inline struct 当作引用共享。
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
| 08-S1 | 泛型实例化收集表 + 去重 + 共享性判定（§1/§3） | ✅ 2026-06-24 完成：单测覆盖 reference share、value monomorphize、去重与 resolved class/struct shape |
| 08-S2 | 约束求解编译期检查（§4） | ✅ 2026-06-24 完成：named/class/struct/owner/specific owner 既有覆盖，新增 new() 约束独立验收 |
| 08-S3 | 单态化值类型/const 泛型：发 `ZrLayout_<id>` + 特化函数（§5） | ✅ 2026-06-24 完成：`Pair<int,int>` AOT 发 closed inline layout、monomorphized marker/特化 wrapper，shared library 结果 81 对齐解释器 |
| 08-S4 | 泛型字典布局 + lazy 解析 + 共享引用类型代码生成（§2/§5） | ✅ 2026-06-24 完成：ABI v4 发布 `SZrAotGenericDictionary`/slot/cache，runtime lazy 解析 TYPE_LAYOUT/SIZEOF；`Box<RefA>`/`Box<RefB>` 发两份字典但共享一份 `zr_fn_box__shared`。2026-06-25 通过 11-S4E 加固：TYPE_LAYOUT/SIZEOF slot 现在经 `SZrMetadataRuntime` 读取 code-registration layout registry，不再 fallback 到 prototype layout cache |
| 08-S5 | 泛型 `CALL_TYPED` 单态/共享两形态（§5） | ✅ 2026-06-24 完成：METHOD slot lazy helper、共享 METHOD-slot 调用形态、单态 wrapper direct marker、源级引用泛型 `CALL_TYPED` METHOD-slot callsite 接入，并以共享库 AOT 执行结果对齐解释器 |
| 08-S6 | 未收集实例 deopt 到解释器动态实例化（§6） | 🚧 2026-07-19 部分完成：08-S6A 已覆盖 shared generic `CALL_TYPED` METHOD slot 缺失时回退解释器；08-S6B 已为元数据有效的现有 TypeSpec 建立 AOT layout / interpreter-deopt 路由载体；08-S6C 已支持 open base + primitive/direct type-token 实参请求精确匹配已有 TypeSpec，并把未收集组合标记为携带借用实参视图的 interpreter-deopt 请求；08-S6D 已支持 TypeSpec token 表示的 nested closed-generic 实参结构匹配；08-S6E 已支持递归 `ARRAY(rank, element)` 实参身份与深度门禁；08-S6F 已支持递归 `TUPLE`、`OWNERSHIP`、`NULLABLE` 实参身份；08-S6G 已支持命名 `UNION(valueType, nameOffset, children)` 实参身份；08-S6H/10-S4Z29 已把 request-resolved AOT/deopt carrier 物化为 GC 管理的 public constructed-generic type object，并递归复制实参对象图；08-S6I/10-S4Z30 已提供 open base + arguments 的单调用 public `MakeGenericTypeObject` 入口；08-S6J/10-S4Z31 已支持把现有 TypeSpec 的 token-only primitive/direct TypeDef/TypeRef 实参物化进同一 public type object；08-S6K/10-S4Z32/11-S4BO 已递归物化 token-only nested generic/array/tuple/ownership/nullable/union 实参，并以完整本地签名节点跨度绑定真实 TypeSpec record；08-S6L/10-S4Z33 已为未收集的 reference-class 请求建立首个解释器实例对象上下文：普通对象复用 open class prototype，并持有深拷贝 public generic type object，AOT route fail closed；08-S6M/10-S4Z34 已消费既有 11-S5 GenericParam owner/index view，把实例 context 中对应 argument type object 作为解释器参数替换结果返回，并校验 same-runtime/open-base/arity；08-S6N/10-S4Z35 已把实例 generic type context 绑定到 VM `SZrCallInfo` 的 GC-visible `SZrTypeValue` carrier，普通/native/hot/tail 重用路径会清零，compact GC 会标记并重写转发地址，并可通过 call-info 按 GenericParam owner/index 取回 concrete argument type object；08-S6O/10-S4Z36 已执行带实例 context 的 resolved VM 方法；08-S6P/10-S4Z37 已物化 MethodSpec context；08-S6Q/10-S4Z38 已将 MethodSpec context 独立绑定到 call info，支持 method-owned GenericParam 替换并通过 compact full GC；08-S6R/10-S4Z39 已执行带 MethodSpec context 的 caller-resolved VM 泛型函数，覆盖显式参数、结果与活动帧替换；08-S6S/10-S4Z40 已让未收集 struct 请求使用既有 boxed dynamic struct 表示，保留 context、GenericParam 替换、深复制隔离和 resolved VM 方法执行。本地 metadata runtime 的请求匹配和 token-only object 已覆盖全部当前编码复合节点；跨模块 TypeSpec 搜索、method-token 到函数解析和脚本级 `MakeGenericMethod` 仍未完成 |
| 08-S6O | deopt generic type instance 的 resolved VM 方法执行 | ✅ 2026-07-18 完成：验证 runtime + open owner + fixed arity，复用现有对象调用的 pin/stack-anchor/argument/result 路径，在 `PreCall` 与 `Execute` 之间注入 S6N context；真实 VM 字节码方法从活动帧解析 GenericParam[1]，接收并返回显式 int64 参数。MethodSpec 自身泛型方法、dynamic value type 和跨模块 identity 仍开放 |
| 08-S6P | MethodSpec generic method context object | ✅ 2026-07-18 完成：消费既有 11-S5 MethodSpec signature/argument view，复用 token-only recursive metadata-node 对象化器，发布 GC-managed methodSpec/method token、完整 uint64 signature hash、runtime 与 primitive + TypeRef arguments 上下文对象；尚未绑定到 call info 或执行 MethodSpec |
| 08-S6Q | MethodSpec call-info context + method GenericParam 替换 | ✅ 2026-07-18 完成：新增独立 GC-visible method context carrier，bind/get/parameter resolver 校验 same runtime、underlying method owner、generic range 与 arity；普通/native/hot/tail 重用路径清零，compact full GC 标记并重写；MethodSpec execution 仍开放 |
| 08-S6R | caller-resolved MethodSpec VM function execution | ✅ 2026-07-18 完成：验证 MethodSpec/method GenericParam 精确 arity 与非 native/non-vararg fixed VM function，复用 object-call pin/anchor/argument/result 路径，在 `PreCall` 后注入独立 method context；真实字节码函数解析 GenericParam[1] 并返回 int64 109 |
| 08-S6S | interpreter generic boxed value instance | ✅ 2026-07-19 完成：interpreter-deopt + open struct prototype 进入既有 `ZR_OBJECT_INTERNAL_TYPE_STRUCT` dynamic value path；context/GenericParam 替换可用，`ZrCore_Value_Copy` 克隆后字段隔离，resolved VM 方法执行返回 int64 117；不生成新 AOT layout |
| 08-S6T | bound provider generic TypeSpec identity | ✅ 2026-07-19 完成：消费 requester/provider 既有 binding，按 canonical signature exact bytes 解析 provider-owned route/token/layout；合法 TypeSpec RID remap 通过，stale same-hash bytes fail closed |
| 08-S6U | local MethodSpec method-token/VM-function auto resolution | ✅ 2026-07-19 完成：MethodSpec -> MethodDef row -> AOT-order flat function index -> fixed VM function 自动绑定并执行；root/constant/child 顺序和 identity 去重对齐 AOT function table，超大/越界/畸形 metadata fail closed |
| 08-S6V | public generic method definition object | ✅ 2026-07-19 完成：MethodDef GenericParam owner range 严格物化为 GC-managed definition/parameter objects，方法名和参数名读取 zrp string pool，损坏 range/owner/index fail closed；实际 type-argument -> MethodSpec 构造仍开放 |
| 08-S7 | full-AOT 模式：收集不全编译期报错（§6 可选） | 🚧 2026-06-25 部分完成：08-S7A 已覆盖 C writer full-AOT 开关生效，静态收集到的 shared generic `CALL_TYPED` 不再生成 missing-instance deopt；08-S7B 已让 full-AOT 已闭合 shared generic `CALL_TYPED` 直接调用静态 AOT method entry，不再保留 METHOD slot null runtime branch；11-S7F 已让 `.zrp` manifest 能声明 `aotMode: "full-aot"` 并进入 project model；11-S7G 已让 project manifest policy 可注入 `SZrAotWriterOptions.requireFullAot`；11-S7H/12-S8G 已让 CLI `--emit-aot-c` 项目编译入口发射 AOT C 并消费 manifest full-AOT policy；08-S7C/11-S7N/12-S4H 已把 manifest generic preserve target+arguments 注入 AOT writer options 并在 generated C 清单中输出；08-S7D/11-S7O/12-S4I 已在当前函数 metadata 存在匹配 `GENERIC_INST` `TYPE_SPEC` 时把 generic preserve root 绑定到 TypeSpec/signature token/hash；08-S7E/11-S7P/12-S8H 已让 full-AOT writer 拒绝未绑定 TypeSpec 的 manifest generic preserve root；08-S7F/11-S7Q/12-S3A/12-S4J 已把已绑定 TypeSpec 的 manifest generic root 物化为 generic instantiation identity（baseToken/cInstanceId/shareKind）并输出 manifest 诊断；08-S7G/11-S7R/12-S3B/12-S8I 已让 full-AOT writer 拒绝 TypeSpec-only generic preserve root，要求同时具备 generic instantiation identity；08-S7H/11-S7S/12-S3C/12-S4K 已在当前模块存在同名 `TYPE_REF` metadata 时让 generic instantiation base token 使用 open generic base token，并在缺失 TypeRef 时回退 closed TypeSpec；08-S7I/11-S7T/12-S3D/12-S4L 已支持 `GENERIC_INST(TYPE_DEF target, args...)` TypeSpec，并让 current-module TypeDef base 使用 open `TYPE_DEF` token；08-S7J/11-S7U/12-S3E/12-S4M 已在 manifest generic root 缺失 TypeSpec 但存在同名 open `TYPE_DEF`/`TYPE_REF` metadata 时合成 current-module TypeSpec/signature binding 并继续物化 generic instantiation identity；08-S7K/11-S7V/12-S3F/12-S4N 已把 manifest generic method root 绑定到现有 `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 形态签名，并让 full-AOT closure gate 接受 MethodSpec-bound generic method root；跨模块真实 generic instantiation roots、反射构造闭包和完整 mark-and-sweep closure 仍需后续 |

> 2026-07-19 02:56:02 +08:00 状态补记：08-S6 主行末尾的“跨模块 TypeSpec 搜索、method-token 到函数解析仍未完成”
> 已分别由 08-S6T 与 08-S6U 关闭；当前该阶段剩余主缺口为脚本对象级 `MakeGenericMethod` 与 full-AOT reflection closure。

> 2026-07-19 03:53:01 +08:00 状态补记：08-S6V 已关闭 public 开放泛型方法定义对象与 GenericParam 列表物化；
> 当前仍开放 type-argument 到既有 MethodSpec 的精确匹配、constructed method object/脚本 `MakeGenericMethod` 和
> full-AOT reflection closure。

## 8. 不变量校验

- **A 确定性**：单态化实例全槽单一静态类型；共享实例类型未定槽统一为 `SZrRawObject*`，具体性经字典，不引入运行期类型分支于纯标量段。
- **B 纯降级**：单态化路径零字典零 VM 调用；共享路径仅在字典 slot 首次解析处有受控 runtime 调用（纳入 `04`§4 白名单）。
- **C 单一真相**：字典 TYPE_LAYOUT/SIZEOF/GC 位图全部来自唯一 `SZrTypeLayout`（`02`/`11`），不另算偏移。
- **D 环境隔离**：泛型字典经 `SZrAotMethodInfo`（`07`）携带，不进函数体语句流；共享函数体仍是寄存器 + 纯 C。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-07-19 03:53:01 +08:00 · 08-S6V / 10-S4Z43 / 11-S5A public generic method definition object ·
  状态：08/10/11 交叉子切片完成；完整 08-S6、10-S4 与 11-S5 仍为部分完成。完成项目：metadata runtime
  公开只读 `SZrMetadataRuntimeGenericOwnerView`，返回 TypeDef/MethodDef owner record、row 与精确
  `firstGenericParamIndex/genericParamCount`，失败始终清 output。反射层新增
  `ZrCore_Reflection_BuildGenericMethodDefinitionObject()`，严格要求 MethodDef 非零泛型参数范围，按声明数量逐项验证
  owner record、物理 row index 与 parameter index，再物化 method/parameter reflection objects；方法名和参数名来自
  zrp string pool，token、runtime、flags、signature 坐标、constraint range 与参数数组均保留。RED 先为两个 public API
  unresolved symbols，首轮 GREEN 为 metadata query 25/0、dynamic reflection 25/0；review RED 再以真实 string pool
  暴露占位名，最终 GREEN 25/0。GCC 11.4、Clang 14.0、MSVC 19.44 聚焦 CTest 各 6/6，GC 各 66/0、
  指令执行各 31/0、指令表各 95/0；本切片实现源诊断为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6v-10-s4z43-11-s5a-generic-method-definition-object.md`。
  分层记录：`docs/plans/aot/07-12-codegen/2026-07-19-08-s6v-10-s4z43-11-s5a.md`。
  备注：不新增 zrp row、不改变 registration ABI、不创建 MethodSpec，也不声明 constructed method object、脚本
  `MakeGenericMethod`、跨模块 method binding 或 full-AOT reflection closure 完成。

- 2026-07-19 02:56:02 +08:00 · 08-S6U / 10-S4Z42 / 11-S2E local MethodSpec method-token/VM-function auto resolution ·
  状态：三阶段交叉子切片完成；完整 08-S6、10-S4 与 11-S2 仍为部分完成。完成项目：新增
  AOT-compatible `ZrCore_Function_ResolveGraphFunctionByFlatIndex()`，按 root、constant-referenced function、child
  function 的深度优先顺序解析 flat index，并按指针或 AOT function identity 去重；visited storage 按实际图节点
  有界增长，不按不可信 index 预分配。metadata runtime 新增 interpreter MethodDef binding view，要求唯一 local
  `MEMBER_DEF` row/record、有效 `functionIndex` 与 non-native instruction-backed VM function。public MethodSpec invoke
  自动读取 signature 的 underlying method token、绑定函数并复用 S6R 的 context/arity/pin/anchor/result 路径。
  RED 为 MSVC 两个缺失 public API unresolved symbols；GREEN 后动态泛型反射 24/0。边界覆盖 root/constant/child
  顺序、constant+child 重复去重、index 2 越界、`UINT32_MAX-1` 超大索引、错误 token/arity 和损坏 MethodDef index，
  均 fail closed 并清 result。最终 WSL GCC 11.4、Clang 14.0、MSVC 19.44 聚焦 CTest 各 5/5，GC 各 66/0、
  指令执行各 31/0、指令表各 95/0；本切片源诊断为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6u-10-s4z42-11-s2e-methodspec-method-token-vm-function-resolution.md`。
  备注：不改变 zrp/registration ABI，不生成 MethodSpec 专用 code slot，不解析跨模块方法，也不声明脚本级
  `MakeGenericMethod` 或 full-AOT reflection closure 完成。

- 2026-07-19 01:39:29 +08:00 · 08-S6T / 10-S4Z41 / 11-S6J bound provider generic TypeSpec identity ·
  状态：三阶段交叉子切片完成；完整 08-S6、10-S4 与 11-S6 仍为部分完成。完成项目：新增 public
  `ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider()`，只消费现有
  `SZrMetadataTokenBinding`，要求 requester/provider 为不同模块，校验 ref token/signature/hash、provider module
  signature hash、resolved TypeSpec/Signature records，并逐字节比较两侧非空 canonical TypeSpec signature；成功后
  仍由 provider runtime 的既有动态泛型 resolver 决定 AOT/deopt route，因此输出保留 provider RID 9、Signature
  RID 29 与 provider layout。runtime binding compatibility 仅在两侧 metadata token 都是 TypeSpec 且两侧 paired
  token 都是 Signature 时放行 RID 重映射，版本/module hash/signature hash/layout 校验不变。unbound requester、
  wrong provider、same-module、畸形 expected identity、非法 token 及绑定后签名字节漂移均 fail closed 并清 output。RED/GREEN：先由不完整
  手工 fixture 暴露 entity signature range 缺失（生产规则未放宽）；有效绑定后 RED 为 reflection 24/1 与
  compatibility 17/1（metadata token mismatch）；放行规范 TypeSpec remap 后 17/0、24/0；再以相同缓存 hash 下
  int64->uint64 provider blob 漂移得到 24/1，增加 exact blob revalidation 后最终 24/0。WSL GCC 11.4、Clang 14.0、
  MSVC 19.44 聚焦 CTest 各 4/4；三工具链 GC 各 66/0、指令执行各 31/0、指令表各 95/0；变更文件诊断扫描为空。
  产出：`tests/acceptance/2026-07-19-aot-08-s6t-10-s4z41-11-s6j-bound-provider-generic-typespec-identity.md`。
  备注：无全局 TypeSpec registry、无 metadata/zrp 格式变化、无 layout 合成；method-token/function 自动解析与
  script-level generic reflection 仍开放，故不关闭完整 08-S6。

- 2026-07-19 00:13:22 +08:00 · 08-S6S / 10-S4Z40 interpreter generic boxed value instance ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：
  `ZrCore_Reflection_NewInterpreterGenericInstanceObject()` 在 route 为 interpreter-deopt 时接受 open struct
  prototype，并将实例初始化为既有 `ZR_OBJECT_INTERNAL_TYPE_STRUCT` boxed dynamic value；class 路径和 AOT route
  门禁不变。context getter、instance GenericParam resolver 与 resolved VM method invoke 同时接受 class/struct
  两种受支持实例。测试确认 struct 持有 generic type context、GenericParam[1] 替换为 TypeDef，
  `ZrCore_Value_Copy()` 经既有 `ZrCore_Object_CloneStruct()` 生成不同对象且 payload 17/29 互不污染，并在活动
  type context 下执行真实 VM identity 方法返回 int64 117。RED/GREEN：RED 为 23/1，唯一失败是 struct 创建
  返回 null；GREEN 为 23/0。隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 聚焦 CTest 各 3/3；三工具链
  共享 GC 各 66/0、指令执行各 31/0；变更实现/测试无编译告警。产出：
  `tests/acceptance/2026-07-19-aot-08-s6s-10-s4z40-interpreter-generic-value-instance.md`。备注：本切片复用
  dynamic struct/value-copy/GC 语义，不生成或猜测 typed/AOT layout；未改 metadata 格式/runtime API，故不新增
  11 状态。跨模块 identity、method-token/function 解析与脚本级 generic reflection 仍开放。

- 2026-07-18 23:46:42 +08:00 · 08-S6R / 10-S4Z39 caller-resolved MethodSpec VM function execution ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增 public
  `ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction()`，读取既有 MethodSpec view，要求
  underlying method 的 GenericParam[0..N) 与 MethodSpec arguments 精确闭合，并只接受 non-native、non-vararg、
  fixed-arity VM function。函数/对象调用内部边界扩展为两个可选且独立的 type/method contexts；旧 type-only API
  保持兼容，普通调用仍传空 contexts。调用复用既有 callable/argument pin、stack anchor、argument staging 与
  result restore，在 `PreCall` 后、`Execute` 前复制 pinned method context。真实 VM identity function 在 trace
  observer 中确认 type context 为空、MethodSpec context 存在，解析 method GenericParam[1] 为 TypeRef，并返回
  int64 109；错误 token/explicit arity fail closed 并清 result。RED 为 1 个 MSVC unresolved symbol；GREEN 为
  22/0。隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 聚焦 CTest 各 3/3；三工具链共享 GC 各 66/0、
  指令执行各 31/0。新增实现无 GCC/Clang 告警；`object_call.c` 保留 S6N/S6O 基线已有的 2 个 Clang
  unused-helper 告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6r-10-s4z39-methodspec-vm-function-execution.md`。备注：API 接受
  caller-resolved function，不声称完成 method-token -> function 解析或脚本级 `MakeGenericMethod`；未改 metadata
  格式/runtime API，故不新增 11 阶段状态。

- 2026-07-18 23:13:13 +08:00 · 08-S6Q / 10-S4Z38 MethodSpec call-info context + method GenericParam substitution ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：在 `SZrCallInfo` 新增与
  type context 独立的 GC-visible `interpreterGenericMethodContext`；public bind 从 S6P builder 创建 MethodSpec
  context，get 返回活动 reflection object，method parameter resolver 复用 11-S5 GenericParam owner/index view，
  同时验证 same runtime、`genericMethodToken`、owner generic range、context arity 与 parameter index。失败 bind
  清空旧 context；普通/native/VM/hot/tail call-info 初始化和重用路径成对清零 type/method carriers；活动帧在
  compact full GC 中标记并重写 method context。RED/GREEN：RED 为 3 个 MSVC unresolved symbols；GREEN 为
  21/0。中间 21/2 暴露测试夹具声明 `MemberDef RID 7` owner 却缺失 MethodDef table；夹具改为真实 TypeDef +
  MethodDef + generic range 后通过，生产 ZRP 校验未放宽。隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44
  聚焦 CTest 各 3/3；三工具链共享 GC 各 66/0、指令执行各 31/0；GCC/Clang 本阶段文件无告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6q-10-s4z38-methodspec-callinfo-context.md`。备注：本切片只建立
  MethodSpec call-frame context 与替换规则，尚未执行 MethodSpec；未改 metadata 格式/runtime API，故不新增
  11 阶段状态。

- 2026-07-18 22:36:52 +08:00 · 08-S6P / 10-S4Z37 MethodSpec generic method context object ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增 public
  `ZrCore_Reflection_BuildMethodSpecGenericContextObject()`，直接消费 11-S5
  `ReadMethodSpecSignatureView/ReadMethodSpecGenericArgumentView`，不引入平行 MethodSpec 表。对象携带
  `metadataToken`、underlying `genericMethodToken`、完整 uint64 `genericSignatureHash`、same runtime、
  argument count/array 与 generic-method flags；primitive uint64 与 TypeRef 参数复用 S6K 递归 metadata-node
  对象化路径。非 MethodSpec token/null state fail closed。RED/GREEN：RED 为 1 个 MSVC unresolved symbol；
  GREEN 为 20/0；中间 20/1 暴露测试 fixture 违反 module runtime 非空 code-registration 契约，改为零初始化
  registration 后通过，生产校验未放宽。隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 聚焦
  CTest 各 3/3；`reflection_generic_type_object.c` 无 GCC/Clang 告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6p-10-s4z37-methodspec-generic-context-object.md`。
  备注：本切片只物化 MethodSpec context，尚未绑定 call info、替换 method GenericParam 或执行 MethodSpec；
  未改 metadata 格式/API，因此不新增 11 阶段状态。

- 2026-07-18 22:13:24 +08:00 · 08-S6O / 10-S4Z36 interpreter generic instance resolved VM method execution ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增 public
  `ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod()`，仅接受 same-runtime/open-owner、
  非 vararg、receiver + fixed arguments 精确等于 parameterCount 的 VM function；wrong runtime/owner/arity 会
  fail closed 并清空 result。调用复用现有 object-call 的 callable/receiver/argument pinning、stack anchor、
  receiver 与 result 恢复，函数边界仅在 `PreCall` 与 `Execute` 之间复制已固定的 S6N context。
  真实 VM 字节码方法在活动帧 trace observer 内解析 GenericParam[1] 为 TypeDef 实参，并原样返回
  显式 int64 参数 73。RED/GREEN：RED 为 1 个 MSVC unresolved symbol；GREEN 为 19/0。隔离 WSL
  GCC 11.4、Clang 14.0 与 MSVC 19.44 最终聚焦 CTest 各 3/3；三工具链共享层 GC 各 66/0、
  指令执行各 31/0。变更实现无新 GCC/Clang 告警；`object_call.c` 仅保留 S6O 前已存在的
  2 个 Clang unused-helper 告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6o-10-s4z36-interpreter-generic-instance-vm-method-execution.md`。
  备注：本切片首次真实执行 deopt generic type instance 的 resolved VM 方法；MethodSpec 自身
  泛型参数/方法物化、dynamic value type 与 cross-module identity 仍开放；未改 metadata 格式/API。

- 2026-07-18 20:55:12 +08:00 · 08-S6N / 10-S4Z35 interpreter generic call-info context carrier ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：`SZrCallInfo` 新增 GC-visible
  `SZrTypeValue interpreterGenericContext`；三个 public API 负责从 S6L 实例绑定 context、回读 type object，
  以及复用 S6M owner/index 校验解析 concrete argument。非 VM call-info 和非 generic instance fail closed；
  ordinary/native/VM/hot exact-args/tail-call 初始化或重用路径全部清零 carrier，防止跨调用泄漏。
  活动 call-info 已纳入 GC mark，compact/full GC 会重写 forwarding address。RED/GREEN：RED 为 3 个新 API
  产生 3 个 MSVC unresolved symbols；GREEN 为聚焦目标 18/0，其中 full GC 后仍可从 call-info 解析第 2 个
  TypeDef 实参。隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 三项 CTest 各 3/3；三工具链共享层
  GC 测试各 66/0、指令执行测试各 31/0，变更实现文件无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6n-10-s4z35-interpreter-generic-callinfo-context.md`。
  备注：本切片建立 call-frame context 与 GC 生命周期，但尚未执行 uncollected generic method；
  未改动 metadata 格式/API，因此不新增 11 阶段状态。

- 2026-07-18 19:48:30 +08:00 · 08-S6M / 10-S4Z34 interpreter GenericParam substitution lookup ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增 public
  `ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject()`，先消费 11-S5 的 owner+parameterIndex
  GenericParam view，再校验实例拥有的 type context 与同一 metadata runtime、open base token、argument count/array
  一致，最后按 metadata `parameterIndex` 返回已有 concrete argument type object；不复制参数表、不按名称猜测、
  不接受 prototype object、错误 owner 或越界 index。测试职责拆分到 122 行 interpreter header，主场景文件由
  980 行降到 944 行。RED/GREEN：RED 为新入口缺失导致 1 个 MSVC unresolved symbol；GREEN 为 17/0。
  隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 三项 CTest 均为 3/3；解释器泛型实例模块无 GCC/Clang
  自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6m-10-s4z34-interpreter-generic-parameter-substitution.md`。
  备注：本切片提供 interpreter type-parameter lookup/substitution 读路径，尚未把 context 注入 call frame 或执行
  uncollected generic method，也未覆盖动态 struct/union 与跨模块 identity，因此不关闭 08-S6。

- 2026-07-18 19:25:44 +08:00 · 08-S6L / 10-S4Z33 interpreter reference-class generic instance context ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增 public carrier revalidation API；
  对重新校验为 `INTERPRETER_DEOPT` 的未收集请求，以 open class prototype 创建普通解释器对象，并把深拷贝的
  public generic type object 作为对象拥有的 `__zr_genericTypeInfo` 上下文；getter 可从对象回读同一上下文，已有
  open-prototype 成员继续由普通对象原型链解析。AOT carrier、struct/union prototype 和非实例对象 fail closed；
  分配期间 type object、instance 和 field key 均按 GC ignore-root 规则保护并在统一清理路径解除。
  RED/GREEN：RED 为两个 public interpreter API 缺失，MSVC 最终链接报告 2 个 unresolved symbols；GREEN 为聚焦
  目标 16 tests/0 failures。隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 的 dynamic generic route、TypeSpec
  layout、reflection token CTest 均为 3/3；三个泛型反射实现文件无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6l-10-s4z33-interpreter-generic-reference-instance-context.md`。
  备注：本切片只物化 reference-class 对象和类型上下文，不进行 generic parameter substitution 或 method
  execution，也不覆盖 struct/union 动态值、跨模块 identity 或脚本对象级构造入口，因此不关闭 08-S6。

- 2026-07-18 18:56:26 +08:00 · 08-S6K / 10-S4Z32 / 11-S4BO token-only compound generic type object ·
  状态：三阶段子切片完成；完整 08-S6、10-S4 与 11-S4 仍为部分完成。完成项目：token-only TypeSpec object
  builder 在 64 层门禁内递归物化 nested `GENERIC_INST`、`ARRAY`、`TUPLE`、`OWNERSHIP`、`NULLABLE`、
  `UNION`；新增 metadata runtime signature-node record resolver，以完整节点字节跨度把 direct TypeDef/TypeRef 和
  本地 nested TypeSpec 绑定到真实 record，未绑定节点 fail closed。原 1036 行 `metadata_runtime.c` 抽出 98 行
  `metadata_runtime_type_node_binding.c` 后为 979 行；测试对象断言拆入 131 行 support header，主场景文件为 913 行。
  RED/GREEN：RED 为 nested+array 与 compound 两个 token-only 场景 15 tests/2 failures；GREEN 为 15/0。
  隔离 WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 的 dynamic generic route、TypeSpec layout、reflection token
  CTest 均为 3/3；两个新/变更模块无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6k-10-s4z32-11-s4bo-token-only-compound-generic-type-object.md`。
  备注：record resolver 限定 attached runtime 的本地 TypeSpec identity；跨模块 canonical identity、脚本对象方法与
  解释器 generic substitution/execution 仍开放，因此不关闭 08-S6。

- 2026-07-18 18:34:53 +08:00 · 08-S6J / 10-S4Z31 token-only direct-argument generic type object ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：public object builder 现在接受由
  `ResolveDynamicGenericTypeInstance()` 返回的 token-only carrier，重新校验同一 TypeSpec route/token/layout 后，
  通过 metadata argument view 把 primitive 与 direct TypeDef/TypeRef 参数物化为 `genericArguments`；compound
  metadata node 继续 fail closed，不伪造子 token 或浅层身份。RED/GREEN：RED 为现有已收集
  `Generic<int64, Base>` token-only carrier 构造对象时 13 tests/1 failure；GREEN 为聚焦目标 13/0，隔离 WSL
  GCC 11.4、Clang 14.0 与 MSVC 19.44 三项 CTest 均 3/3，新模块无 GCC/Clang 自身告警，scoped
  `git diff --check` 通过。产出：
  `tests/acceptance/2026-07-18-aot-08-s6j-10-s4z31-token-only-direct-generic-type-object.md`。
  备注：nested generic/array/tuple/ownership/nullable/union 的 token-only 对象递归物化、脚本对象方法、跨模块
  identity 与解释器 generic substitution/execution 仍开放，因此不关闭 08-S6。

- 2026-07-18 18:25:07 +08:00 · 08-S6I / 10-S4Z30 public MakeGenericType object entry ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增 public
  `ZrCore_Reflection_MakeGenericTypeObject(state, runtime, genericBaseToken, arguments, argumentCount)`，在一个边界内
  调用唯一 constructed-generic resolver 并交给 S6H/Z29 object builder；已收集复合请求返回携带 TypeSpec/layout 的
  AOT type object，未收集请求返回无伪造 token/layout 的 interpreter-deopt type object。resolver 与对象物化规则未复制。
  RED/GREEN：RED 为 MSVC 链接缺失 MakeGenericType symbol；GREEN 为聚焦目标 13/0，隔离 WSL GCC 11.4、Clang
  14.0 与 MSVC 19.44 三项 CTest 均 3/3，新入口所在模块无自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6i-10-s4z30-make-generic-type-object.md`。
  备注：这是 C 级 public reflection 构造入口，不是脚本对象方法；解释器仍缺 generic parameter substitution 与
  uncollected instance execution，因此 08-S6 不关闭。

- 2026-07-18 18:13:41 +08:00 · 08-S6H / 10-S4Z29 public constructed-generic type object ·
  状态：交叉子切片完成；完整 08-S6 与 10-S4 仍为部分完成。完成项目：新增
  `ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject()`，对 request-resolved carrier 先重新解析并拒绝陈旧
  route/token/layout，再生成 public `kind == "type"` 对象；对象携带 AOT/interpreter-deopt route、base/TypeSpec token、
  layout id、same-runtime native pointer 和 `genericArguments`。递归 argument 对象覆盖全部 S6C~S6G kind，并把 borrowed
  descriptor 同步复制到 GC 对象图；调用方随后修改 descriptor 不影响已物化身份。实现独立放入 462 行新模块，未继续
  扩大约 5000 行 `reflection.c`，且每个新对象在下一次分配前进入 ignore-root 或父对象字段。
  RED/GREEN：RED 为 MSVC 链接缺失 builder symbol；GREEN 为聚焦目标 13/0，隔离 WSL GCC 11.4、Clang 14.0 与
  MSVC 19.44 的 dynamic generic route、TypeSpec layout、reflection token CTest 均为 3/3，新模块无三编译器自身告警。
  产出：`tests/acceptance/2026-07-18-aot-08-s6h-10-s4z29-public-generic-type-object.md`。
  备注：本切片对象化的是已解析 request carrier，不接受 token-only carrier；未提供 `MakeGenericType` 用户入口，亦未
  声明解释器已能替换泛型参数或执行未收集实例。

- 2026-07-18 17:48:40 +08:00 · 08-S6G union generic argument identity ·
  状态：08-S6G 子切片完成；完整 08-S6 仍为部分完成。本地 metadata runtime 已覆盖当前编码器可生成的
  primitive/token/nested generic/array/tuple/ownership/nullable/union 实参结构；跨模块发现和解释器动态实例
  物化/执行仍待后续。
  完成项目：argument kind 以 append-only 方式追加 `UNION`，身份携带 `unionValueType`、
  `unionNameStringOffset` 及复用的 ordered child list；验证拒绝非法 value type、空名称和 count/pointer 不一致，
  匹配器精确比较 union payload、arity 与递归子项。tuple/union 共用单一 metadata child-list walker，避免游标规则
  分叉。测试夹具覆盖 `Tuple<unique Base, Union<nullable int64>>` 的交叉嵌套；value type、名称 offset、union arity、
  nullable child、ownership qualifier 或 tuple arity 任一变化均不误绑定静态 layout。
  RED/GREEN：RED 为新增 union kind/identity fields 缺失导致干净 MSVC 编译失败；GREEN 后聚焦目标 11/0，隔离
  WSL GCC 11.4、Clang 14.0 与 MSVC 19.44 的 dynamic generic route、TypeSpec layout、reflection token CTest 3/3。
  产出：`tests/acceptance/2026-07-18-aot-08-s6g-union-generic-argument-identity.md`。
  备注：union 名称使用当前 metadata runtime 的 string-heap offset 身份；跨模块名称重映射不能用裸 offset 比较，
  必须在后续跨模块切片中先 canonicalize。

- 2026-07-18 17:39:24 +08:00 · 08-S6F tuple/ownership/nullable generic argument identity ·
  状态：08-S6F 子切片完成；完整 08-S6 仍为部分完成。constructed-generic request 已覆盖 tuple、ownership、
  nullable 复合实参，union、跨模块 TypeSpec 和解释器动态实例消费者仍待后续。
  完成项目：公共 argument kind 以 append-only 方式追加 `TUPLE`、`OWNERSHIP`、`NULLABLE`，增加稳定的
  reflection ownership qualifier 值、borrowed `childTypes` 列表和 `childCount`；请求校验与候选匹配在现有
  64 层上限内递归遍历 tuple child list，并对 ownership qualifier、nullable base node 做精确结构匹配。
  非法空 tuple child list、`NONE` ownership、空 nullable element 均 fail closed；qualifier、nullable element 或
  tuple arity 不同均保持 `INTERPRETER_DEOPT`，不误绑定已收集 TypeSpec/layout。
  RED/GREEN：RED 为新增复合 kind/field 缺失导致干净 MSVC 编译失败；GREEN 后聚焦目标 11/0，隔离 WSL GCC
  11.4、Clang 14.0 与 MSVC 19.44 的 dynamic generic route、TypeSpec layout、reflection token CTest 均为 3/3。
  产出：`tests/acceptance/2026-07-18-aot-08-s6f-tuple-ownership-nullable-generic-argument-identity.md`。
  备注：descriptor 及其递归子项继续由调用方持有；runtime 不复制请求，也不伪造未收集 metadata token/layout。

- 2026-07-18 17:24:58 +08:00 · 08-S6E recursive array generic argument identity ·
  状态：08-S6E 子切片完成；完整 08-S6 仍为部分完成。constructed-generic request 支持递归 array 实参，
  tuple/union/ownership 等复合节点、跨模块 TypeSpec 和解释器动态实例消费者仍待后续。
  完成项目：`SZrReflectionGenericTypeArgument` 追加 append-only `ARRAY` kind、`arrayRank` 与 borrowed
  `elementType`；请求校验和签名匹配统一为最多 64 层的递归结构 walker。array 节点要求 rank 精确相等，
  element 由 `ReadSignatureTypeNode()` 解析并递归复用 primitive/direct token/nested TypeSpec identity；
  顶层 direct token 仍优先要求 token 相等，array element 没有独立 token 时才与请求 token 的已验证签名根
  做同 metadata-runtime 结构比较。rank 0、空 element、自引用/超深 descriptor 均 fail closed。
  RED/GREEN：RED 为 array kind/rank/element 字段缺失导致干净 MSVC 编译失败；GREEN 后 outer TypeSpec 的
  `Base[]` 实参精确命中，rank 2 请求保持未收集 deopt，自引用 descriptor 被深度门禁拒绝，新目标 9/0。
  WSL GCC 11.4、Clang 14.0 与干净 MSVC 19.44 均通过
  `reflection_dynamic_generic_instance|metadata_runtime_typespec_layout|reflection_token_resolve` CTest 3/3；
  test argument 全部改为 designated initializer，GCC/Clang 无新增 missing-field warning。
  产出：`tests/acceptance/2026-07-18-aot-08-s6e-array-generic-argument-identity.md`。
  备注：递归 request view 仍由调用方持有生命周期，不在 runtime 内复制或伪造 metadata。

- 2026-07-18 17:13:36 +08:00 · 08-S6D nested TypeSpec argument identity ·
  状态：08-S6D 子切片完成；完整 08-S6 仍为部分完成。constructed-generic request 可用本地 TypeSpec token
  表示 nested closed-generic 实参，数组/其他复合实参、跨模块 TypeSpec 和解释器动态实例消费者仍待后续。
  完成项目：`TYPE_TOKEN` request argument 校验扩展到 metadata-valid local TypeSpec；匹配器通过现有
  `ReadTypeSpecSignatureView()` / `ReadTypeSpecGenericArgumentView()` 取得源 TypeSpec 根节点与候选 nested
  `GENERIC_INST` 节点，先验证 `[blobOffset,nextBlobOffset)` 坐标边界，再对完整结构化签名字节跨度做精确比较。
  不使用类型名称、浅层 node kind/payload 或伪造 token；结构长度/内容不同的 nested request 保持未收集 deopt。
  RED/GREEN：RED 为 inner TypeSpec 作为 outer generic 实参时 7 tests/1 failure；GREEN 后 inner nested request
  精确命中 outer TypeSpec/layout，outer-as-self 的结构近似请求不误命中，测试目标 8/0。WSL GCC 11.4、
  Clang 14.0 与干净 MSVC 19.44 均通过
  `reflection_dynamic_generic_instance|metadata_runtime_typespec_layout|reflection_token_resolve` CTest 3/3。
  产出：`tests/acceptance/2026-07-18-aot-08-s6d-nested-typespec-argument-identity.md`。
  备注：签名字节比较限定在同一 metadata runtime/本地 string-token 坐标域；跨模块等价性必须先经过 11 的
  token/string remap，不在本切片中用名称比较绕过。

- 2026-07-18 17:02:03 +08:00 · 08-S6C constructed generic request resolution ·
  状态：08-S6C 子切片完成；完整 08-S6 仍为部分完成。`MakeGenericType` 风格的 open base + 实参请求已有
  静态 TypeSpec 精确匹配与未收集组合 deopt request carrier，解释器动态实例物化/执行消费者仍待后续。
  完成项目：新增 primitive/direct TypeDef/TypeRef 两类 `SZrReflectionGenericTypeArgument` 与
  `ZrCore_Reflection_ResolveConstructedGenericType()`；解析器只扫描当前 metadata function 的本地 TypeSpec，
  对 base token、实参数量和逐实参 identity 做精确匹配，命中后复用 08-S6B route。未命中时仅在该 base 已有
  有效 TypeSpec 绑定、足以证明它是泛型定义时返回 `INTERPRETER_DEOPT`，携带 open base、argument count 与
  caller-owned borrowed argument view；不伪造 TypeSpec/signature token 或 layout。`NULL/UNKNOWN` primitive、
  非 TypeDef/TypeRef token、无泛型绑定 base 和空请求均 fail closed。
  RED/GREEN：RED 为扩展测试在干净 MSVC 下缺少 argument carrier、`requestedArguments` 与 constructed-generic API；
  GREEN 后测试目标 6/0，覆盖双实参 primitive+TypeDef 精确命中、未收集 bool 组合降级和非法请求矩阵。
  WSL GCC 11.4、Clang 14.0 与干净 MSVC 19.44 均通过
  `reflection_dynamic_generic_instance|metadata_runtime_typespec_layout|reflection_token_resolve` CTest 3/3。
  产出：`tests/acceptance/2026-07-18-aot-08-s6c-constructed-generic-request-resolution.md`。
  备注：借用实参视图的生命周期由调用方持有；nested generic/array argument identity、跨模块 TypeSpec 搜索、
  public reflection object materialization 和解释器执行闭环仍未声明完成。

- 2026-07-18 16:47:00 +08:00 · 08-S6B reflection dynamic generic TypeSpec route carrier ·
  状态：08-S6B 子切片完成；完整 08-S6 仍为部分完成。现有 TypeSpec 已有明确的 AOT/解释器降级决策点，
  但由反射实参合成新 TypeSpec 的 `MakeGenericType` 构造和解释器动态实例化执行闭环仍待后续。
  完成项目：新增 `EZrReflectionGenericInstanceRoute`、`SZrReflectionDynamicGenericTypeInstance` 与
  `ZrCore_Reflection_ResolveDynamicGenericTypeInstance()`；API 先通过
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()` 校验并携带 TypeSpec token、signature token/hash、
  open generic base token/record 和 argument list 坐标，再以唯一 metadata runtime layout registry 判定路由。
  已登记 `SZrTypeLayout` 的实例走 `AOT`，泛型绑定有效但没有静态 layout 的实例返回
  `INTERPRETER_DEOPT`，非法/非 TypeSpec 输入失败并清理输出。
  RED/GREEN：RED 为新测试在干净 MSVC 下编译失败，缺少 carrier、route enum 和解析 API；GREEN 后新测试覆盖
  非法输入、未收集实例降级和已登记实例 AOT 三条路径。WSL GCC 11.4、Clang 14.0 与干净 MSVC 19.44
  均通过 CTest `reflection_dynamic_generic_instance|metadata_runtime_typespec_layout|reflection_token_resolve` 3/3。
  产出：`tests/acceptance/2026-07-18-aot-08-s6b-reflection-dynamic-generic-typespec-route.md`。
  备注：本切片消费已有 TypeSpec metadata identity，不把 route carrier 误当作 public reflection type object，
  不合成新 metadata token，也不声明完整 `MakeGenericType`、跨模块动态实例或 full-AOT 反射闭包完成。

- 2026-06-25 18:22:45 +08:00 · 08-S4 / 11-S4E 泛型字典 TYPE_LAYOUT metadata runtime resolver 加固 ·
  状态：08-S4 已完成切片的后续加固完成；完整 08 阶段仍有跨模块真实 generic instantiation roots、
  反射构造闭包和 full-AOT mark-and-sweep closure 等后续项。
  完成项目：泛型字典 TYPE_LAYOUT/SIZEOF runtime helper 改为接收 `SZrMetadataRuntime*`，通过
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 读取 11-S4 的 code-registration layout registry；
  generated C 的 shared-reference generic TYPE_LAYOUT 宏和 shared function signature 同步携带
  `metadataRuntime`。静态 `staticTypeLayout` 快路径保留；metadata function prototype layout cache 不再作为
  runtime fallback。
  RED/GREEN：RED 为 reference-sharing 测试新增“registry layout 与 stale prototype layout 同 id 但不同 size”
  的断言后失败；GREEN 后 TYPE_LAYOUT 返回 registry layout、SIZEOF 返回 registry size，并在 registry 缺失时
  返回 null/false 而非 prototype cache。
  验证：WSL gcc/clang 均通过 `zr_vm_aot_c_generic_reference_sharing_test` 4/0、
  `zr_vm_aot_c_generic_call_typed_test` 6/0、source contracts 19/0、frame setup 1/0、
  metadata runtime type-layout 3/0、metadata runtime query 20/0、shared-library smoke 8/0、
  value-type shared-library smoke 2/0、descriptor diagnostics 2/0；Windows MSVC Debug 同组通过，
  其中 Unix-only shared-library/generic-call 分支按既有测试策略 ignored。
  产出：`tests/acceptance/2026-06-25-aot-11-s4e-generic-dictionary-type-layout-runtime-resolver.md`。
  备注：本记录只说明 08-S4 字典 layout consumer 已接入 11 的单一 layout 表；不声明 08-S6/08-S7
  的动态泛型实例闭包或 full-AOT 缺失实例诊断完成。

- 2026-06-25 06:26:16 +08:00 · 08-S7K / 11-S7V / 12-S3F / 12-S4N manifest generic MethodSpec binding ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，跨模块真实 generic instantiation roots、
  反射构造闭包和完整 mark-and-sweep closure 仍待后续。
  完成项目：manifest generic preserve target 现在可以命名 current-module typed exported method；CLI AOT preserve bridge
  匹配已有 `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 形态签名，把 method-spec token、
  open method token 与 instantiation signature hash 写入 writer root；generated C 头部同步输出 MethodSpec 诊断。
  RED/GREEN：RED 为新增 generic method preserve 用例引用 `hasMethodSpecBinding`/`methodSpecToken`
  等字段时编译失败；GREEN 后 `Factory.make<Foo>` 绑定到 method-spec token `0x08000002`、
  method token `0x03000001`，full-AOT writer 不再因缺 TypeSpec 拒绝该 MethodSpec-bound method root。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 14/0；WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7v-12-s3f-manifest-generic-methodspec-binding.md`。
  备注：本切片只关闭 current-module writer-visible generic method MethodSpec 绑定；不导出持久 zrp
  MethodSpec table，不收集泛型方法代码体的传递闭包，不绑定跨模块 generic method target。

- 2026-06-25 06:03:45 +08:00 · 08-S7J / 11-S7U / 12-S3E / 12-S4M manifest generic synthesized TypeSpec binding ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，MethodSpec 绑定、跨模块真实 generic instantiation roots、
  反射构造闭包和完整 mark-and-sweep closure 仍待后续。
  完成项目：当 manifest generic preserve root 没有现成 `TYPE_SPEC` record，但当前函数 metadata 存在同名 open
  `TYPE_DEF` 或 `TYPE_REF` base record 时，CLI AOT preserve bridge 现在会追加 synthesized `TYPE_SPEC` /
  `SIGNATURE` record pair、生成 deterministic `GENERIC_INST` signature hash，并继续复用 generic instantiation
  table 物化 baseToken/cInstanceId/shareKind。没有 open base record 的 full-AOT 未绑定 root 仍被拒绝。
  RED/GREEN：RED 为新增 full-AOT `List<Foo>` 用例只提供 `TYPE_REF(List)`、不提供 `TYPE_SPEC(List<Foo>)` 时，
  `hasTypeSpecBinding` 仍为 false；GREEN 后 synthesized TypeSpec 绑定成功，generic instance base token 为
  `0x05000001`，full-AOT writer 生成通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 13/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7u-12-s3e-manifest-generic-synthesized-typespec.md`。
  备注：本切片只关闭 current-module open-base manifest generic root 的缺失 TypeSpec 合成；MethodSpec、跨模块 target、
  反射动态实例和完整 closure checker 仍未完成。

- 2026-06-25 05:41:31 +08:00 · 08-S7I / 11-S7T / 12-S3D / 12-S4L generic instantiation TypeDef base token ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，MethodSpec 绑定、缺失 TypeSpec 合成、
  跨模块真实 generic instantiation roots、反射构造闭包和完整 mark-and-sweep closure 仍待后续。
  完成项目：generic preserve root 的 TypeSpec 签名匹配现在接受 `GENERIC_INST(TYPE_DEF target, args...)`；
  generic instantiation base token 解析会根据已绑定 TypeSpec 的 base 节点选择同类 token 表，`TYPE_DEF`
  base 查同名 TypeDef，`TYPE_REF` base 查同名 TypeRef，找不到时回退 TypeSpec。
  RED/GREEN：RED 为新增 CLI writer options 用例构造 `TYPE_DEF(List)` + `TYPE_SPEC(GENERIC_INST(TYPE_DEF List, Foo))` 后，
  `hasTypeSpecBinding` 仍为 false；GREEN 后 TypeSpec 绑定成功，generic instance base token 为 `0x02000001`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 12/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7t-12-s3d-generic-instantiation-typedef-base-token.md`。
  备注：本切片只关闭 current-module TypeDef-backed open base token 选择；MethodSpec、跨模块 target、
  缺失 TypeSpec 合成、反射动态实例和完整 closure checker 仍未完成。

- 2026-06-25 05:28:38 +08:00 · 08-S7H / 11-S7S / 12-S3C / 12-S4K generic instantiation open base token ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，MethodSpec 绑定、缺失 TypeSpec 合成、
  跨模块真实 generic instantiation roots、反射构造闭包和完整 mark-and-sweep closure 仍待后续。
  完成项目：CLI preserve bridge 现在会在 TypeSpec-bound manifest generic root 物化为 generic instantiation
  identity 前，扫描当前函数 metadata token records；若存在同名 `TYPE_REF` 签名记录，则把该 open generic
  base token 传给 `SZrGenericInstantiationTable_GetOrAddResolved()`，否则保持 closed `TYPE_SPEC` 回退。
  generated C manifest 诊断可输出 `genericInstance.baseToken = 0x05000001`。
  RED/GREEN：RED 为新增 CLI writer options 用例构造 `TYPE_REF(List)` + `TYPE_SPEC(List<Foo>)` 后，
  仍得到 closed `TYPE_SPEC` base token `0x07000001`；GREEN 后得到 open `TYPE_REF` base token `0x05000001`，
  既有缺失 TypeRef 的 TypeSpec-backed fallback 仍通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 11/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7s-12-s3c-generic-instantiation-open-base-token.md`。
  备注：本切片只关闭 current-module TypeRef-backed open base token 选择；MethodSpec、跨模块 target、
  缺失 TypeSpec 合成、反射动态实例和完整 closure checker 仍未完成。

- 2026-06-25 05:08:49 +08:00 · 08-S7G / 11-S7R / 12-S3B / 12-S8I full-AOT generic instantiation closure gate ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，MethodSpec 绑定、缺失 TypeSpec 合成、
  跨模块真实 generic instantiation roots、反射构造闭包和完整 mark-and-sweep closure 仍待后续。
  完成项目：`backend_aot_manifest_generic_roots_closed_for_full_aot()` 不再只接受 `hasTypeSpecBinding`；
  full-AOT 下每个 manifest generic preserve root 必须同时拥有 TypeSpec metadata binding 和
  `hasGenericInstantiationBinding`，否则 writer 在发射前返回 `ZR_FALSE`。
  RED/GREEN：RED 为新增 CLI writer options 用例直接构造 TypeSpec-only generic root 后，writer 仍返回 true；
  GREEN 后该 root 被拒绝，CLI 物化出的 `List<Foo>` TypeSpec-backed generic instantiation root 和 hybrid 诊断路径保持通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 10/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7r-12-s8i-full-aot-generic-instantiation-closure-gate.md`。
  备注：本切片只收紧 full-AOT writer-side generic instance identity 门禁；open generic base token、
  MethodSpec、跨模块 target、反射动态实例和完整 closure checker 仍未完成。

- 2026-06-25 04:50:01 +08:00 · 08-S7F / 11-S7Q / 12-S3A / 12-S4J manifest generic TypeSpec-backed instantiation root ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，MethodSpec 绑定、缺失 TypeSpec 合成、
  跨模块真实 generic instantiation roots、反射构造闭包和完整 mark-and-sweep closure 仍待后续。
  完成项目：`SZrAotManifestGenericRoot` 新增 generic instantiation 绑定字段；
  CLI preserve bridge 在 generic preserve root 已匹配当前模块 `TYPE_SPEC` 后，复用
  `SZrGenericInstantiationTable_GetOrAddResolved()` 为该 closed TypeSpec-backed root 分配稳定
  `genericInstantiationBaseToken`、`genericInstantiationInstanceId` 和 `genericInstantiationShareKind`。
  generated C manifest 诊断同步输出 `genericInstance.baseToken`、`genericInstance.id` 与
  `genericInstance.shareKind`。
  RED/GREEN：RED 为 CLI writer options 测试先引用缺失的 generic instantiation binding fields 后编译失败；
  GREEN 后 `List<Foo>` TypeSpec-backed generic preserve root 绑定为 shared-reference generic instance id 1。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 9/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7q-12-s3a-manifest-generic-preserve-instantiation-root.md`。
  备注：本切片只把已有 TypeSpec binding 转为 writer 可见的 generic instantiation identity；`baseToken`
  暂用 closed `TYPE_SPEC` token，后续仍需 open generic base token/MethodSpec/跨模块/反射动态实例接入。

- 2026-06-25 04:14:31 +08:00 · 08-S7E / 11-S7P / 12-S8H full-AOT manifest generic TypeSpec closure gate ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，MethodSpec 绑定、缺失 TypeSpec 合成、
  真实 generic instantiation roots 和反射构造闭包仍待后续。
  完成项目：`ZrParser_Writer_WriteAotCFileWithOptions()` 在 full-AOT 模式进入 module/function table 构建前，
  检查 writer options 中的 manifest generic preserve roots；只要存在未绑定 `TYPE_SPEC` 的 root，就返回
  `ZR_FALSE`，避免把仅有文本 target/arguments 的 generic preserve 声称为完整 AOT 闭包。默认 hybrid 模式继续允许
  未绑定 root 输出诊断清单。
  RED/GREEN：RED 为新增 CLI writer options 用例在 `aotMode: "full-aot"` 且没有匹配 TypeSpec metadata 时仍成功生成 C；
  GREEN 后该路径被拒绝，既有 hybrid generic preserve root 和已绑定 TypeSpec root 路径保持通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 8/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7p-12-s8h-full-aot-generic-preserve-typespec-closure-gate.md`。
  备注：本切片只关闭 manifest generic preserve root 的 full-AOT TypeSpec 绑定门禁；不合成缺失 TypeSpec，
  不解析 MethodSpec，不把 root materialize 到 generic instantiation table，也不声明完整 mark-and-sweep closure 完成。

- 2026-06-25 04:00:47 +08:00 · 08-S7D / 11-S7O / 12-S4I manifest generic preserve TypeSpec binding ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，full-AOT 收集不全编译期诊断、MethodSpec 绑定、
  缺失 TypeSpec 合成、真实 generic instantiation roots 和反射构造闭包仍待后续。
  完成项目：`SZrAotManifestGenericRoot` 新增可选 TypeSpec 绑定字段；
  CLI preserve bridge 会扫描当前函数 metadata token records，当 `.zrp` generic preserve 的 target/arguments
  与已有 `GENERIC_INST(TYPE_REF target, args...)` `TYPE_SPEC` 签名匹配时，写入 TypeSpec token、
  paired signature token 和 signature hash；generated C 头部同步输出这些 token/hash 诊断。
  RED/GREEN：RED 为 CLI writer options 测试先引用缺失的 `hasTypeSpecBinding`、`typeSpecToken`、
  `signatureToken`、`signatureHash` 字段而编译失败；GREEN 后 `List<Foo>` generic preserve root
  绑定到 `TYPE_SPEC` token `0x07000001`、`SIGNATURE` token `0x08000001` 和 hash `0x123456789abcdef0`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 7/0；WSL gcc/clang 与 Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5；`git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-25-aot-11-s7o-12-s4i-manifest-generic-preserve-typespec-binding.md`。
  备注：本切片只绑定当前模块中已存在、签名完全匹配的 `TYPE_SPEC`；不合成缺失 TypeSpec，不处理
  MethodSpec、跨模块 generic target、实例表 materialization 或 full-AOT 缺失实例诊断。

- 2026-06-25 03:27:16 +08:00 · 08-S7C / 11-S7N / 12-S4H manifest generic preserve writer roots ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，收集不全编译期诊断、MethodSpec/TypeSpec token 绑定、
  真实 generic instantiation 可达闭包和反射构造闭包仍待后续。
  完成项目：`SZrAotWriterOptions` 新增 `manifestPreserveGenericRoots` 与
  `manifestPreserveGenericRootCount`，承载 `.zrp` `preserve` 中 `kind: "generic"` 的 target 与
  concrete arguments；`SZrCliAotPreserveRoots` 同步管理 generic root 数组及参数指针数组；
  `ZrCli_Compiler_ApplyProjectAotPreserveRules()` 在 feature 条件匹配后把 generic preserve 规则注入 writer options。
  AOT C emitter 在文件头输出 `manifest.genericRoots` 与每个 generic root 的 target/argument 清单，作为后续
  MethodSpec/TypeSpec 解析和 generic instantiation 收集的稳定输入面。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 引用缺失的 generic root writer fields 后编译失败；
  GREEN 后 `List<Foo, Bar.Baz>` 被注入 writer options，generated C 输出对应 manifest generic root 记录。
  验证：WSL gcc/clang `zr_vm_cli_aot_writer_options_test` 均 6/0，并且 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed` 均 3/3；
  Windows MSVC Debug 同目标 6/0，同 CTest 过滤 3/3；`python -m json.tool zrp.schema.json` 通过；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-25-aot-11-s7n-12-s4h-manifest-generic-preserve-writer-roots.md`。
  备注：本切片只关闭 manifest generic preserve 到 writer option/generated-C 清单的 bridge；不声明
  MethodSpec/TypeSpec token binding、泛型实例表 materialization、跨模块 generic target 或 full-AOT missing-instance
  闭合诊断完成。

- 2026-06-25 01:13:27 +08:00 · 08-S7 / 11-S7H / 12-S8G CLI AOT C emission entry ·
  状态：08-S7 CLI 发射入口子切片完成；完整 08-S7 仍未关闭，收集不全的编译期诊断、
  manifest 动态泛型实例和反射构造闭包仍待后续 12 可达性/manifest/full-AOT 校验。
  完成项目：新增 CLI `--emit-aot-c` 编译选项；项目编译记录解析 `bin/aot_c/src/<module>.c`
  输出路径，依赖模块从依赖包 `bin` 派生；编译后的 `.zro` 作为 binary input embedded module blob
  传给 `ZrParser_Writer_WriteAotCFileWithOptions()`；`.zrp` `aotMode: "full-aot"` 通过
  `ZrCli_Compiler_ApplyProjectAotWriterOptions()` 注入 `requireFullAot`，full-AOT generic
  `CALL_TYPED` 生成物保留 no-deopt marker 且不含 missing-instance deopt bridge。
  RED/GREEN：RED 为 CLI args 测试引用缺失 `SZrCliCommand.emitAotC` 编译失败；GREEN 后
  `--emit-aot-c` 解析/校验、AOT C 路径解析、CLI project full-AOT AOT C 输出均通过。
  验证：WSL gcc/clang `cli_args|cli_project_incremental` CTest 均 2/2；Windows MSVC Debug 同组 2/2；
  Windows MSVC CLI 实际执行 `--compile --emit-aot-c --incremental` 在缺失 `main.c` 时重编译并重新生成
  `bin/aot_c/src/main.c`（114478 bytes）；`git diff --check` 退出 0，仅有既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7h-cli-aot-c-emission-entry.md`。
  备注：本记录只关闭 CLI 项目编译到 AOT C writer 的入口；不声明 full-AOT 缺失泛型实例诊断、
  反射保留或完整 mark-and-sweep closure 完成。

- 2026-06-25 00:29:49 +08:00 · 08-S7 / 11-S7G manifest full-AOT writer option bridge ·
  状态：08-S7 manifest policy 注入子切片完成；完整 08-S7 仍未关闭，CLI AOT C 发射入口接线、
  收集不全的编译期诊断、manifest 动态泛型实例和反射构造闭包仍待后续。
  完成项目：`ZrCli_Compiler_ApplyProjectAotWriterOptions()` 将 `SZrLibrary_Project.aotMode`
  映射到 `SZrAotWriterOptions.requireFullAot`；manifest `full-aot` 置 true，缺省 hybrid 置 false，
  并保持其他 writer option 字段。
  RED/GREEN：RED 为 CLI project incremental 测试引用缺失 helper 后链接失败；GREEN 后
  full-AOT/hybrid 两条 project writer option 用例通过。
  验证：WSL gcc/clang `zr_vm_cli_project_incremental_test` 均 10/0；Windows MSVC Debug 同目标 10/0；
  Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-25-aot-11-s7g-zrp-project-manifest-aot-mode-writer-injection.md`。
  备注：本记录只建立 manifest policy 到 writer option 的注入点；当前 CLI 仍未提供 AOT C 发射模式，
  full-AOT 泛型闭包诊断仍依赖后续 12 可达性工作。

- 2026-06-25 00:08:34 +08:00 · 08-S7 / 11-S7F manifest-declared full-AOT mode ·
  状态：08-S7 前置 manifest 子切片完成；完整 08-S7 仍未关闭，manifest 到 writer option 的自动注入、
  收集不全的编译期诊断、manifest 动态泛型实例和反射构造闭包仍待后续。
  完成项目：`.zrp` project manifest 新增 `aotMode` declaration parser，缺省 `hybrid`，显式
  `"full-aot"` 写入 `SZrLibrary_Project.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT`；
  schema 同步 `hybrid`/`full-aot` enum。
  RED/GREEN：RED 为 manifest normalization 测试引用缺失 AOT mode project model 后编译失败；GREEN 后
  缺省 hybrid、显式 full-AOT 和非法 mode 拒绝均通过。
  验证：WSL gcc/clang `zr_vm_project_manifest_normalization_test` 14/0 与
  `zr_vm_project_import_resolver_test` 9/0；schema JSON 解析通过；Windows MSVC 同两 focused 测试 14/0、9/0，
  CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-25-aot-11-s7f-zrp-project-manifest-aot-mode.md`。
  备注：本记录只为 08-S7 建立 manifest 声明层；该 11-S7F 切片本身不包含把该字段传入
  `SZrAotWriterOptions.requireFullAot` 的后续 bridge。

- 2026-06-24 19:10:02 +08:00 · 08-S7B full-AOT generic METHOD slot static closure ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，收集不全的编译期诊断、manifest 动态泛型实例和反射
  `MakeGenericType` 闭包仍依赖 `12` 的可达性/manifest/full-AOT 闭合校验。
  完成项目：full-AOT shared generic `CALL_TYPED` lowering 在已静态解析 callee function index 时不再生成
  callsite-local `SZrAotGenericDictionary`、`ZrAot_GenericSlot_Method()` 和 METHOD slot null runtime branch；
  生成物保留 `zr_aot_generic_call_typed_full_aot_no_deopt` marker，并直接把 `zr_aot_fn_<callee>` 传给
  `ZrLibrary_AotRuntime_CallInlineStruct()`。默认 hybrid 路径仍保留 METHOD slot lazy 解析和 missing-instance
  deopt bridge。
  RED/GREEN：RED 为 full-AOT generic call typed 用例要求不出现
  `if (zr_aot_generic_call_typed_method == ZR_NULL)` 后失败；GREEN 后 full-AOT 生成 C 没有 METHOD slot
  null 分支、没有 missing-instance deopt bridge，并继续编译共享库执行返回 `42`。
  验证：`zr_vm_aot_c_generic_call_typed_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8e-full-aot-generic-method-slot-closure.md`。
  备注：本切片只关闭已静态收集 shared generic callsite 的运行期 METHOD slot 缺失分支；不声明运行期动态泛型实例
  收集、manifest 预声明或完整“收集不全编译期报错”完成。

- 2026-06-24 14:03:46 +08:00 · 08-S7A full-AOT generic `CALL_TYPED` no-deopt switch ·
  状态：08-S7 子切片完成；完整 08-S7 仍未关闭，缺失泛型实例的编译期诊断依赖 `12`
  的可达性/manifest/full-AOT 闭合校验。完成项目：`SZrAotWriterOptions` 增加
  `requireFullAot`，AOT C writer 将该选项传入函数体和 value SemIR `CALL_TYPED`
  lowering；默认 hybrid 模式继续在 shared generic METHOD slot 缺失时生成
  `zr_aot_generic_call_typed_missing_instance_deopt` 与
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()`；显式 full-AOT 模式下同一静态收集
  callsite 生成 `zr_aot_generic_call_typed_full_aot_no_deopt`，保留
  `ZrAot_GenericSlot_Method()` + `CallInlineStruct()` fast path，但若 METHOD slot 为空则直接
  `ZR_AOT_C_FAIL()`，不再写入解释器 deopt bridge。
  RED/GREEN：RED 为 08-S6A 后 writer 没有 full-AOT 选项，所有 shared generic `CALL_TYPED`
  都生成 missing-instance deopt 兜底；GREEN 后新增 full-AOT 源级泛型 callsite 验收，生成 C
  同时包含 `zr_aot_generic_call_typed_shared_callsite` 与
  `zr_aot_generic_call_typed_full_aot_no_deopt`，且不包含
  `zr_aot_generic_call_typed_missing_instance_deopt`、
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state, ...)` 或
  `"generic call typed missing AOT instance"`，编译共享库并通过 AOT execution 返回与解释器一致的 `42`。
  验证：`zr_vm_aot_c_generic_call_typed_test` 6/0。
  产出：`tests/acceptance/2026-06-24-aot-08-s7a-full-aot-generic-call-typed.md`。
  备注：当前工程尚无 `12` 的 mark-and-sweep 泛型实例闭合图、manifest 动态泛型保留规则或
  `10` 的运行期 `MakeGenericType` 入口，因此本记录不声称“收集不全编译期报错”已完成；它只关闭
  full-AOT 开关在已静态收集 callsite 上禁用动态 deopt 的可验证子切片。

- 2026-06-24 13:47:41 +08:00 · 08-S6A 泛型 `CALL_TYPED` missing-instance deopt bridge ·
  状态：08-S6 子切片完成；完整 08-S6 仍未关闭，反射构造/运行期动态泛型实例化缺口仍待实现；08-S7
  full-AOT 模式仍未完成。完成项目：runtime 新增
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()`，复用现有 deopt id 校验与动态调用路径，
  为 inline struct typed return 准备 call window、复制 value 参数、以 `CallWithoutYield...ReturnDestination`
  调回解释器，并把结果写入 AOT frame 的 inline struct destination；shared generic `CALL_TYPED`
  生成物在 `ZrAot_GenericSlot_Method()` 返回非空时继续走 AOT `CallInlineStruct()`，当 METHOD slot
  无法解析到静态入口时生成 `zr_aot_generic_call_typed_missing_instance_deopt` marker，并调用
  `CallInlineStructDynamicDeoptBridge()`。测试补充 METHOD slot 缺失不缓存的 runtime 行为，以及把生成 C
  的 `.staticMethod = zr_aot_fn_*` 改成 `.staticMethod = ZR_NULL` 后重新编译共享库，验证 AOT 执行仍经解释器
  fallback 返回与解释器一致的 `42`。
  RED/GREEN：RED 为 08-S5 完成后 shared generic `CALL_TYPED` METHOD slot 缺失仍会 `ZR_AOT_C_FAIL()`，
  没有可执行的 missing-instance deopt 路径；GREEN 后 `zr_vm_aot_c_generic_call_typed_test` 扩展到 5/0，
  生成 C 同时包含 `zr_aot_generic_call_typed_missing_instance_deopt`、
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state, ...)` 与
  `"generic call typed missing AOT instance"`，手动置空 METHOD slot 的生成共享库执行成功。
  验证：`zr_vm_aot_c_generic_call_typed_test` 5/0、`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 2/0。
  产出：`tests/acceptance/2026-06-24-aot-08-s6a-generic-call-typed-missing-instance-deopt.md`。
  备注：这只是 shared generic METHOD-slot missing-instance fallback；计划要求的反射
  `MakeGenericType` / 运行期动态实例化入口还未建立，故不关闭完整 08-S6。

- 2026-06-24 13:32:21 +08:00 · 08-S5 泛型 `CALL_TYPED` 单态/共享两形态 ·
  状态：08-S5 验收完成；08-S6 未收集实例 deopt、08-S7 full-AOT 模式仍未完成。完成项目：
  在 08-S5A METHOD-slot carrier 基础上，AOT C value SemIR `CALL_TYPED` lowering 现在接收
  callee typed metadata、caller function index 和 exec instruction index；当源级泛型调用的 callee
  参数含 reference-like `T: class` 且 caller argument 为 `SZrTypeValue` inline value 时，生成
  `ZR_AOT_GENERIC_SLOT_METHOD` 单 slot callsite dictionary，经 `ZrAot_GenericSlot_Method()` lazy 解析
  具体 `zr_aot_fn_*` 入口，再把该入口传给 `ZrLibrary_AotRuntime_CallInlineStruct()`；非共享泛型路径仍保留
  直接 `zr_aot_fn_*` 调用。typed metadata 构建补齐当前函数/方法泛型参数识别，并在 script typed export
  metadata 构建时切换到被导出函数 declaration，保证 `func stamp<T>(value: T) where T: class`
  的参数 metadata 可被 AOT route 使用。验收用例把 `Stamp stamp<T>(T)` 的源级泛型调用写成项目二进制输入，
  编译生成 C 共享库并执行，断言 AOT 返回值与解释器返回值一致。
  RED/GREEN：RED 为 08-S5A 后仅有 METHOD-slot carrier，源级泛型 `CALL_TYPED` 没有共享 callsite
  route，也缺少 AOT/解释器执行一致性；GREEN 后 `zr_vm_aot_c_generic_call_typed_test` 扩展为 3/0，
  第三个用例同时包含 `zr_aot_generic_call_typed_shared_callsite`、`ZrAot_GenericSlot_Method(...)`、
  `ZR_AOT_GENERIC_SLOT_METHOD`、`.staticMethod = zr_aot_fn_*` 与
  `ZrLibrary_AotRuntime_CallInlineStruct(...)`，并通过 shared-library AOT execution 验证结果。
  验证：`zr_vm_aot_c_generic_call_typed_test` 3/0、`zr_vm_aot_c_generic_reference_sharing_test` 2/0、
  `zr_vm_aot_c_generic_monomorphization_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_method_info_signature_test` 1/0、
  `zr_vm_type_inference_test` 全量通过、CTest
  `aot_c_generic_monomorphization`/`aot_c_generic_reference_sharing`/`aot_c_generic_call_typed` 3/3。
  产出：`tests/acceptance/2026-06-24-aot-08-s5-generic-call-typed.md`。备注：本记录关闭完整
  08-S5；dynamic reflection / 未收集泛型实例 deopt 仍属 08-S6，full-AOT 缺失实例诊断仍属 08-S7。

- 2026-06-24 12:46:24 +08:00 · 08-S5A 泛型 METHOD slot carrier + CALL_TYPED 双形态生成契约 ·
  状态：08-S5 子切片完成；08-S5B 源级泛型 `CALL_TYPED` 双形态接入、08-S6 未收集实例 deopt、
  08-S7 full-AOT 模式仍未完成。完成项目：runtime 新增
  `ZrLibrary_AotRuntime_GenericSlot_Method()`，按 `SZrAotGenericDictionary.resolvedSlots` lazy 缓存
  `ZR_AOT_GENERIC_SLOT_METHOD` 的 `FZrAotEntryThunk`；AOT C generic sharing emitter 增加
  `ZrAot_GenericSlot_Method(dict, slot)` 访问宏、每实例 METHOD slot、`zr_aot_generic_dict_*_method_0`
  静态方法入口，以及共享函数内“取字典 METHOD slot 并调用”的生成形态；AOT C generic monomorphization
  wrapper 增加 `zr_aot_generic_call_typed_monomorphized_direct` marker，锁定值类型泛型的直接特化入口形态。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_generic_call_typed_test` 后链接失败，缺少
  `ZrLibrary_AotRuntime_GenericSlot_Method`；GREEN 后同一测试 2/0，生成 C 同时包含
  monomorphized direct marker、`ZrAot_GenericSlot_Method(dict, 1u)`、`ZR_AOT_GENERIC_SLOT_METHOD`、
  `zr_aot_generic_method_1(state)`，并成功编译为共享库。验证：
  `zr_vm_aot_c_generic_call_typed_test` 2/0、`zr_vm_aot_c_generic_reference_sharing_test` 2/0、
  `zr_vm_aot_c_generic_monomorphization_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_method_info_signature_test` 1/0、
  CTest `aot_c_generic_monomorphization`/`aot_c_generic_reference_sharing`/`aot_c_generic_call_typed`
  3/3。产出：`tests/acceptance/2026-06-24-aot-08-s5a-generic-call-typed-method-slot.md`。
  备注：本记录不关闭完整 08-S5；还需 08-S5B 证明源级泛型 `CALL_TYPED` 的单态/共享两形态会被实际调用路径选择，
  且 AOT 执行结果与解释器一致。

- 2026-06-24 12:14:13 +08:00 · 08-S4 泛型字典布局 + lazy 解析 + 共享引用类型代码生成 ·
  状态：08-S4 验收完成；08-S5 泛型 `CALL_TYPED` 双形态、08-S6 未收集实例 deopt、
  08-S7 full-AOT 模式仍未开始。完成项目：公共 AOT ABI 升至 v4，新增
  `EZrAotGenericSlotKind`、`SZrAotGenericSlot`、`SZrAotGenericResolvedSlot` 与
  `SZrAotGenericDictionary`，`SZrAotMethodInfo` 增加 `genericDictionary` 指针；
  runtime 新增 `ZrLibrary_AotRuntime_GenericSlot_TypeLayout()` 与
  `ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf()`，按字典 cache lazy 填充
  TYPE_LAYOUT/SIZEOF slot；AOT C emitter 增加 `ZrAot_GenericSlot_*` 访问宏、引用型闭泛型扫描、
  每实例字典表和按泛型基名去重的 `zr_fn_<base>__shared` 入口，MethodInfo 挂载当前函数首个共享字典。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_generic_reference_sharing_test` 后编译失败，缺少
  `SZrAotGenericDictionary`/slot/cache 类型与 lazy API；GREEN 后同一测试 2/0，生成 C 中
  `Box<RefA>` 与 `Box<RefB>` 各有字典但只出现一份 `zr_fn_box__shared`，并成功编译为共享库。
  验证：`zr_vm_aot_c_generic_reference_sharing_test` 2/0、CTest `aot_c_generic_reference_sharing` 1/1、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_method_info_signature_test` 1/0、`zr_vm_aot_c_generic_monomorphization_test` 1/0。
  产出：`tests/acceptance/2026-06-24-aot-08-s4-generic-reference-sharing.md`。备注：本切片只建立
  dictionary ABI/lazy helper 与共享引用型生成形态；跨泛型方法 `CALL_TYPED` 的单态/共享分派留到 08-S5。
  额外探测的旧 `zr_vm_aot_c_shared_library_smoke_test` 仍有一个既有文本断言未找到 `zr_aot_arith_exec`，
  与本切片字典路径无直接依赖，未作为 08-S4 验收门槛。

- 2026-06-24 11:38:33 +08:00 · 08-S3 单态化值类型/const 泛型 ·
  状态：08-S3 验收完成；08-S4 泛型字典、08-S5 泛型 CALL_TYPED、08-S6 未收集实例 deopt、
  08-S7 full-AOT 模式仍未开始。完成项目：新增 `backend_aot_c_generic_monomorphization.{h,c}`，
  从 AOT function table 的 closed generic inline slots 收集 monomorphized instance，发
  `zr_aot_generic_monomorphization_table`、closed `ZrLayout_<id>` fallback 声明和 `zr_fn_pair__*`
  特化 wrapper；closed generic prototype 在 type inference 阶段生成 concrete field layout，
  typed metadata 优先精确匹配 `Pair<int,int>`；core frame layout 可从 open generic layout id
  保守解析到唯一 closed instance，constructor inline receiver 回拷改按解析后 layout 兼容性判断。
  RED/GREEN：RED 先暴露缺少 monomorphization marker/layout，随后暴露 inline typed call、`COPY_STACK`
  missing inline layout、最终结果 0；逐步补齐 closed layout、generic fallback 和 constructor receiver
  compatible copy 后 GREEN。验证：`zr_vm_aot_c_generic_monomorphization_test` 1/0、CTest
  `aot_c_generic_monomorphization` 1/1、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_typed_scalar_test` 1/0、`zr_vm_aot_c_generic_numeric_contracts_test` 1/0、
  `zr_vm_aot_c_generic_numeric_shared_library_smoke_test` 1/0、`zr_vm_parser_test` 75/0、
  `zr_vm_type_inference_test` 全量通过、CTest `generic_instantiation`/`generic_constraints` 2/2。
  产出：`tests/acceptance/2026-06-24-aot-08-s3-generic-monomorphization.md`。备注：本切片只证明
  closed value-generic struct 的单态化 C layout/entry 与执行一致性；引用类型共享字典、泛型
  `CALL_TYPED` 双形态、dynamic-instance deopt 和 full-AOT missing-instance 诊断仍属 08-S4..S7。

- 2026-06-24 10:28:48 +08:00 · 08-S2 约束求解编译期检查 ·
  状态：08-S2 验收完成；08-S3 单态化生成、08-S4 泛型字典、08-S5 泛型 CALL_TYPED、
  08-S6 未收集实例 deopt、08-S7 full-AOT 模式仍未开始。完成项目：确认现有
  `validate_generic_call_bindings_constraints()` 与闭型实例化约束检查已覆盖 named constraint、
  class/struct、new()、owner、unique/shared/weak 精确 ownership；新增独立
  `zr_vm_generic_constraints_test`，锁定 `where T: new()` 对默认可构造 class 放行、对 interface
  报 `new() constraint` 诊断。RED/GREEN：初次构建新目标前 CMake 未重配，target 不存在；
  重配后测试直接 GREEN，说明生产约束逻辑已满足计划验收。验证：`zr_vm_generic_constraints_test`
  1/0、CTest `generic_constraints` 1/1、`zr_vm_parser_test` 75/0、`zr_vm_type_inference_test` 118/0。
  产出：`tests/acceptance/2026-06-24-aot-08-s2-generic-constraints.md`。备注：本切片不做 AOT codegen；
  后续 08-S3 才开始把约束/实例化结果接入单态化生成。

- 2026-06-24 10:18:45 +08:00 · 08-S1 泛型实例化收集表 + 去重 + 共享性判定 ·
  状态：08-S1 已完成；08-S2 约束求解、08-S3 单态化生成、08-S4 泛型字典、08-S5 泛型 CALL_TYPED、
  08-S6 未收集实例 deopt、08-S7 full-AOT 模式仍未开始。完成项目：新增
  `SZrGenericInstantiationTable` / `SZrGenericInstantiationRecord`，记录 `baseToken`、类型实参、
  `shareKind` 与稳定递增 `cInstanceId`；按 base token + 实参类型 + 已解析 reference/value shape 去重；
  按 il2cpp 规则实现“全部 reference → shared，任一 value → monomorphized”；提供默认
  `EZrValueType` shape 推断和显式 resolved shape 入口，供后续 compiler prototype class/struct 区分接入。
  RED/GREEN：RED 为新增 08-S1 测试目标后构建失败，缺少 `zr_vm_parser/generic_instantiation.h`；GREEN 后
  `zr_vm_generic_instantiation_test` 3/0、CTest `generic_instantiation` 1/1、相关 `zr_vm_type_inference_test`
  118/0。参考证据：HybridCLR `GenericSharing::IsShareable` 对非引用实参返回 false；NativeAOT
  `GenericDictionaryNode` 以具体 type/method instantiation 驱动 canonical dictionary；Mono
  `MonoGenericInst` 用 hash/equality cache 去重；Roslyn TypeSpec/MethodSpec writer 用结构签名索引去重。
  产出：`tests/acceptance/2026-06-24-aot-08-s1-generic-instantiation-table.md`。
