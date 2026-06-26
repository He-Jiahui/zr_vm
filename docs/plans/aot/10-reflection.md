---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn 完善反射
  - decision: 2026-06-20 反射默认最小 + 注解保留（对标 NativeAOT/illink）；分三级 None/RuntimeMapping/Description
references:
  - lua/hybridclr/libil2cpp/vm/Reflection.h          # invoker_method / 反射对象缓存
  - lua/hybridclr/libil2cpp/il2cpp-class-internals.h   # MethodInfo.invoker / Il2CppMethodPointer
  - lua/mono/mono/metadata/reflection.c               # 完整动态反射（备选，未采用为默认）
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/AnalysisBasedMetadataManager.cs   # MetadataCategory 三态
  - lua/runtime/src/tools/illink/src/linker/Linker.Dataflow/FlowAnnotations.cs   # DynamicallyAccessedMembers
related_code:
  - zr_vm_core/include/zr_vm_core/reflection.h         # TypeOfValue / BuildTypeLiteralObject / 成员反射
  - zr_vm_core/src/zr_vm_core/reflection.c             # 缓存 + PIN; 10-S4A / 11-S4H registry-backed type/member layout consumer
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h    # 11-S4I FieldDef token/row/offset/layout binding view for later token-driven field reflection entity materialization; 11-S4J TypeSpec layout binding view for later type-argument reflection; 11-S4K TypeDef/TypeSpec token -> layout cache resolver for future public token reflection lookup; 11-S4L typeLayoutId -> token reverse resolver; 11-S4M bounded multi-entry cache; 11-S4N cTypeId -> token resolver; 11-S4O code-registration typeLayout token carrier count mirror; 11-S4P/11-S4Q generated TypeDef/TypeSpec-backed token population consumer path; 11-S4R registry-backed owner-field layout table consumer path
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c # 11-S4I..11-S4O FieldDef/TypeSpec row binding validates row identity, resolves layouts through the AOT registry, exposes TypeDef/TypeSpec token -> layout cache lookup, provides typeLayoutId/cTypeId -> token reverse lookup, keeps bounded multi-entry cache hits, and consumes code-registration typeLayout token tables
  - zr_vm_core/include/zr_vm_core/object.h             # SZrMemberDescriptor / prototype
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h      # 10-S1A MethodInfo reflection level ABI; 10-S2A invoker ABI
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c # 12-S7Y default-min reflection metadata policy option helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h # 12-S7Y reflection metadata policy option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 10-S1A MethodInfo default; 10-S2A invoker emission dispatch; 12-S7Y metadata policy marker/plumbing
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h # 10-S2A invoker emitter API; 12-S7Y policy-driven reflection level parameter
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 10-S2A shared entry-thunk invoker + MethodInfo binding; 12-S7Y MethodInfo NONE/RUNTIME_MAPPING emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c # 11-S4R generated ownership offset table emission for owner-field layouts
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c # 11-S4Q generated TypeSpec-backed token table population for future type-argument reflection
  - tests/parser/test_aot_c_frame_setup_contracts.c     # 10-S1A/10-S2A source contract; 12-S7Y policy-driven MethodInfo emitter source contract
  - tests/parser/test_aot_c_shared_library_smoke.c      # 10-S1A/10-S2A runtime descriptor assertion; non-stripped RUNTIME_MAPPING guard
  - tests/parser/test_aot_c_code_stripping.c            # 12-S7Y opt-in stripping lowers generated MethodInfo reflection level to NONE
  - tests/parser/test_aot_c_source_contracts.c          # 12-S7Y metadata policy plumbing source contract
  - tests/parser/test_aot_c_descriptor_diagnostics.c    # 10-S1A/10-S2A ABI diagnostic version drift guard
  - tests/module/test_metadata_runtime_type_layout.c    # 10-S4A reflection layout source contract and 11-S4H prototype layout resolver coverage
  - tests/module/test_metadata_runtime_query.c          # 11-S4I FieldDef layout binding view coverage
  - tests/module/test_metadata_runtime_typespec_layout.c # 11-S4J TypeSpec layout binding view coverage; 11-S4K TypeDef/TypeSpec token -> layout cache coverage; 11-S4L typeLayoutId -> token reverse lookup coverage; 11-S4M multi-entry cache coverage; 11-S4N cTypeId -> token coverage; 11-S4O/11-S4P code-registration token table coverage
  - tests/parser/test_aot_c_generic_call_typed.c        # 11-S4Q generated TypeSpec token table coverage for generic layouts
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 11-S4R generated ownership-offset table coverage
---

# 10 · 反射（分级元数据 + invoker thunk + 注解保留）

