---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_contract_validation.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/plans/aot/11-metadata.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_native_binding_direct_call.c
  - tests/library/test_official_provider_convergence.c
  - tests/ffi/test_native_extern_contract.c
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_call_binding_artifact.c
doc_type: reference
---

# ABI 与兼容性参考

ZrVm 没有一个可以代表所有兼容性的“版本号”。native provider、AOT module、artifact、FFI
contract、type layout、call binding、TestManifest、CLI manifest 和 `.zrm` package 各自有
独立版本轴与 hash。安全的集成策略是分别验证每个轴，再决定重新编译、重新绑定、拒绝加载
还是提示用户升级；绝不能只比较应用版本字符串后继续执行旧二进制。

本页给出当前 checkout 的版本常量和实际检查边界。C native module 的 authoring 顺序见
[Native Module 编写](../05-interop/native-module-authoring.md)，descriptor 字段与 registry
状态机见 [Native Provider Descriptor 深度参考](../03-modules/native-provider-descriptor-reference.md)。

## 1. 版本轴总览

| 轴 | 当前常量/标识 | 保护的对象 | 失配后的正确动作 |
| --- | --- | --- | --- |
| native runtime ABI | `ZR_VM_NATIVE_RUNTIME_ABI_VERSION = 4` | host runtime 对 provider 的最小服务面 | 拒绝 provider 或升级 runtime |
| native plugin ABI | `ZR_VM_NATIVE_PLUGIN_ABI_VERSION = 6` | `ZrLibModuleDescriptor` / plugin entry 的 ABI | 拒绝加载 plugin |
| native module info | `ZR_NATIVE_MODULE_INFO_VERSION = 1` | module information record | 按 loader 支持度拒绝 |
| AOT ABI | `ZR_VM_AOT_ABI_VERSION = 16` | generated AOT module 与 loader 的 struct/function contract | 重新生成 AOT module |
| artifact schema | `ZR_ARTIFACT_SCHEMA_VERSION = 5` | `.zrs/.zri/.zro` container/readers | 重新编译 artifact |
| FFI contract schema | `ZR_FFI_CONTRACT_SCHEMA_VERSION = 4` | native import contract 结构 | 重新生成/验证 native import |
| FFI ABI model | `ZR_FFI_CONTRACT_ABI_MODEL_VERSION = 3` | target ABI、layout、marshal model | 重新生成 target-specific binding |
| type layout | `ZR_TYPE_LAYOUT_SCHEMA_VERSION = 2` | inline layout、GC scan、ownership map | 重新 materialize type / rebuild output |
| call binding | `ZR_CALL_BINDING_SCHEMA_VERSION = 1` | call-site contract/cache row | 重新 resolve target / rebuild artifact |
| TestManifest | `ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION = 1` | test entry/case encoded data | 重新编译 Test phase |
| CLI manifest | `ZR_CLI_MANIFEST_VERSION = 1` | CLI incremental metadata | 删除/重建 cache，不手改 |
| package format | `zr.zrm/v1` | archive manifest/entry conventions | 用兼容 packer/rebuild package |

这些数字没有可比较的大小关系。比如 AOT ABI 从 16 变到 17 不意味着 native plugin ABI 也
应从 6 变到 7；artifact schema 保持 5 也不意味着一个 layout hash 仍可接受。

## 2. Native provider ABI

所有 native module/provider 都通过 `ZrLibModuleDescriptor` 声明身份。最小兼容字段为：

| 字段 | 作用 |
| --- | --- |
| `abiVersion` | plugin 自己面向 runtime 的 ABI 版本，必须匹配 native plugin ABI |
| `moduleName` | canonical module identity，不能在加载时改名 |
| `moduleVersion` | 人类/依赖层版本信息，不替代 ABI/hash |
| `minRuntimeAbi` | provider 所需的最低 native runtime ABI |
| `requiredCapabilities` | provider 功能位，例如 FFI runtime capability |
| `providerPhase` | Runtime、Test、CompileTool 消费边界 |
| `publicContractHash` | 公开 API surface 的稳定身份 |
| `providerContractRole` | 官方 canonical provider role |
| `canonicalTypeRoles` | 类型投影/role 的唯一性和可验证性 |

