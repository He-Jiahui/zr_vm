# 01 Canonical TypeRef、Place IR、CFG facts 与 artifact schema

> 状态：细化草案，等待人工确认。
>
> 上位设计：[ZR 语法、引用与内存模型重设计](./2026-07-18-zr-syntax-and-memory-model-redesign.md)
>
> 后继设计：[引用语法与 borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)

## 1. 目标结果

建立一个位于 AST 与 ExecBC/bytecode 之间的规范语义层，使 parser、类型检查、borrow checker、VM、AOT、artifact、反射和 LSP 不再分别解释类型与所有权。

本设计完成后，以下问题必须有唯一答案：

- 一个类型的结构身份是什么。
- 一个表达式产生 Value、Place 还是 Ref。
- 一次读取、写入、移动、借用和 drop 作用于哪个 Place。
- CFG 的控制流、异常流和 cleanup 流如何表达。
- 哪些 facts 是局部分析数据，哪些契约需要跨模块序列化。
- `.zrs/.zri/.zro` 如何表达同一规范类型和布局身份。

## 2. 当前基础与不足

### 2.1 可复用基础

- `semantic_facts.h` 已有 `TZrTypeId`、`TZrSymbolId`、`TZrOverloadSetId`、`TZrLifetimeRegionId`。
- `cfg.h` 已有 entry、statement、join、exit block 和 reachability facts。
- `type_layout.h` 已有 size、alignment、field offset、GC/ownership offset、copy/drop kind。
- `metadata_token.h` 已有 TypeDef、TypeRef、TypeSpec、Signature token，以及 signature/layout hash。
- `function.h` 已有 typed locals、typed exports、SemIR table 和 escape binding sidecar。
- parser、LSP 和 AOT 已经存在 semantic fact、metadata token、layout registry 的验收测试。

### 2.2 必须替换的薄弱点

1. `SZrFunctionTypedTypeRef` 用 `baseType + typeName + ownershipQualifier + isArray` 扁平描述类型，无法自然表达嵌套 ref、readonly view、owner、tuple、union 和泛型实例。
2. AST `SZrType.ownershipQualifier`、parameter `passingMode`、ownership builtin kind 和类型名字符串重复表达所有权/引用语义。
3. 当前 `compiler_semir.c` 从已经生成的执行指令映射出 `SZrSemIrInstruction`，更接近 AOT/deopt sidecar，不是前置 Semantic IR。
4. 当前 CFG block 只持有一个 AST statement，successor 固定最多两个，不能统一表达 switch、异常、cleanup 和 suspension edge。
5. semantic facts 以 AST node 为主要关联键，缺少稳定 PlaceId、ValueId、LoanId 和 block entry/exit state。
6. artifact signature 已有 ownership node，但尚无规范 ref/read-only/scoped/call contract 表达。

结论：不能继续扩充扁平 `SZrFunctionTypedTypeRef` 或给现有 post-bytecode SemIR 增加更多表层语法枚举；必须先建立前置规范层。

## 3. 分层架构

```text
Source
  -> Tokens
  -> Syntax AST
  -> Bound declarations / symbols
  -> Canonical Type graph
  -> Semantic IR + CFG + Place graph
  -> Flow facts / diagnostics / public contracts
  -> ExecIR / ExecBC
  -> VM or AOT
```

各层职责：

- Syntax AST：保留源代码结构、token range 和恢复节点，不决定规范类型身份。
- Bound layer：完成名字解析、overload candidate、SymbolId 和声明关联。
- Canonical Type graph：实习化不可变类型节点，提供稳定 TypeId。
- Semantic IR：在执行 lowering 前表达 typed Value、Place、Ref、Call、Drop 和 cleanup。
- Flow facts：在 CFG 上计算 definite assignment、move、loan、escape 和 reachability。
- Public contracts：从规范类型和 facts 投影出跨模块可见契约。
- ExecIR/ExecBC：只接收已经通过语义检查的确定操作，不反向定义语言语义。