> 承接缺口：zr_vm 已有 `ZrCore_Reflection_TypeOfValue`、`BuildTypeLiteralObject`（按名查类型）、
> 成员查询（`SZrMemberDescriptor` + `FindMemberDescriptor`，含继承）、协议检查、反射对象缓存与 PIN。
> **缺**：按 token 反射、泛型参数反射、字段 offset 反射、统一动态调用入口（invoker）、
> 反射与裁剪的隔离。本文按既定决策（**默认最小 + 注解保留**）补齐，并与 `11`/`12` 联动。

## 0. 核心决策：反射三级（对标 NativeAOT MetadataCategory）

每个类型/方法/字段的反射能力分三级，由裁剪（`12`）可达性 + 注解决定，**默认按可达性取最小**：

| 级别 | 含义 | 产出 | 默认 |
|------|------|------|------|
| `NONE` | 无反射元数据 | 不可被反射发现 | 未被反射可达的实体 |
| `RUNTIME_MAPPING` | 可动态调用 / 查表，但不可完整枚举 | invoker thunk + token↔实体映射 | 被间接调用/泛型字典引用的实体 |
| `DESCRIPTION` | 完整签名，可枚举成员 | 完整元数据（参数名/类型/特性/offset） | 被注解或 manifest 显式要求 |

> 默认产物最小：未被注解、未被反射可达的类型只生成执行所需的 layout/函数，**不**带反射元数据。
> 这是「极致性能 + 小体积」与「按需反射」的平衡（对标 NativeAOT，反 il2cpp/mono 的默认全保留）。

## 1. 统一动态调用入口（invoker thunk，对标 il2cpp invoker_method）

反射调用（`Method.Invoke` 等价）需要把「`SZrValue[]` 参数 → 调具体 typed C 函数 → 打包返回」。
**这正是 `07`§6 的边界 marshaling**，复用而非新建：

```c
/* 每个 RUNTIME_MAPPING+ 的函数登记一个 invoker，签名统一 */
typedef void (*FZrAotReflectionInvoker)(struct SZrState *state,
                                        FZrAotEntryThunk target,
                                        const struct SZrAotMethodInfo *method,
                                        struct SZrTypeValue *self,        /* 实例方法的接收者，静态则 NULL */
                                        struct SZrTypeValue *args,        /* 入参数组（dynamic 表示） */
                                        struct SZrTypeValue *outReturn);  /* 返回打包目标 */
```

- invoker 内部：按 `method->signature` 把 `args[i]` unbox 成 typed 寄存器（`07`§6 入参解包）→
  调 `target` 的真实 C 函数 → 把返回寄存器 box 回 `outReturn`（返回打包）。
- invoker 按**签名分桶生成**（相同 C 签名共享一个 invoker，对标 il2cpp 按签名生成 invoker），
  避免每方法一份导致膨胀。
- 入口表登记到代码注册表（`11`§2），token/反射对象据此找到 invoker（对标 il2cpp `invokerPointers[]`）。
- 10-S2A 先按当前 AOT C 事实落地第一类签名桶：generated 函数仍统一暴露为
  `FZrAotEntryThunk(SZrState *)`，因此 `SZrAotMethodInfo.invoker` 先登记共享
  `zr_aot_invoker_entry_thunk`。它是后续 token/反射调用 API 可消费的 ABI carrier，不声明完整
  参数解包/返回打包已经完成。

## 2. token 驱动的反射解析（衔接 11）

现状只能按 string 名查类型；补 token 通道（对标 mono token→entity、il2cpp index→entity）：

- 反射 API 增 `ZrCore_Reflection_ResolveToken(metadataToken) -> 运行期实体`，经 `11` 的
  `SZrMetadataRuntime`（MetadataCache 等价）lazy 解析 token → 原型/方法/字段。
- string 名查找改为「名→token→实体」两段，token 是单一真相（不变量 C），名表可被裁剪（`12`）。

## 3. 字段 offset / 布局反射

- `DESCRIPTION` 级类型暴露字段偏移：直接读唯一 `SZrTypeLayout.fields[i].byteOffset`（`02`/`11`），
  **不**在反射层另存偏移（不变量 C）。
- 11-S4I 已补出后续 token-driven 字段反射实体可消费的只读绑定视图：
  `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView()` 以 FieldDef token 为入口读取 zrp FieldDef row 的
  `byteOffset/typeLayoutId`，并要求 owner/field layout 都来自 code-registration layout registry。当前只是数据路径，
  尚未把该 view 接入 public reflection field entity。