典型静态 provider descriptor 采用公共常量，而不是把数字散落在源文件中：

```c
#include "zr_vm_common/zr_abi_conf.h"
#include "zr_vm_library/native_binding.h"

static const ZrLibModuleDescriptor kExampleModule = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "example.metrics",
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .requiredCapabilities = 0U,
    .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
    .publicContractHash = "<generated-public-contract-hash>",
    /* constants/functions/types/callbacks and remaining fields */
};
```

真实 provider 必须填充其公开 functions/types/metadata；示例中的 hash 不是可复制的字面量。
registry 先 attach 到 global，再 validate/register descriptor。失败可通过
`ZrLibrary_NativeRegistry_GetLastErrorCode` 和 `GetLastErrorMessage` 获取，错误分类包括 ABI、
version、capability、module name、phase、reserved official module、duplicate provider、provider
contract、canonical type role 和 module-in-use。不能在 registry 拒绝后继续缓存 callback 指针。

## 3. Module version、contract hash 与 generation 的区别

```text
moduleVersion         : 人类可读的发布/依赖版本
publicContractHash    : module surface 的语义 identity
module signature hash : descriptor-derived compiler call-binding identity
provider generation   : 当前 registry/materialization revision
```

它们解决不同问题。`moduleVersion` 相同但函数参数、passing mode、layout 或 type role 改变时，
`publicContractHash`/signature hash 必须变化；hash 相同但 plugin 被 unload/reload 时，provider
generation 仍会变化。已编译 call binding 必须在新 generation 下重新 resolve，而不是因为
版本字符串未变就直接调用旧 native target。

registry 提供 `ZrLibrary_NativeRegistry_ComputeModuleSignatureHash`、
`ResolveCallBinding` 和 `GetCallBindingIdentity`。descriptor plugin source 失效需要通过
`InvalidateDescriptorPluginSource` 走 registry 状态机；若 owner ref count 非零，预期错误是
`MODULE_IN_USE`，不是强制卸载。

## 4. Artifact 与 layout 兼容

artifact public identity 同时包含 TypeRef/TypeSpec/signature token/hash、layout version/hash、
callable contract hash 和 module hash。加载顺序应是：

```text
artifact bytes
  -> container/schema/section validation
  -> public identity validation
  -> type layout and metadata contract validation
  -> native import/provider/call-binding resolution
  -> execute or materialize
```

`layoutHash` 保护 byte size、alignment、GC scan、ownership map 等会影响机器代码/inline span
的事实；`callableContractHash` 保护 passing mode、return、receiver/effects；`moduleHash` 保护
声明来源。任意一个 mismatch 都应该得到具体 `EZrArtifactStatus` 并丢弃旧 artifact。详见
[Canonical Artifact 二进制 Schema](artifact-binary-schema-reference.md)。

## 5. AOT ABI

generated AOT module 通过 `ZrAotCompiledModule` 描述，核心字段包括：

| 字段 | 兼容性含义 |
| --- | --- |
| `abiVersion` | 必须等于 `ZR_VM_AOT_ABI_VERSION` |
| `backendKind` | None/C/LLVM，决定加载和调试语义 |
| `moduleName` | 目标 module identity |
| `inputKind` / `inputHash` | source 或 binary 输入的身份 |
| `runtimeContracts` | 需要的 runtime contract 集合 |
| `embeddedModuleBlob` | 可选 canonical module bytes |
| `functionThunks` / `entryThunk` | 已生成的执行入口 |
| `methodInfos` / `typeLayouts` / `gcDescriptors` | 调用、布局和 GC root 映射 |
| `nativeImportContracts` | native extern ABI/ownership/marshal agreement |
| `callBindingRows` / target indices | 已验证 call-site 绑定投影 |

AOT export 只保证符号可见性，不能跳过 version/identity 验证：