现有 `SZrSemIrInstruction` 在迁移期间称为 execution SemIR sidecar；新前置 IR 使用独立结构，避免两个阶段共用同一表导致职责再次混合。完成迁移后再决定是否将旧 sidecar 重命名为 `ExecSemIR`。

## 4. Canonical TypeRef

### 4.1 TypeId 与类型节点

`TypeId` 是模块内/编译会话内对不可变规范类型节点的句柄。等价节点必须 intern 为同一 TypeId。

建议的规范节点类别：

```text
Error
Never
Primitive
Nominal
GenericParameter
GenericInstance
Array
Tuple
Union
Function
Ref
Owner
ReadonlyView
Nullable
```

核心节点信息：

| 节点 | 规范内容 |
|---|---|
| `Primitive` | 规范 primitive id，不使用显示名作为身份 |
| `Nominal` | domain-aware Canonical ModuleId + TypeDef token |
| `GenericParameter` | owner symbol + parameter ordinal |
| `GenericInstance` | generic definition TypeId + ordered argument TypeId |
| `Array` | element TypeId + rank + storage/layout category |
| `Tuple` | ordered element TypeId 列表 |
| `Union` | union TypeDef 或规范 variant 集合 |
| `Function` | parameter contract ids + return contract id + callable effects |
| `Ref` | pointee TypeId + readonly/writable access |
| `Owner` | unique/shared/weak/atomic-shared kind + target TypeId |
| `ReadonlyView` | target TypeId，表示不能通过当前 capability 修改对象 |
| `Nullable` | target TypeId；owner 是否可空仍由语言规则限制 |

这里的Canonical ModuleId不是裸字符串，而是第10章定义的`ModuleDomain + logical segments + package instance identity/version（若有）`。因此`Workspace:engine.render.Pixel`与`RegisteredNative:engine.render.Pixel`即使显示segments和TypeDef名相同，也必须是不同nominal identity；同一official native模块从builtin或descriptor plugin加载时则保持同一identity。

### 4.2 不进入 TypeId 身份的信息

以下信息不得直接 intern 到结构 TypeId：

- 局部 `LifetimeRegionId`。
- 某次 borrow 的起止位置。
- `scoped` 推导后的具体 block id。
- `out` 的当前初始化状态。
- source range、变量名和诊断 origin。
- 优化器推导出的常量和 numeric range。
- import原spelling、local alias与`.`/`/`分隔形式。
- provider kind/phase、DLL/source/`.zrm` locator、artifact generation和本地cache path。
- `zr.*`的N0-N3加载分级。

原因是这些信息是使用点或 flow-sensitive contract，不是结构类型身份。否则同一 `ref T` 会因所在 block 不同生成无限多个 TypeId，并把局部分析泄漏到 artifact ABI。

### 4.3 Semantic value type

每个 Value/Place 的语义类型使用 TypeId 加使用点约束：

```text
SemanticValueType
  typeId
  escapeUpperBound
  exactness
```

`Ref` 的 readonly/writable 属于 TypeId；`scoped` 属于 `escapeUpperBound`。`in/out` 是 parameter contract，不是可在任意类型位置构造的独立 TypeId。

### 4.4 Parameter 与 return contract

```text
CallableValueContract
  typeId
  passingForm: value | in | ref | refReadonly | out
  escapeUpperBound
  entryInitialization: required | uninitialized
  exitInitialization: unchanged | definitelyInitialized
  acceptsTemporary
  callSiteMarker: none | ref | out
```

规范化示例：

- `in T`：`Ref(readonly, T)` + function-scoped + accepts temporary。
- `ref T`：`Ref(writable, T)` + caller-scoped + `ref` marker。
- `ref readonly T`：`Ref(readonly, T)` + caller-scoped + `ref` marker。
- `out T`：`Ref(writable, T)` + function-scoped + entry uninitialized + exit initialized + `out` marker。

这样 callable contract 可以表达调用规则，而无需让 `out` 成为普通局部变量类型。