- 反射读写字段值 = 在边界处按 offset 构造/解构 `SZrValue`（`07`§6），typed 内部仍是 `.`/偏移。

## 4. 泛型反射（衔接 08）

- 暴露泛型实例的类型实参：反射对象记 `baseToken + argTokens[]`（来自 `08`§3 实例化表）。
- 11-S4J 已补出后续类型实参反射可消费的只读绑定视图：
  `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView()` 以 TypeSpec token 为入口，把 zrp TypeSpec row、
  11-S3K generic base-token binding 和 code-registration registry layout 连起来，并校验 signature identity。
  当前只是数据路径，尚未把该 view 接入 public generic type reflection 或 `MakeGenericType`。
- 11-S4K 已把 TypeDef/TypeSpec token→layout 查询收敛为
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()`，并缓存最近一次 token→layout 命中；这为后续
  `ResolveToken`/泛型类型实参反射提供 public token lookup 入口，但当前仍未物化 public reflection entity。
- 11-S4L 补出 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`，可从 registry-backed `typeLayoutId`
  反查 TypeDef/TypeSpec token 并复用最近一次 cache；这为后续 layout-driven reflection entity 回写 token
  提供底座，但仍不是 public reflection API。
- 11-S4M 将上述 token/layout cache 扩展为 bounded 8-entry multi-entry cache，同一 runtime 可同时保留
  TypeDef 与 TypeSpec 的 token→layout 和 layoutId→token 命中；这避免后续类型实参枚举、layout-driven entity
  回写 token 与 public `ResolveToken` 互操作在同一反射流程中互相覆盖 cache，但仍不是 public reflection API。
- 11-S4N 补出 `ZrCore_MetadataRuntime_ResolveCTypeIdToken()`，在当前 `cTypeId == typeLayoutId` 的 registry
  不变量下让后续 layout-driven reflection entity 可直接按 generated C type id 回写 TypeDef/TypeSpec token；
  它复用 11-S4M cache 和 no-prototype-fallback 行为，但仍不是 public reflection API。
- 11-S4O 补出 code-registration `typeLayoutTokens/typeLayoutTokenCount` carrier，metadata runtime 可在 zrp scan
  fallback 前先从 registration 表把 cTypeId/typeLayoutId 解析为 TypeDef/TypeSpec token；这为后续 layout-driven
  reflection entity 提供更直接的 token 回写路径。
- 11-S4P 已把 generated token table 的可靠子集填为真实 `TYPE_DEF` token：本地 named struct/union layout
  能唯一匹配 TypeDef metadata 时，cTypeId/typeLayoutId→token resolver 可直接命中 registration 表；缺 metadata、
  多重匹配、TypeSpec/generic layout 仍为 0 且 fallback 到 zrp scan。该入口仍未接入 public reflection entity。
- `MakeGenericType`/运行期构造泛型 → 若实例已静态收集（`08`§3）返回其原型；否则解释器动态实例化
  （`08`§6 deopt），动态实例反射级别为 `RUNTIME_MAPPING`。
- 泛型参数约束（`08`§4）在 `DESCRIPTION` 级暴露。

## 5. 反射与裁剪的注解（对标 DynamicallyAccessedMembers / DynamicDependency / RequiresUnreferencedCode）

在 zr 语言层引入保留注解，驱动 `12` 的标记，使「静态裁剪 + 动态反射」共存：

- `@reflectable(members: methods|fields|all)`：把某类型/成员提升到 `DESCRIPTION` 级（对标 `preserve`）。
- `@dynamically_accessed(MemberTypes)` 标注参数/返回：数据流分析（`12`）据此保留被反射访问的成员
  （对标 `DynamicallyAccessedMembers` + `FlowAnnotations`）。
- `@dynamic_dependency("Member", Type)`：手工保留特定成员（对标 `[DynamicDependency]`）。
- `@requires_unreferenced_code("reason")`：标记「内部用反射、裁剪下不安全」的 API，编译器在调用点
  给裁剪警告（对标 `RequiresUnreferencedCode` + analyzer，衔接 `12` trim warnings）。
- 未注解的动态反射点（`ResolveToken`/按名查未知类型）→ 裁剪后该目标可能 `NONE` → 返回空/抛错
  并产出 trim 警告（对标 illink「未注解反射」诊断）。

## 6. 反射对象缓存与 GC（沿用现状 + 对接 09）

- 沿用现有反射对象缓存（`__zr_reflection_cache`）+ PIN（`reflection_pin_raw_object`）。
- PIN 与 `09` 移动 GC 协同：反射对象进 `pinned` region 或登记为不可移动根，避免 compact 失效。