```c
typedef const ZrAotCompiledModule *(*FZrVmGetAotCompiledModule)(void);
```

loader 获得 module 后必须校验 ABI、backend、input hash、contracts、layouts、native imports 和
call bindings。`SZrAotCodeRegistration` 的 function pointers、metadata tokens、type layouts、
GC descriptors、native import range 与 call binding target indices 是相互关联的平行数组；
生成器和 loader 不得只替换其中一个数组来“热修复”模块。

## 6. FFI compatibility 不是 plugin ABI

`native extern` 的 `SZrNativeImportContract` 还包含独立的 FFI schema/model。它记录：

| 类别 | 关键事实 |
| --- | --- |
| target | ABI（system/C/stdcall）、pointer size、endianness、target triple、target ABI hash |
| types | kind、size、alignment、canonical type hash、layout hash、aggregate fields |
| parameters | in/ref/out、marshal kind、ownership、nullable |
| callable | passing form、escape upper bound、entry/exit initialization、marker、receiver/effects/hash |
| cleanup | charset、error policy、cleanup policy |
| callback | lifetime、thread policy、exception policy |
| availability | Windows/Unix mask 和 required capabilities |

FFI contract hash 用确定性散列覆盖 callable 和 signature 内容。相同的 C symbol 名并不足以
证明兼容：calling convention、target pointer size、struct offset、ownership、callback lifetime
和 error policy 任意一项改变都可导致 ABI 不兼容。调用方案见 [FFI Contract](../05-interop/ffi-contract.md)
和 [FFI Runtime Handle 深度参考](../03-modules/ffi-runtime-handle-reference.md)。

## 7. 兼容决策表

| 观察到的变化 | 是否可继续复用 | 处理方式 |
| --- | --- | --- |
| CLI 表层版本变化，但 artifact/ABI/hash 都匹配 | 可能 | 仍走正常 loader validation |
| artifact schema 不支持 | 否 | 用当前 compiler 重新生成 |
| layout hash 不同 | 否 | 重新 materialize/rebuild，不解释旧 inline bytes |
| call-binding generation stale | 否，旧 target 不可用 | 重新 resolve binding |
| provider moduleVersion 变而 contract/signature 未变 | 视 resolver policy | 仍验证 phase/capability/generation |
| public contract/signature hash 变 | 否 | 重编译消费方并重新注册/resolve |
| AOT ABI 不同 | 否 | 重新生成 AOT output |
| FFI target triple/pointer size/ABI hash 变 | 否 | 重新生成对应 target 的 binding |
| TestManifest schema 变 | 否 | 重编译 Test phase |

“重新生成”应从 source/manifest 和当前 descriptor inventory 出发。不要通过十六进制编辑 schema
字段、复制 hash、降低 minRuntimeAbi 或屏蔽 error code 来伪造兼容性；这些手段会把明确的
加载失败变成不可定位的 memory corruption 或错误调用。

## 8. 发布与升级检查表

1. 修改 public native descriptor、type layout、calling convention 或 FFI contract 后，更新其
   contract hash，并重建依赖 artifact/AOT output。
2. 修改 native plugin ABI struct/entry 语义时，提升 `ZR_VM_NATIVE_PLUGIN_ABI_VERSION` 并让
   loader 明确拒绝旧插件。
3. 修改 runtime 向 provider 提供的能力时，审查 `minRuntimeAbi` 与 required capability；
   不要仅升级 `moduleVersion`。
4. 修改 artifact encoding 或 section 语义时，提升 artifact schema 并保持 reader 对未知
   mandatory section 的拒绝行为。
5. 每次升级应覆盖静态 provider、descriptor plugin、artifact read/write、AOT registration、
   FFI contract 和 cache invalidation 测试。

当前版本值以公共头文件为准，Wiki 只解释为什么需要分别检查它们。跨语言 host 还应阅读
[Rust Binding](../05-interop/rust-binding.md)，因为 Rust wrapper 的 opaque-handle status
mapping 不是 C plugin ABI 的替代品。