表层 delimiter 不进入 canonical callable identity：命名/匿名函数定义写 `fn name(...): R` / `fn(...): R => ...`，callable TypeRef 写 `fn(...) -> R`。Syntax AST 保留 `:`、`->`、`=>` 的 token/range 供 formatter、diagnostics 和 migration 使用；绑定后统一投影为 parameter contracts + return TypeId + receiver/effect contract。artifact loader、VM 和 AOT 不得从 delimiter 拼写推断 callable semantics。

### 4.5 Type definition capabilities

Nominal TypeDef 另有稳定 flags：

```text
valueType
gcClass
resourceClass
valueConstructible
readonlyType
refLike
hasDrop
hasGcReferences
gcScanKind: free | mapped | barriered
hasOwnershipFields
blittable
send
sync
```

标准库 `Span<T>`、`Unique<T>` 等通过这些 capability 和 TypeDef/token 注册成为普通类型消费者。shared compiler/runtime 路径不得比较名字 `"Span"` 或 `"Unique"`。

`valueConstructible` 表示 TypeDef 可作为 `init TypeRef(...)` 的静态 target，并关联可绑定的 constructor set。它不是“运行时值看起来像 prototype”的判断。type alias 和 constructed generic 通过 canonical TypeId 继承/实例化该能力；generic parameter 必须由公开 constraint 提供该能力。普通 runtime Type object 不能仅凭携带 TypeId 就进入核心 value-construction syntax。

`gcScanKind` 由 closed TypeLayout 递归计算，不能由用户 annotation 伪造。`free` 才能允许 inline slab/array segment 标记 NoScan；`mapped/barriered` 分别要求精确 pointer map 和 write barrier/card contract。

pool 等库可以注册 `StableSlotSourceContract`：identity TypeId、validate/acquire/release functions、ref projection、retirement policy和isolation domain。该contract只让普通call产生受guard约束的RefValue/region，不新增pool专用AST/Place/opcode，也不按`PoolHandle`名字分派。

## 5. Place IR

### 5.1 Place identity

每个可寻址存储使用 `PlaceId`。Place 由 base 和零个或多个 projection 组成：

```text
PlaceBase
  local | parameter | this | static | temporary | returnSlot | externalHandle

Projection
  field(fieldSymbolId)
  index(valueId)
  constantIndex(index)
  dereference
  unionVariant(variantId)
  tupleElement(index)
```

property 本身不是隐藏 field projection：

- 普通 getter lower 为 call，产生 Value。
- 普通 setter lower 为 call，消费 Value。
- ref-return getter lower 为 call，产生 Ref；对该 Ref 执行 dereference 后形成 Place。
- concrete property 不生成 backing field；accessor 内显式字段访问形成普通 `field(fieldSymbolId)` projection。
- ref-return property是getter-only；不存在通过PropertySet重绑定RefValue的SemIR形态。

### 5.2 Place overlap

borrow checker 必须能查询两个 Place 的关系：

```text
equal
disjoint
overlap
unknown
```

最低规则：

- 同 base、同 projection 链为 equal。
- 同 struct/tuple base 的不同确定字段通常 disjoint。
- 动态数组索引默认 unknown；只有 range/constant facts 能证明不同时才 disjoint。
- dereference、dynamic property、native alias 默认保守 overlap/unknown。
- union 不同 variant 在 variant 未被固定时 overlap。

该查询是共享基础，borrow checker、out definite assignment、partial move、optimizer 和 LSP 都复用。

### 5.3 Place 操作

Semantic IR 的内存操作必须显式：

```text
Load(place) -> value
Store(place, value)
Initialize(place, value)
Move(place) -> value
Copy(place) -> value
BorrowShared(place) -> refValue
BorrowMut(place) -> refValue
Reborrow(refValue, access, escapeBound) -> refValue
Drop(place)
```

`Load`、`Copy` 和 `Move` 不能由同一 opcode 根据类型名临时选择。Semantic type/copy capability 必须在 lowering 前决定操作是否合法。

## 6. 前置 Semantic IR

### 6.1 函数结构

```text
SemanticFunction
  symbolId
  callableContract
  locals
  places
  values
  blocks
  regions
  cleanupScopes
  sourceMap
```

