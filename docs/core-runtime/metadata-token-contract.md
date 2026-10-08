---
related_code:
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c
  - zr_vm_core/src/zr_vm_core/function_metadata_binding.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c
  - zr_vm_core/src/zr_vm_core/reflection_generic_instance.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_spec.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c
  - zr_vm_core/src/zr_vm_core/function_metadata_binding.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - "user: 2026-10-05 metadata_token.h 全 114 单元注释审查与有限集成"
  - docs/code-review/comment-standard.md
tests:
  - tests/module/test_metadata_token_model.c
  - tests/module/test_metadata_type_ref_binding.c
  - tests/module/test_metadata_runtime_binding_compatibility.c
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_ssa_source_script_entry_tokens.c
  - docs/acceptance/ssa-source-script-entry-tokens.md
doc_type: module-detail
---

# 元数据 token、记录与绑定契约

## 用途与范围

`metadata_token.h` 提供编译器、source IO、模块绑定、反射和格式读取器共享的身份编码与进程内记录类型。token 用于在所属函数或模块的元数据域里找记录、表达签名与目标约束。可执行目标仍由适用调用契约和链接路径建立。

本页说明该 header 的 114 个审查单元：8 个类型、24 个枚举值、66 个真实成员、13 个宏（3 个 callable、9 个值宏和 1 个 include guard）、不透明字符串前向声明及 7 个既有 TODO 决策。相关源码仅作为所述操作的依赖证据；本页不授予其他 metadata format header 或整个 runtime 的审查完成信用。

## 编码与表命名空间

token 的高 8 位是表号，低 24 位是 RID。零 token 表示缺席；非零 token 也可能编码 RID 0。`MAKE` 各求值一次后截断输入，接受未知表号或零 RID；生产者负责容量检查，消费者负责适用表、非零 RID、范围与实际记录检查。不同表或不同元数据域中相同 RID 不构成同一实体。

| 表 | 实际用途 | 消费边界 |
| --- | --- | --- |
| MODULE | 编译器从 RID 1 产生模块与配对签名，AssemblyRef 可映射到它 | 模块身份不授予入口调用 |
| TYPE_DEF | union 定义或 callable prototype owner | 正文和 `reserved0` 区分用途 |
| MEMBER_DEF | typed export、callable 常量、child 或受限脚本入口 | 方法与字段共用表，须另核签名及用途 |
| ASSEMBLY_REF | 导入模块身份与可选版本约束 | 可解析为不同表的 MODULE，token 数值不必相同 |
| TYPE_REF | 名称或明确 provider TYPE_DEF 目标约束 | 当前 TypeRef 绑定统计只处理 TYPE_DEF 目标 |
| MEMBER_REF | 导入成员或 MethodSpec 基方法引用 | 需解析 provider 身份及签名 |
| TYPE_SPEC | 泛型、array、nullable、ownership 等额外类型构造 | hash 筛选后仍核完整 blob，union 定义与布局另核 |
| SIGNATURE | 实体配对或独立 typed-call witness | hash-only witness 不要求结构 blob，不能直接执行 |

artifact 签名的 TYPE_DEF 谓词只核 token 非零、表号等于 TYPE_DEF 与 RID 非零。这个门禁不查询目标记录，也不证明引用已经解析。

## 签名字节的适用读取器

节点码说明后续字节应如何解释。根节点与类型子节点的允许集合不同；`INVALID` 和预留 `FUNC` 在当前结构读取路径中被拒绝，方法根使用 `METHOD_SIG`。

普通类型节点支持 primitive 编号、type ref/def、rank 加元素类型的 array、count 加有序类型列表的 tuple、generic base/arguments、ownership qualifier、union 和 nullable 子类型。`METHOD_SIG` 与 `FIELD_SIG` 是根正文格式，不能当参数类型子节点。generic 类型实例基可用 TYPE_REF/TYPE_DEF；MethodSpec 专用读取要求 MEMBER_REF 基节点。