## 7. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 10-S1 | 反射三级模型 + 实体级别标注（默认按可达性最小）（§0） | 🚧 2026-06-26 部分完成：10-S1A 已完成 AOT MethodInfo 级 `NONE`/`RUNTIME_MAPPING`/`DESCRIPTION` ABI carrier，默认/非裁剪生成方法保持 `RUNTIME_MAPPING`；12-S7Y 已让 opt-in code stripping 产物的 generated MethodInfo `reflectionMetadataLevel` 降为 `NONE` 并输出 `metadata_policy.reflectionLevel` marker；实体级 annotation/DESCRIPTION 提升、类型级默认最小与完整体积对比仍待 `12`/`10` 后续 |
| 10-S2 | 按签名分桶 invoker thunk（复用 07§6）+ 注册表登记（§1） | 🚧 2026-06-25 部分完成：10-S2A 已完成 `FZrAotEntryThunk` 当前签名桶 invoker ABI carrier、生成物共享 invoker 和 MethodInfo 登记；11-S2A 已提供 generated-C code registration carrier；完整 `Invoke` 参数解包/返回打包、token 注册表消费和 AOT/解释器结果等价仍待后续 |
| 10-S3 | token 驱动反射解析（衔接 11）（§2） | 按 token 查类型/方法/字段成功；名表可裁剪后仍按 token 可用 |
| 10-S4 | 字段 offset / 泛型参数反射（§3/§4） | 🚧 2026-06-26 部分完成：10-S4A 已让脚本类型与字段 layout/offset 反射在 attached AOT registry 下读取 11-S4H 的 registry-backed `SZrTypeLayout`；11-S4I 已提供后续 DESCRIPTION 级 FieldDef token-driven 字段实体可消费的 FieldDef row→`byteOffset/typeLayoutId`→owner/field layout binding view；11-S4J 已提供后续类型实参反射可消费的 TypeSpec row→generic base binding→registry layout binding view；11-S4K 已提供后续 public token reflection 可复用的 TypeDef/TypeSpec token→registry layout resolver；11-S4L 已提供 layoutId→TypeDef/TypeSpec token 反查入口；11-S4M 已将 token/layout 命中扩展为 bounded multi-entry cache；11-S4N 已提供 cTypeId→TypeDef/TypeSpec token 反查入口；11-S4O 已提供 code-registration typeLayout token carrier 和 table-first cTypeId/typeLayoutId→token 消费路径；11-S4P 已让唯一匹配本地 TypeDef 的 generated struct/union entries 写入真实 token；11-S4Q 已让唯一匹配同函数 TypeSpec 的 generated generic entries 写入真实 token；11-S4R 已让 generated struct owner fields 暴露 ownership offset table；泛型参数反射、public reflection field entity 接入、union owner offsets 和完整类型实参暴露仍待后续 |
| 10-S5 | 保留注解（@reflectable/@dynamically_accessed/@dynamic_dependency/@requires_unreferenced_code）驱动 12（§5） | 注解的反射目标裁剪后仍可用；未注解给警告 |

## 8. 不变量校验