每条 instruction 使用稳定 ValueId/PlaceId/TypeId，不直接使用 VM stack slot。stack slot、frame offset 和 register 分配属于 Exec lowering。

### 6.2 指令组

第一阶段至少需要：

- 常量与类型转换。
- Place construction 与 projection。
- load/store/init/move/copy/drop。
- borrow/reborrow/deref。
- typed call、virtual call、dynamic call boundary。
- branch/switch/return/throw。
- scope enter/cleanup/drop sequence。
- value construct、aggregate construct、field init、union construct。
- property get/set/ref-get 的已解析调用。
- destructuring source evaluation、shape validation、field/index projection和leaf initialize/move/copy。

动态调用仍可保留，但调用前后的所有权和逃逸契约必须显式；不能因为目标 dynamic 就跳过 owner/ref 校验。

`init TypeRef(...)` 在三层中保持明确边界：

```text
Syntax: StructInitExpression(TypeSyntax, Arguments)
Bound:  BoundValueConstruct(TypeId, ConstructorId, BoundArguments)
SemIR:  ValueConstruct(DestinationPlaceId, TypeId, ConstructorId, Arguments)
```

`ValueConstruct` 不能与 `Call`/`MetaCall`、`GcNew` 或 `OwnConstruct` 共用一个 `isNew`/ownership qualifier 组合节点。binder 只从 TypeRef namespace 和 `valueConstructible` capability 选择 constructor；普通 call binder 只处理 callable/`@call`。两条路径都不允许在失败时回退到另一条。destination-first lowering 负责 partial initialization facts、cleanup scope 和 hidden return destination，Exec lowering 才决定 frame offset/register/ABI 细节。

destructuring 不把RHS文本复制到每个leaf：先求值一次形成source ValueId/temporary Place，再构建binding plan。静态field/length错误在初始化任何local前报告；动态array执行一次`length >= N` shape check。leaf按源码顺序projection，并依据CopyKind/Place facts选择Copy或Move。若后续getter/projection可能throw，已经materialize的move-only temporary进入同一cleanup scope；用户可见binding只在对应Initialize完成后进入definitely-assigned facts。

### 6.3 非 SSA 起步

第一版不强制完整 SSA/phi。建议：

- instruction result 使用 ValueId。
- 可变状态通过 Place 和 block facts 表达。
- CFG join 使用数据流 lattice 合并，而不是先实现通用 phi insertion。
- AOT 优化阶段可以从已验证 SemIR 构建 SSA view。

这能先建立正确语言语义，同时不阻塞后续标量替换和范围优化。

## 7. CFG 设计

### 7.1 Block 与 edge

CFG block 持有 Semantic IR instruction range 和唯一 terminator，不再只持有单个 AST statement。

Edge 使用可扩展数组，并标记种类：

```text
normal
trueBranch
falseBranch
switchCase
switchDefault
exception
cleanup
return
suspend
resume
```

取消固定 `ZR_PARSER_CFG_MAX_SUCCESSORS = 2` 的语言语义限制。实现可以对常见二分支使用 small-vector 优化，但不能暴露为 CFG 能力上限。

### 7.2 Cleanup CFG

return、throw、break、continue 和构造失败必须显式经过所需 cleanup block：

```text
body -> cleanup(inner) -> cleanup(outer) -> return/throw target
```

所有 owner drop、using close 和部分构造清理由同一 cleanup plan 生成，禁止 compiler 与 runtime 各自维护第二份隐式顺序。

### 7.3 Suspension point

async/yield 的 suspension edge 在 borrow 检查前可见。跨 suspension 存活的 local 会进入 coroutine frame candidate；ref/ref struct/borrow 是否允许存入 frame 由类型 capability 和 region facts 决定。

## 8. CFG facts

### 8.1 分离的状态维度

不要把所有状态压入单一 ownership enum。每个 Place 的 block entry/exit state 至少分为：