同一节点码存在专属富正文。普通 TYPE_DEF 类型读取期待基础类型编号和名称索引；compiler union 定义从名称索引及 generic arity 开始，再写 variant 契约，由专用定义消费者读取。导入 MEMBER_REF 正文写模块名、成员名、effect kind 并可附目标签名；ASSEMBLY_REF 写模块名与三个版本字符串引用。普通结构读取对后两码只处理一个 u32，不能用于替代导入正文 decoder。MODULE 正文保存模块名与版本引用，模块 ABI hash 另存。

## 记录、配对与索引域

`SZrMetadataTokenRecord` 同时承载普通实体/签名 pair 与用途标记记录。普通 pair 的 `relatedToken` 互指，签名行的 `ownerToken` 回指实体；签名查询还核这个关系。CALLABLE_OWNER、CALLABLE_CHILD 与 hash-only CALLABLE_SIGNATURE 不保证这种配对形态。

`signatureBlobOffset` 引用所属函数的 signature heap，偏移 0 可以有效。结构消费路径要求 heap 存在、长度非零、offset 小于 heap length，且 length 不超过 `heapLength - offset`。零长度 hash-only witness 被该 blob 路径拒绝，但仍可用于自己的 typed-call 契约。`signatureHash` 可属于普通 blob、callable 结构或 prototype layout guard 的不同协议，不能跨域互换或据非零值推定唯一。

`ownerIndex` 的数组域由用途决定。普通 export/effect pair 的 SIGNATURE 行继承相同序号；它们并非统一无槽。导入 effect 索引按 module entry effects，再按 exported callable summaries 的 effects 展开，只计适用 IMPORT_REF/READ/CALL 且具备模块名/成员名的项。

| `reserved0` 标记 | `ownerIndex` 与身份关系 |
| --- | --- |
| CALLABLE_CONSTANT | 本函数常量槽；member 与 signature 行可共享标记 |
| CALLABLE_OWNER | 本函数 prototype 槽，TYPE_DEF 行的 signatureHash 是 layout guard |
| CALLABLE_MODULE | 模块 relocation 的 targetIndex，供适用链接/dispatch guard 核对 |
| CALLABLE_SIGNATURE | hash-only typed-call witness，显式使用无槽哨兵，无结构 blob |
| CALLABLE_CHILD | 本函数 childFunctionList 槽，constant targetMetadataToken 指向局部 child 行 |
| SCRIPT_ENTRY | 受限无参数 i64 脚本入口及其签名，独立使用无槽哨兵 |

普通 pair 的 `reserved0` 初始为零。Binding 类型同名的 reserved 字段是 UInt32 序列化保留值，不承载这些 marker。

## CHILD 发布与 whole-definition 路径

编译入口先组装 children 并重绑常量，再 finalize call bindings。无显式 alias 且未被现有 definition 阻止时，重绑要求恰好一个 child 满足 whole-definition 比较；该比较有自己的共享 buffer shortcut TODO，不能泛化为完整身份证明。随后 module publisher 只在目标指针已经等于本函数某个 child 时建立局部显式边。

消费 alias 时，先按 constantIndex 找唯一 MEMBER_DEF/CALLABLE_CONSTANT，重复常量定义会被拒绝。随后核显式 target token、CHILD marker、signatureHash 相同及 child 索引边界，才返回借用 child 槽。显式 alias 不普遍重复 whole-definition 比较；marker/hash 不构成跨函数或公开 callable 身份。

## sidecar 绑定与版本约束

`SZrMetadataTokenBinding` 由 caller 函数持有，非零 `refToken` 是 Upsert/Find 键。Find 借出数组行；Upsert 同键直接复用，不重置旧值。扩容复制旧行、清零新存储并释放旧 native 数组，旧 binding 指针随扩容失效。

`refSignatureToken/Hash` 保存 caller 引用正文，TypeRef 的 `expectedSignatureHash` 则保存 provider 目标约束，可与引用正文 hash 不同。TypeSpec 使用 caller 规范身份，TypeRef 使用目标定义，MemberRef 使用 effect 目标与 typed export 实际值，AssemblyRef 允许映射到 provider MODULE1。MemberRef producer 不写 layout 字段，因此不能声称它重置了复用行的旧 layout。expected/resolved token 可属于不同域；兼容检查对合法 AssemblyRef/Module 或 TypeSpec 映射放宽 token 数值等值，但仍执行适用 hash/layout 检查。