- **B 纯降级**：反射是边界能力，invoker = 边界 marshaling；typed 函数体本身不含反射代码。
- **C 单一真相**：偏移/签名/类型实参全部读唯一 layout + token 记录，反射层不另存。
- **D 环境隔离**：invoker 是独立边界函数，与 typed 函数体物理分离（可被 `07`§9 grep 排除）。
- 与 `12` 协同：反射级别即裁剪可达性的产物，注解是二者唯一接口。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-06-26 06:00:16 +08:00 · 10-S1 support / 12-S7Y default-min generated MethodInfo reflection policy ·
  状态：10-S1 支撑子切片完成；完整 10-S1 仍未关闭，annotation 驱动 `DESCRIPTION` 提升、
  类型/字段/泛型实体级默认最小策略和完整体积下降验收仍待后续。
  完成项目：generated AOT C MethodInfo 的 `reflectionMetadataLevel` 现在由 writer policy 决定：
  默认/非裁剪产物继续输出 `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，opt-in code stripping 产物输出
  `ZR_AOT_REFLECTION_METADATA_NONE`，文件头同步记录 `metadata_policy.reflectionLevel`。
  RED/GREEN：RED 为 code-stripping fixture 要求裁剪产物 MethodInfo 降为 `NONE` 后失败 1/4；GREEN 后
  shared option helper 统一计算 reflection metadata level，method metadata emitter 按该 level 输出常量名。
  验证：WSL gcc/clang 均通过 code stripping 4/0、source contracts 21/0、frame setup contract 1/0、
  typed scalar 1/0、shared-library smoke 8/0；Windows MSVC Debug 同组通过，其中 typed scalar/shared-library
  smoke 按既有 Unix-only guard 分别 1 ignored / 8 ignored。
  产出：`tests/acceptance/2026-06-26-aot-12-s7y-default-min-reflection-metadata-policy.md`。
  备注：本切片只关闭 generated MethodInfo 反射级别的默认最小接线；未实现 public reflection entity
  物化、注解语义或运行期按 token 反射。

- 2026-06-26 01:14:40 +08:00 · 10-S4 support / 11-S4R generated ownership offset table emission ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体、union owner offsets 和注解驱动保留策略
  仍待后续 10/11/12 切片。完成项目：generated C 的 `SZrTypeLayout` descriptor 现在可为 struct owner fields
  暴露 `ZrOwnershipOffsets_<typeLayoutId>[]`，让后续 layout-driven reflection/metadata consumers 能在读取 registry-backed
  layout 时看到 owner-field offset table；unsafe/unsupported/union 路径保持 `ZR_NULL`。RED/GREEN：RED 为
  type-layout source contract 要求 ownership-offset writer helper 后缺少
  `backend_aot_c_type_layout_can_emit_ownership_offsets(`；GREEN 后 `Unique<string>` 字段 generated layout 输出
  `ownershipFieldCount = 1u` 和 `.ownershipFieldOffsets = ZrOwnershipOffsets_0`。验证：WSL gcc/clang 均通过
  `aot_c_type_layout_contracts` CTest 1/1、source contracts 19/0、value-type smoke 4/0；Windows MSVC Debug 通过
  CTest 1/1、source contracts 19/0、value-type smoke 3/0/1 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4r-generated-ownership-offset-table.md`。
  备注：本记录不声明 public reflection entity、泛型反射对象、union owner offset 表、runtime generic layout construction
  或 cross-module token-table policy 完成。

- 2026-06-26 00:42:14 +08:00 · 10-S4 support / 11-S4Q generated TypeSpec token population ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：generated C 的 `zr_aot_type_layout_tokens[]` 现在可为同函数 metadata 中
  canonical `TYPE_SPEC` signature 可唯一匹配的 generated generic layout 写入真实 `TYPE_SPEC` token；后续
  layout-driven reflection entity 可从 code-registration table-first resolver 直接拿到 closed generic type token，
  而不是只能通过 zrp row scan fallback。RED/GREEN：RED 为 `Pair<int, int>` generated-C smoke 要求
  `0x07000001u` 时旧 token 表全为 0；GREEN 后 generated `zr_aot_type_layout_tokens[4] == 0x07000001u`。
  验证：WSL gcc/clang 均通过 `aot_c_type_layout_contracts|aot_c_generic_call_typed` CTest 2/2、source contracts
  19/0、value-type smoke 3/0；Windows MSVC Debug 通过同组 CTest 2/2、source contracts 19/0、value-type smoke
  2/0/1 ignored。产出：`tests/acceptance/2026-06-26-aot-11-s4q-generated-typespec-type-layout-token-population.md`。
  备注：本记录不声明 public generic reflection API、跨模块 TypeSpec token 表、runtime generic layout synthesis
  或泛型实参枚举对象完成。

- 2026-06-26 00:13:32 +08:00 · 10-S4 support / 11-S4P generated type-layout token population ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：generated C 的 `zr_aot_type_layout_tokens[]` 现在对唯一匹配本地 TypeDef metadata 的
  named struct/union layout 写入真实 `TYPE_DEF` token；metadata runtime 的 table-first cTypeId/typeLayoutId→token
  resolver 因此可在这类 generated layout 上直接命中 registration 表，缺 metadata、多重匹配、TypeSpec/generic 仍为 0
  并保留 zrp scan fallback。RED/GREEN：RED 为 union `Shape` generated-C smoke 缺 runtime layout descriptor 和非零
  token；GREEN 后 generated union layout registry、token 表和 `0x02000001u` token entry 通过。验证：WSL gcc/clang
  均通过 metadata TypeSpec layout 14/0、AOT type-layout contracts 1/0、source contracts 19/0、frame setup 1/0、
  shared-library smoke 8/0、value-type smoke 3/0；Windows MSVC Debug 通过 metadata 14/0、type-layout contracts 1/0、
  source contracts 19/0、frame setup 1/0，shared/value-type smoke 的 Unix-only 分支按既有规则 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4p-generated-type-layout-token-population.md`。
  备注：本记录不声明 public reflection entity、泛型反射对象、TypeSpec/generic token population 或运行期泛型实例构造完成。

- 2026-06-25 23:13:20 +08:00 · 10-S4 support / 11-S4O type layout token carrier ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：code registration 新增 `typeLayoutTokens/typeLayoutTokenCount`，metadata runtime
  在 cTypeId/typeLayoutId→token 解析时可先读 registration token 表，再 fallback 到 zrp row scan；该路径为后续
  layout-driven reflection entity 回写 token 提供更直接 carrier，但当前 generated 表项仍为 0，尚未完成真实 token
  population，也尚未接入 public reflection entity。RED/GREEN：RED 为 focused ABI/source/runtime tests 后缺少 token
  carrier 字段和 runtime count mirror；GREEN 后手工非零 token 表消费、非 type token/缺 layout 拒绝、generated table
  shape 和 descriptor validation 均通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime TypeSpec
  layout 14/0、AOT source contracts 19/0、frame setup contracts 1/0；WSL gcc/clang 通过 shared-library smoke 8/0
  和 value-type smoke 2/0，MSVC 对应 Unix-only smoke 分支为 ignored。产出：
  `tests/acceptance/2026-06-25-aot-11-s4o-type-layout-token-carrier.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体、真实 token 填充或运行期泛型实例构造完成。

- 2026-06-25 22:22:13 +08:00 · 10-S4 support / 11-S4N cTypeId token resolver ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 新增
  `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)`，在当前 registry 的 `cTypeId == typeLayoutId`
  不变量下复用 bounded token/layout cache 与 TypeDef/TypeSpec token 反查路径。该入口为后续 layout-driven
  reflection entity 按 generated C type id 回写 token 提供底座，但尚未接入 public reflection entity。
  RED/GREEN：RED 为 cTypeId→token focused tests 后缺少 public resolver/API；GREEN 后 TypeDef/TypeSpec cTypeId
  反查、多项 cache 命中和 no-prototype-fallback 负向用例通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  metadata runtime TypeSpec layout 12/0、metadata runtime query 22/0、metadata runtime type-layout 10/0；
  zrp metadata format 11/0 同组通过。产出：
  `tests/acceptance/2026-06-25-aot-11-s4n-ctype-id-token-resolver.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 22:13:54 +08:00 · 10-S4 support / 11-S4M bounded multi-entry type layout cache ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 的 TypeDef/TypeSpec token/layout cache 从单项最近命中扩展为
  8 项 bounded cache，`ResolveTypeTokenLayout()` 与 `ResolveTypeLayoutToken()` 可在同一 runtime 中同时保留
  TypeDef 和 TypeSpec 的正向/反向命中。该入口为后续类型实参枚举、layout-driven reflection entity 回写 token
  和 public `ResolveToken` 互操作提供更稳定的缓存底座，但尚未接入 public reflection entity。
  RED/GREEN：RED 为多项 cache 用例暴露旧单项 cache 会覆盖前一个 TypeDef/TypeSpec 命中；GREEN 后多项
  token→layout 与 layoutId→token 命中均通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime
  TypeSpec layout 10/0、metadata runtime query 22/0、metadata runtime type-layout 10/0；zrp metadata format 11/0
  同组通过。产出：
  `tests/acceptance/2026-06-25-aot-11-s4m-multi-entry-type-layout-cache.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 21:53:56 +08:00 · 10-S4 support / 11-S4L typeLayoutId to token reverse resolver ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 新增
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)`，可从 registry-backed layout id 反查
  TypeDef/TypeSpec token，并复用最近一次 token/layout cache。该入口为后续 layout-driven reflection entity
  回写 token、类型实参枚举和 public `ResolveToken` 互操作提供底座，但尚未接入 public reflection entity。
  RED/GREEN：RED 为新增 layoutId→token focused test 后缺少 public resolver/API；GREEN 后 TypeDef/TypeSpec
  layoutId 反查、cache 命中和 stale prototype cache 负向用例通过。验证：WSL gcc/clang 与 Windows MSVC Debug
  均通过 metadata runtime TypeSpec layout 8/0、metadata runtime query 22/0、metadata runtime type-layout 10/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4l-layout-id-token-reverse-cache.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 21:37:38 +08:00 · 10-S4 support / 11-S4K TypeDef/TypeSpec token layout cache resolver ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 新增
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, typeToken, outTypeLayoutId)`，把 `TYPE_DEF` 和
  `TYPE_SPEC` token 解析为 registry-backed `SZrTypeLayout`，并缓存最近一次 token→layoutId/layout 命中。
  该入口为后续 `ResolveToken`、字段/类型实参反射和 generic type reflection 提供统一 token lookup 底座，
  但尚未接入 public reflection entity。RED/GREEN：RED 为新增 TypeDef/TypeSpec token cache focused test 后
  缺少 public resolver/API；GREEN 后 TypeDef token cache、TypeSpec token cache、非 type token 拒绝和 stale
  prototype cache 负向用例通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime TypeSpec
  layout 5/0、metadata runtime query 22/0、metadata runtime type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4k-type-token-layout-cache.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 21:18:46 +08:00 · 10-S4 support / 11-S4J TypeSpec layout binding view ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、
  `MakeGenericType` runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略
  仍待后续 10/11/12 切片。完成项目：metadata runtime 新增 TypeSpec layout binding view，从 zrp `TYPE_SPECS`
  row 读取 `typeLayoutId/signatureHash`，复用 11-S3K generic base-token binding，并通过 code-registration layout
  registry 解析 closed TypeSpec 的 `SZrTypeLayout`；该 view 为后续类型实参/泛型实例反射提供 token→row→generic
  binding→layout 的单一真相入口，但尚未接入 public reflection entity。RED/GREEN：RED 为新增 TypeSpec layout
  focused test 后缺少 zrp row 字段、view type/API；GREEN 后正向 binding 与 stale prototype cache 负向用例通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime TypeSpec layout 2/0、metadata runtime query
  22/0、metadata runtime type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4j-typespec-layout-binding-view.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API 或运行期泛型实例构造完成。