```text
Initialization: uninitialized | initialized | maybeInitialized
Availability: available | moved | maybeMoved | dropped
Borrowing: sharedLoanSet + optionalMutableLoan
Escape: local | function | caller | heapStatic | unknown
Reachability: reachable | unreachable
```

分离状态可以正确表达“已初始化但已 move”“可能初始化但未借用”等组合，避免枚举状态乘积爆炸。

### 8.2 Loan facts

```text
LoanFact
  loanId
  sourcePlaceId
  access: shared | mutable
  regionId
  originRange
  lastUseRange
  createdByValueId
```

loan 的结束由最后使用点、显式 scope/escape 上界和 CFG 活跃性共同决定，不要求纯词法作用域一直存活到 block 结束。

### 8.3 Join

- initialization：所有前驱 initialized 才为 initialized；混合状态为 maybeInitialized。
- availability：所有前驱 available 才为 available；move/drop 混合为 maybeMoved/error candidate。
- loan：对可能活跃 loan 取保守 union，再用支配/最后使用信息缩短。
- escape：取能够容纳所有前驱的最宽上界。
- unreachable 前驱不参与可达状态 join。

### 8.4 Query 与 source map

semantic facts 以 ID 为主、source range 为查询索引：

- compiler 按 BlockId/PlaceId/ValueId 查询。
- LSP 按 document position 查询，再得到 SymbolId/PlaceId/TypeId。
- diagnostics 保存 primary range、related ranges 和 causal fact ids。
- AST node pointer 只作为当前会话便利索引，不作为 artifact 或增量缓存身份。

## 9. Artifact schema

### 9.1 三类产物职责

| 产物 | 规范职责 | 不应包含 |
|---|---|---|
| `.zrs` | 可读 syntax tree、token/source ranges、恢复节点 | 借用结论、runtime layout pointer |
| `.zri` | 可读 semantic/ExecIR 诊断产物、Type/Place/CFG/facts dump | 作为唯一可执行契约 |
| `.zro` | 稳定二进制模块、代码、公开类型/布局/调用契约 | AST pointer、局部 LoanId、编译器原始 C struct dump |

### 9.2 `.zro` section

建议采用显式版本和长度的 section：

```text
Header
ModuleIdentityTable
DependencyTable
StringHeap
TypeDefTable
TypeRefTable
TypeSpecTable
MemberDefTable
PropertyDefTable
SignatureHeap
ContractTable
LayoutTable
CodeTable
RelocationBindingTable
DebugMap (optional)
```

现有 metadata token/table/hash 机制继续使用，但新增语义必须成为正式 table/signature node，而不是附在某个 writer 私有字符串中。

- `Header`引用当前模块的domain-aware Canonical ModuleId、artifact schema/target和公开provider phase contract；phase用于可用性校验，不进入TypeId。
- `ModuleIdentityTable`结构化保存domain、logical segments和package instance identity/version，不能只保存display string。
- `DependencyTable`引用ModuleIdentity row并保存required public contract hash/capability/phase。selected provider kind、physical locator和artifact generation属于lock/debug/incremental resolution record；可发布`.zro`不得把它们并入ModuleIdentity或nominal TypeId。

### 9.3 Signature node

在现有 primitive/type-ref/type-def/array/tuple/func/generic/ownership/union/nullable 节点基础上，至少新增或规范化：

- `REF`：access flag + pointee signature。
- `READONLY_VIEW`：target signature。
- `OWNER`：取代含糊 ownership node，明确 unique/shared/weak/atomic-shared。
- callable parameter contract flags。
- callable receiver effect：readonly(`const fn`) 或 mutable(`fn`)。
- callable ref-export effect：none/ref-readonly/ref-writable；writable-ref view capability决定readonly receiver能否导出既有writable ref。
- TypeDef flags：value/gc/resource/readonly/ref-like/drop。
- TypeDef value-construction capability 与公开 constructor token/contract。
- TypeLayout `gcScanKind` 与可选 StableSlotSource capability/contract hash。

`scoped`、`in/out` 的公开逃逸和初始化约束进入 callable ContractTable；局部 region id 不进入 signature hash。

