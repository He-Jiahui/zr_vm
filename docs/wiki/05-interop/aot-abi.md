---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
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
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/aot/index.md
  - docs/plans/aot/02-typed-value-and-layout.md
tests:
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/parser/test_aot_c_type_layout_contracts.c
doc_type: api-reference
---

# AOT ABI v16

当前 `ZR_VM_AOT_ABI_VERSION` 为 **16**。AOT module 载荷必须声明 `abiVersion`、`backendKind`
（C=1，LLVM=2）、`inputKind`（source/binary）、moduleName、inputHash 和 runtimeContracts；
loader 在任何函数调用前校验这些字段。

## 核心结构

| 结构 | 作用 |
| --- | --- |
| `SZrAotSignatureType` / `SZrAotSignature` | 参数/返回 base type、static C type、ownership、nullable、array、passing mode |
| `SZrAotMethodInfo` | function index、metadata function、frame bytes、GC root map、signature、generic dictionary、reflection invoker |
| `SZrAotGcRootSlot/Map` | frame byte offset 或 local address 的 root 描述 |
| `SZrAotGenericSlot/Dictionary` | type layout、prototype、method、box、sizeof 延迟解析 |
| `SZrAotCodeRegistration` | thunks、method/token/layout/GC descriptors、native imports、call-binding rows |
| `ZrAotCompiledModule` | loader 可见的完整模块和 entry thunk |

`EZrAotParameterPassingMode` 必须与 parser/native descriptor 一致：VALUE、IN、REF、
REF_READONLY、SCOPED_REF、SCOPED_REF_READONLY、OUT。`SZrAotGcRootMap` 不能把短生命周期
临时 C 局部误标成 frame root；local address 只在生成函数的 safepoint window 有效。

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