- 2026-06-25 20:43:52 +08:00 · 10-S4 support / 11-S4I FieldDef layout binding view ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public token-driven 字段反射实体、
  泛型参数反射、类型实参枚举和注解驱动保留策略仍待后续 10/11/12 切片。
  完成项目：metadata runtime 新增 FieldDef token binding view，从 zrp `FIELD_DEFS` row 读取
  `byteOffset/typeLayoutId`，绑定 owner `TYPE_DEF` row，并通过 code-registration layout registry 解析
  field/owner `SZrTypeLayout`；该 view 为 DESCRIPTION 级字段 offset 反射提供 token→row→layout 的单一真相入口，
  但尚未接入 public reflection field entity。RED/GREEN：RED 为 metadata runtime query 新增 FieldDef binding view
  用例后缺少 view type/API；GREEN 后正向 binding 与 stale prototype cache 负向用例通过。验证：WSL gcc/clang 与
  Windows MSVC Debug 均通过 metadata runtime query 22/0 和 metadata runtime type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4i-fielddef-layout-binding-view.md`。
  备注：本记录不声明 `FieldInfo` 等 public 反射实体、字段读写 marshaling 或泛型反射完成。

- 2026-06-25 20:27:41 +08:00 · 10-S4A / 11-S4H reflection field layout registry consumer ·
  状态：10-S4 子切片完成；完整 10-S4 仍未关闭，泛型参数反射、DESCRIPTION 级 token-driven 字段实体、
  类型实参枚举和注解驱动保留策略仍待后续 10/11/12 切片。
  完成项目：反射层在构建脚本 type reflection 时通过
  `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(entryFunction, prototype, &typeLayoutId)` 获取
  attached AOT registry 中的 `SZrTypeLayout`；类型级 `layout.fieldCount/size/alignment` 从该 layout 写入；
  字段级 `offset/size/layout` 由 `SZrTypeLayout.fields[i].byteOffset/byteSize` 写入，并按实例字段序号而非旧
  serialized `member->fieldOffset` 反查 registry field；decorator target member reflection 使用同一 helper。
  未 attached registry、native type entry 或无匹配 field 时继续保留既有非 AOT 反射行为。RED/GREEN：RED 为
  focused metadata runtime type-layout 测试在新增 prototype layout resolver/反射消费契约后编译/链接失败；
  GREEN 后源码契约证明反射消费端包含 metadata runtime、调用 registry-backed resolver、类型 layout 写入 helper
  和字段 layout 写入 helper，并锁定字段按实例序号消费 registry。验证：WSL gcc/clang 均通过
  `zr_vm_metadata_runtime_type_layout_test` 10/0、`zr_vm_metadata_runtime_query_test` 20/0、
  `zr_vm_aot_c_type_layout_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0；
  Windows MSVC Debug 同组通过 10/0、20/0、1/0、19/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4h-10-s4a-reflection-layout-registry-consumer.md`。
  备注：本记录只关闭字段 offset/layout 的 registry-backed 读取入口；不声明完整 token 反射解析、泛型参数反射、
  `DESCRIPTION` metadata policy 或裁剪注解完成。

- 2026-06-25 12:05:39 +08:00 · 10-S2A MethodInfo reflection invoker carrier ·
  状态：10-S2 子切片完成；完整 10-S2 仍未关闭，`Method.Invoke` 参数解包/返回打包、token 注册表消费、
  typed-target 分桶和 AOT/解释器结果等价验收仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 7u`；新增
  `FZrAotReflectionInvoker(state, target, method, self, args, outReturn)`；`SZrAotMethodInfo`
  增加 `invoker` 字段；AOT C 生成物在 MethodInfo 表前发射共享 `zr_aot_invoker_entry_thunk`，
  当前按统一 `FZrAotEntryThunk(SZrState *)` 签名分桶，并让每个 generated MethodInfo 登记
  `.invoker = zr_aot_invoker_entry_thunk`。
  RED/GREEN：RED 为 `zr_vm_aot_c_shared_library_smoke_test` 在新增 `methodInfos[0]->invoker`
  断言后编译失败，且 frame setup 源契约缺少 `struct SZrTypeValue` / invoker ABI 文本；
  GREEN 后 ABI/source 契约通过，生成共享库 descriptor 暴露非空 MethodInfo invoker。
  验证：WSL gcc/clang 均通过 focused 组：`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0、`zr_vm_aot_c_return_contracts_test` 1/0；
  Windows MSVC Debug 构建通过，frame setup 1/0、source contracts 19/0、return contracts 1/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支）、descriptor diagnostics 1/0（1 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-10-s2a-reflection-invoker-carrier.md`。
  备注：本切片只提供 invoker ABI carrier 和 generated-C 登记点；不声明完整反射调用 API、参数数组 marshaling、
  token 解析、字段/generic DESCRIPTION 元数据或注解裁剪完成。