`.zrs` 区分 FunctionDefinition、AnonymousFunctionExpression 和 FunctionTypeSyntax，并保存各自 delimiter/source range；`.zri/.zro` 的 callable signature/contract 不保存“定义使用冒号还是类型使用箭头”作为类型身份。LSP 展示 declaration symbol 时从 syntax/definition projection 使用 `:`，展示纯 value type 时由 canonical TypeRef formatter 使用 `->`。

### 9.4 Hash 与兼容

分离三类 hash：

- type signature hash：结构类型身份。
- layout hash/version：size、alignment、offset、GC/ownership maps。
- callable contract hash：参数 passing、receiver/ref-export effect、escape/init contract。

任一公开契约不匹配时，loader 必须给出结构化 mismatch，不得降级为按名字绑定。旧 schema artifact 在正式切换时明确拒绝并要求重编译，不实现长期双格式执行。

`.zrs` 保留 `StructInitExpression` 的 `init` token、TypeSyntax、argument/source ranges；`.zri` 输出解析后的 TypeId、ConstructorId、argument contracts、destination Place 和 partial-init/cleanup facts；`.zro` 不保存 `$`/`init` 表层拼写，只保存跨模块需要的 TypeDef capability、constructor signature/token、layout 与 executable construct operation。loader 不得从 prototype 名字或 source spelling 重建 constructor 语义。

statement terminator与module import也遵守同一持久化边界：`.zrs`保存required/optional semicolon range、ImportExpression string literal和ModuleImportBinding source range；newline只作为trivia存在，不能被writer恢复成terminator。`.zri`保存import alias到domain-aware Canonical ModuleId/ModuleNamespace symbol的binding fact，并可在debug sidecar引用provider resolution。`.zro`只保存dependency ModuleIdentity、required phase/capability和公开contract hash，不保存local alias、分号、DLL/source绝对路径或provider注册顺序。source import与binary restore都必须按ModuleDomain创建等价readonly ModuleNamespace object，TypeRef qualifier只由原始import binding fact授予。

### 9.5 持久化边界

默认不写入 `.zro`：

- 局部 Place graph。
- block entry/exit initialization facts。
- loan origins/last-use。
- 局部 numeric range。

这些内容可以进入 `.zri`、debug sidecar 或编译缓存。`.zro` 只保存执行和跨模块验证所需投影，避免产物膨胀及编译实现细节成为 ABI。

## 10. 共享 API 边界

目标模块边界：

- `zr_vm_parser`：syntax AST、binding、Canonical Type graph、Semantic IR、CFG/facts、diagnostics。
- `zr_vm_core`：可执行 TypeLayout、metadata token/schema、runtime type descriptor、ExecIR/bytecode runtime view。
- `zr_vm_aot`：只消费规范 SemIR/ExecIR 和 layout，不读取 AST ownership flags。
- `zr_vm_language_server`：只消费 semantic query/facts，不重新解析 type-name string。
- `zr_vm_library`：通过 TypeDef capability/protocol 注册内建类型，不获得 compiler 特判入口。
- `zr_vm_cli`：负责产物选择、version error 和 dump 命令，不参与类型判断。

## 11. 里程碑

### M1 Type graph

目标：所有 parser/type inference/compiler 路径能以 TypeId 表示现有类型，不损失当前语义。

验收门：

- primitive、nominal、generic、array、tuple、union、function、nullable、owner 全覆盖。
- structural equality/hash/intern tests 完整。
- 不再新增 `typeName` 字符串分支。
- LSP 能从 TypeId 格式化当前类型。
- `init TypeRef(...)` target 可由 TypeId/capability/constructor set 完成绑定，runtime `zr.reflection.Type` expression 不进入该路径。

### M2 Place 与通用 CFG

目标：所有左值和控制流先映射为 Place/CFG。

验收门：

- local/field/index/deref/tuple/union projection 全覆盖。
- switch、exception、cleanup、多 successor 和 suspension edge 有直接测试。
- place overlap 测试覆盖 equal/disjoint/overlap/unknown。