可选 expected token/hash/module hash 通常非零才启用对应门禁。任一侧 layout version/hash 非零会启动 version 与 hash 两项比较。TypeRef binder 要求非零目标签名 hash；runtime layout-view 路径对该目标 hash 的零值有自己的可选约束规则，不能跨入口合并语义。跨模块泛型解析还读取 ref 身份、重新查 provider 实体与配对签名并核完整 blob，sidecar 存在不保证随后仍兼容。

AssemblyRef 的三版本字符串只在 effect 对应字段缺失时补齐。当前区间兼容器仅在 min/max 都存在且 actual/min/max 都可解析时核 `actual >= min` 和 `actual < max`；缺端点或无法解析时该门禁放行。requested 版本的存储与回退本身不建立通用 exact-version 拒绝保证。

## 统计与首个失败样本

TypeSpec status 统计适用 caller TYPE_SPEC；TypeRef status 只统计 TYPE_REF 且目标表为 TYPE_DEF 的项。非空 status 在入口清零，首个失败样本按扫描顺序保留。缺配对签名或 sidecar 分配失败可提前返回，计数可能只是已扫描前缀，先前成功项不会回滚。

TypeSpec unmatched 表示未找到 provider hash/blob 对应项，其首 hash 是 caller TypeSpec 正文；definition mismatch 来自 union 一侧定义缺失或 hash/完整 blob 不同，layout mismatch 比较 union TypeDef 布局。无状态 wrapper 容忍单纯 unmatched，但不能由这个策略证明后续硬失败未被前缀状态掩盖。

TypeRef unmatched 表示未找到目标 provider TYPE_DEF，其首 hash 是 `targetSignatureHash`。definition mismatch 包括目标 hash 缺失/不同、可选 signature/module 约束或基本名不匹配；layout mismatch 在基本名比较之前分类。两种 status 同名成员必须按真实参数类型读取。expected/actual 样本与首失败 token 同分支写入；无该类别失败时保留入口零值，hash 相同仍可能因为其他身份或 blob 条件失配。

## 字符串、持久化与待闭合边界

stringIndex 是内容稳定 hash 的低 31 位加一，非空字符串编号非零；空字符串不加入。builder 检查同编号异内容冲突并去重，lookup 按编号遍历 entries，编号不是数组下标。entries 借用 `SZrString*`；Free 释放 native 数组不等于释放这些 GC 对象。

source writer 逐字段保存记录并按内容写字符串，source loader 按版本重建字段/字符串；runtime 安装复制 native 数组及指针值。native ZRP/AOT tokenRecords 使用 `sizeof(record)` 复制，是另一格式边界。

当前具体 TODO 分为三类：

- 核 AOT compacted retained-count 加一构造 RID 的输入上界，并以 24 位边界核 token 唯一性。
- 核 native ZRP 发布含非空 requested/min/max 字符串的记录时，宿主地址是否被清除或重建，以及跨进程 reader 约束。
- 核 metadataStringHeap 和三版本字符串在 gc_mark、gc_cycle 的 mark/跨代/移动转发及 loader/runtime 安装边界中的根保持。已读有限遍历分支不构成完整 GC 可达证明。

这些 TODO 没有完整 legal trigger 和实际坏结果复现，保持 TODO；未对行为做修复，也未将外部 ABI 未观察入口判为死代码。

## 验证与计划来源

本次来自 2026-10-05 的全 header 注释审查和有限集成要求。独立审查接受全部 114 单元、逐成员类型归属、普通 pair/hash-only witness 区别及上述 7 个 TODO 单元决策。验证包括原 138 条非注释物理行精确字节、当前有意义单行锚点、依赖原始范围、严格补丁检查及仓库外同补丁实际回放；不包含新的运行测试或 native 执行信用。

现有测试入口分别覆盖 token/绑定模型、TypeRef/TypeSpec 统计、运行时兼容性、typed-call metadata 与受限 script-entry metadata。已有 script-entry acceptance 文档只提供其记录的有限 metadata inspection gate；不能据此推定当前 whole runtime 或所有未闭合 TODO 已验证。详细逐单元约束见 [canonical coverage](../code-review/coverage/zr_vm_core_metadata_format_headers.tsv)。