- 2026-06-24 18:52:22 +08:00 · 12-S8D / 10 handoff full-AOT TYPEOF reflection runtime contract guard ·
  状态：12-S8 反射相关子切片完成；10 阶段继续进行中，10-S2 invoker、10-S3 token 解析、10-S5 注解/数据流
  仍未完成。
  完成项目：AOT C writer 在 `requireFullAot` 下把 `TYPEOF` 视为未注解 reflection runtime contract；
  若当前产物仍需 `ZrLibrary_AotRuntime_TypeOf()` / `ZrCore_Reflection_TypeOfValue` runtime boundary，则 writer
  编译期返回 `ZR_FALSE` 并删除半成品 C。默认 hybrid 路径继续允许 TYPEOF runtime boundary。
  RED/GREEN：RED 为 full-AOT TYPEOF smoke 仍成功生成；GREEN 后 full-AOT TYPEOF 被拒绝，hybrid TYPEOF
  仍生成并编译。
  验证：`zr_vm_aot_c_global_shared_library_smoke_test` 10/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8d-full-aot-typeof-reflection-closure.md`。
  备注：这是 12-S8 full-AOT 护栏在反射 runtime boundary 上的第一条落地，不声明 invoker thunk、token 反射解析、
  字段/generic DESCRIPTION 元数据或保留注解完成。

- 2026-06-24 14:37:23 +08:00 · 10-S1A MethodInfo reflection metadata level carrier ·
  状态：10-S1 子切片完成；完整 10-S1 仍未关闭，实体级可达性/裁剪驱动默认最小与体积下降验收仍待
  `12` 可达性图和后续 10 反射策略接入。
  完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 5u`；新增
  `EZrAotReflectionMetadataLevel`，包含 `ZR_AOT_REFLECTION_METADATA_NONE`、
  `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`、`ZR_AOT_REFLECTION_METADATA_DESCRIPTION`；
  `SZrAotMethodInfo` 增加 `reflectionMetadataLevel` 与保留字节；AOT C 生成的 MethodInfo
  默认写入 `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，为后续 invoker/token mapping 保留执行级元数据能力。
  RED/GREEN：RED 为 AOT MethodInfo 无反射 metadata level，生成共享库无法声明当前方法只保留 runtime mapping；
  GREEN 后源契约锁定 ABI/枚举/字段/生成默认值，共享库 descriptor runtime assertion 验证导出 MethodInfo
  的 `reflectionMetadataLevel` 为 `RUNTIME_MAPPING`，ABI mismatch 诊断跟随当前版本号而非硬编码旧值。
  验证：`zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0。产出：
  `tests/acceptance/2026-06-24-aot-10-s1a-reflection-metadata-level.md`。
  备注：本记录只提供 MethodInfo 级三态 carrier；未反射可达类型不带反射元数据、体积下降可测、注解/manifest
  提升到 `DESCRIPTION` 均仍属后续 10/12 工作。