### M3 前置 Semantic IR 与 facts

目标：compile lowering 不再直接从 AST 决定 load/store/move/borrow。

验收门：

- 每种支持语法都有预期 SemIR 指令 golden。
- definite assignment、move、loan、escape 的 block join 有负例。
- current execution SemIR sidecar 不再是语义来源。
- `ValueConstruct`、`Call/MetaCall`、`GcNew`、`OwnConstruct` golden 可区分且不存在 fallback。

### M4 artifact schema

目标：新类型和 public contract 可稳定跨模块绑定。

验收门：

- `.zrs/.zri/.zro` roundtrip。
- TypeRef/TypeSpec/Signature/Layout/Contract hash mismatch 精确诊断。
- unknown mandatory section、截断 blob、非法 token、超限 count 安全拒绝。
- source compile 与 binary import 产生同一 TypeId/public contract。
- OfficialNative/RegisteredNative/Workspace/Package ModuleIdentity roundtrip；同segments不同domain产生不同TypeId，同identity更换provider locator不改变TypeId。

### M5 consumers

目标：VM、AOT、LSP、reflection、debug 全部消费规范投影。

验收门：

- VM/AOT 结果和失败行为一致。
- LSP hover/signature/diagnostics 与 compiler 同源。
- reflection/layout 查询不回退到名字猜测。
- 全仓禁止新增 concrete built-in type-name dispatch。

## 12. 测试矩阵

### Parser/Semantic

- 深层泛型、tuple/union/array/ref/owner 组合。
- 同构类型 intern，相似但不同类型不碰撞。
- malformed/error type 在恢复模式下稳定传播。
- Place 投影与 source range 精确。
- 多分支、循环、异常、finally、cleanup、suspend CFG。

### Compiler/AOT/VM

- load/store/init/move/copy/drop 每个指令形态。
- local/field/array/return destination 的 `ValueConstruct` 与部分构造 cleanup。
- inline struct 与 reference local 的 frame lowering。
- dynamic call boundary 保留 owner/ref contract。
- deopt 后类型和 ownership state 不丢失。

### Artifact

- 大小端/字节宽度由编码定义，不写入裸指针。
- 零项、单项、大量 TypeRef/TypeSpec/Contract rows。
- duplicate/cyclic type signatures 安全处理。
- valueConstructible capability、constructor token/signature 与 construct operation roundtrip；旧 `$` schema/source spelling 不进入新 artifact。
- layout version/hash、call contract hash、module hash 独立失败。
- metadata pruning 不删除动态/反射/GC/layout 根。

### Stress

- 十万级 TypeId intern 和深层构造类型。
- 大函数、多 block、多 edge、嵌套 cleanup。
- 大量 source query 不退化为全表线性扫描。
- 重复写入/读取 artifact 后 hash 稳定。

## 13. 参考依据

- Rust 类型 intern 与 MIR Place/projection：`lua/rust/compiler/rustc_middle/src/ty/mod.rs`、`lua/rust/compiler/rustc_middle/src/ty/sty.rs`、`lua/rust/compiler/rustc_middle/src/mir/syntax.rs`。
- Rust borrow checker 对 MIR/CFG 的消费：`lua/rust/compiler/rustc_borrowck/src/lib.rs` 及 `lua/rust/tests/ui/borrowck`。
- Mono metadata table/signature 解码：`lua/mono/mono/metadata/metadata.c`、`lua/mono/mono/metadata/image.c`。
- .NET ref-like 类型公开契约：`lua/csharplang/proposals/csharp-7.2/span-safety.md`、`csharp-11.0/low-level-struct-improvements.md`。
- ZR 现有 metadata/layout 验证：`tests/module/test_metadata_type_ref_binding.c`、`tests/core/test_type_layout_metadata_contracts.c`、`tests/parser/test_aot_c_value_semir_contracts.c`。

ZR 的刻意差异是：第一阶段不要求完整 SSA，也不把 Rust region 名称暴露给用户；但类型、Place、CFG 和 artifact contract 必须先成为统一基础。
