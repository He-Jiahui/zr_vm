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
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_*.c
  - tests/parser/test_generic_constraints.c               # 08-S2 constraint acceptance
  - tests/parser/test_aot_c_generic_monomorphization.c    # 08-S3 AOT acceptance
  - tests/parser/test_aot_c_generic_reference_sharing.c    # 08-S4 AOT acceptance; 11-S4E metadata-runtime layout resolver regression
  - tests/parser/test_aot_c_generic_call_typed.c           # 08-S5/08-S6A/08-S7A/S7B generic CALL_TYPED acceptance
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
| 08-S6 | 未收集实例 deopt 到解释器动态实例化（§6） | 🚧 2026-06-24 部分完成：08-S6A 已覆盖 shared generic `CALL_TYPED` METHOD slot 缺失时回退解释器；反射构造/运行期动态实例收集缺口仍未完成 |
| 08-S7 | full-AOT 模式：收集不全编译期报错（§6 可选） | 🚧 2026-06-25 部分完成：08-S7A 已覆盖 C writer full-AOT 开关生效，静态收集到的 shared generic `CALL_TYPED` 不再生成 missing-instance deopt；08-S7B 已让 full-AOT 已闭合 shared generic `CALL_TYPED` 直接调用静态 AOT method entry，不再保留 METHOD slot null runtime branch；11-S7F 已让 `.zrp` manifest 能声明 `aotMode: "full-aot"` 并进入 project model；11-S7G 已让 project manifest policy 可注入 `SZrAotWriterOptions.requireFullAot`；11-S7H/12-S8G 已让 CLI `--emit-aot-c` 项目编译入口发射 AOT C 并消费 manifest full-AOT policy；08-S7C/11-S7N/12-S4H 已把 manifest generic preserve target+arguments 注入 AOT writer options 并在 generated C 清单中输出；08-S7D/11-S7O/12-S4I 已在当前函数 metadata 存在匹配 `GENERIC_INST` `TYPE_SPEC` 时把 generic preserve root 绑定到 TypeSpec/signature token/hash；08-S7E/11-S7P/12-S8H 已让 full-AOT writer 拒绝未绑定 TypeSpec 的 manifest generic preserve root；08-S7F/11-S7Q/12-S3A/12-S4J 已把已绑定 TypeSpec 的 manifest generic root 物化为 generic instantiation identity（baseToken/cInstanceId/shareKind）并输出 manifest 诊断；08-S7G/11-S7R/12-S3B/12-S8I 已让 full-AOT writer 拒绝 TypeSpec-only generic preserve root，要求同时具备 generic instantiation identity；08-S7H/11-S7S/12-S3C/12-S4K 已在当前模块存在同名 `TYPE_REF` metadata 时让 generic instantiation base token 使用 open generic base token，并在缺失 TypeRef 时回退 closed TypeSpec；08-S7I/11-S7T/12-S3D/12-S4L 已支持 `GENERIC_INST(TYPE_DEF target, args...)` TypeSpec，并让 current-module TypeDef base 使用 open `TYPE_DEF` token；08-S7J/11-S7U/12-S3E/12-S4M 已在 manifest generic root 缺失 TypeSpec 但存在同名 open `TYPE_DEF`/`TYPE_REF` metadata 时合成 current-module TypeSpec/signature binding 并继续物化 generic instantiation identity；08-S7K/11-S7V/12-S3F/12-S4N 已把 manifest generic method root 绑定到现有 `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 形态签名，并让 full-AOT closure gate 接受 MethodSpec-bound generic method root；跨模块真实 generic instantiation roots、反射构造闭包和完整 mark-and-sweep closure 仍需后续 |

## 8. 不变量校验

- **A 确定性**：单态化实例全槽单一静态类型；共享实例类型未定槽统一为 `SZrRawObject*`，具体性经字典，不引入运行期类型分支于纯标量段。
- **B 纯降级**：单态化路径零字典零 VM 调用；共享路径仅在字典 slot 首次解析处有受控 runtime 调用（纳入 `04`§4 白名单）。
- **C 单一真相**：字典 TYPE_LAYOUT/SIZEOF/GC 位图全部来自唯一 `SZrTypeLayout`（`02`/`11`），不另算偏移。
- **D 环境隔离**：泛型字典经 `SZrAotMethodInfo`（`07`）携带，不进函数体语句流；共享函数体仍是寄存器 + 纯 C。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

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
