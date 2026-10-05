---
related_code:
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c
  - zr_vm_core/src/zr_vm_core/reflection_token_resolve.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-10-04 全仓库公共注释与调用契约审查，仅补文档与静态证据
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/aot/index.md
  - docs/plans/aot/02-typed-value-and-layout.md
tests:
  - tests/core/test_aot_gc_root_frame.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/parser/test_aot_c_type_layout_contracts.c
doc_type: api-reference
---

# AOT ABI v17

本页保留 AOT runtime 结构速查；native plugin、artifact、FFI、layout、call-binding 和 AOT
之间的独立版本轴及升级决策见 [ABI 与兼容性参考](../06-reference/abi-compatibility-reference.md)。
生成 frame、根表、direct call/deopt、异常 cleanup 和 module registration 的行为细节见
[AOT Lowering、运行时 Helper 与注册参考](../10-aot-lowering-registration-reference.md)。

当前 `ZR_VM_AOT_ABI_VERSION` 为 **17**。AOT module 载荷必须声明 `abiVersion`、`backendKind`
（C=1，LLVM=2）、`inputKind`（source/binary）、moduleName、inputHash 和 runtimeContracts；
loader 在挂载描述符前检查 ABI、backend、module identity 和表形状；inputKind 选择外部 source/binary 的哈希路径。runtimeContracts 是生成端发布的需求名称表，当前描述符门禁没有遍历它来证明能力满足。

## 核心结构

| 结构 | 作用 |
| --- | --- |
| `SZrAotSignatureType` / `SZrAotSignature` | 参数/返回 base type、static C type、ownership、nullable、array、passing mode |
| `SZrAotMethodInfo` | function index、metadata function、frame bytes、GC root map、signature、generic dictionary、reflection invoker |
| `SZrAotGcRootSlot/Map` | frame byte offset 或 local address 的 root 描述 |
| `SZrAotGenericSlot/Dictionary` | 发布泛型需求与缓存槽；当前 helper 消费 layout、method、sizeof，prototype/box 仍为保留描述 |
| `SZrAotCodeRegistration` | thunks、method/token/layout/GC descriptors、native imports、call-binding rows |
| `ZrAotCompiledModule` | loader 可见的完整模块和 entry thunk |

`EZrAotParameterPassingMode` 必须与 parser/native descriptor 一致：VALUE、IN、REF、
REF_READONLY、SCOPED_REF、SCOPED_REF_READONLY、OUT。`SZrAotGcRootMap` 不能把短生命周期
临时 C 局部误标成 frame root；local address 只在生成函数的 safepoint window 有效。

当前 C emitter 的 thunk、method-info 和 method-token 表共享 function-index
索引空间。裁剪保留的空洞分别写入 `NULL`、`NULL`、`0`；相关 count 表示可寻址
索引范围，不能改成非空条目数量，也不能独立压缩任意一张表。这是当前 C 生成端的
表关联契约，不表示本轮已验证所有外部生成器或模块卸载生命周期。

## 生成与加载

```text
canonical Type/Place/CFG/SemIR
  -> ExecIR + reachability/metadata trim
  -> C emitter 或 LLVM emitter
  -> ZrAotCompiledModule + registration tables
  -> ZrLibrary_AotRuntime_ConfigureGlobal
  -> module loader 验证并挂接 call binding/layout registry
```

生成代码通过 `ZrLibrary_AotRuntime_BeginInstruction`、`CopyStack`、`GetStack`、
`CreateClosure`、`MetaGet/Set`、ownership helpers 和 generic dictionary 与 VM runtime 共享
语义。不能在 backend 重新依据源码类型名选择 helper；未知/不支持指令必须显式 deopt 或
返回 artifact status。

## 兼容性和反射

ABI version、module input hash、layout hash、native import contract hash、call-binding row
size 任何一个不匹配都拒绝加载。reflection metadata level 可为 NONE、RUNTIME_MAPPING 或
DESCRIPTION；裁剪后的 method/type 没有 preserve rule 时反射查询返回 metadata-not-preserved，
而不是构造一个空 descriptor。VM/AOT parity 测试应覆盖异常、GC root、ownership drop、动态
dispatch、generic specialization 和 tail call。

## 当前公开字段的消费边界

描述符、数组、字符串和 thunk 指针由生成动态库持有；loader 与模块挂载借用这些表，表及被引用存储须在使用期间有效。`inputHash` 对应外部 source 或 binary 输入，不是嵌入 blob 的校验和。精确 ABI 比对为 `zr_vm_library/src/zr_vm_library/aot_runtime.c:649`；这不授所有字段已经完整校验或所有哈希失败路径关闭的保证。

签名发布 baseType、static C type、ownership、nullable、array 与 passing mode。当前 token 反射入口主要检查 VALUE/baseType、固定参数前缀、varargs 和返回标签，再调用 void invoker（`zr_vm_core/src/zr_vm_core/reflection_token_resolve.c:390`）。ABI 没有 argCount；调用方仍须提供有效参数存储，发布的描述字段不能扩大为该入口已完成的全部语义验证。

GC root 的 `frameByteOffset` 已由生成端计入字段偏移；扫描器按 locationKind 读取 VM 值或本地对象指针，不应再次加 field offset。LOCAL_ADDRESS 没有 VM 栈范围检查，map、frameBase 与槽存储的寿命须覆盖根帧使用期。泛型字典当前公开 helper 消费 layout、sizeof 与静态 method；prototype/box 的保留视图不表示解析已经实现，静态 resolved cache 也没有多 runtime 隔离或并发安全保证（`zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c:143`）。

callBindingRows 是 artifact schema 的固定行宽字节，不能作为宿主结构体数组解释；挂载端逐行解码（`zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c:193`）。nativeImportRanges 则按 functionIndex 分割连续 nativeImportContracts：函数内 localContractIndex 在范围内校验后转为全局索引。其范围计数和编码行的目标索引数组承担不同关系，不得互换。

本段仅澄清当前 producer/consumer 契约；前述 parity 测试仍是覆盖要求，不是本次运行结果。本次没有执行 ABI、GC、反射、装载或 native 测试。
