---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/native_binding_call_binding.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_contract_validation.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/10-native-ffi-module-package/m2-artifact-provider-phase.md
  - docs/plans/syntax/10-native-ffi-module-package/m2-v2-manifest-admission.md
tests:
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_native_registry_descriptor_invalidation.c
  - tests/library/test_official_provider_convergence.c
  - tests/library/test_native_binding_direct_call.c
  - tests/parser/test_typed_call_binding.c
doc_type: api-reference
---

# Native Provider Descriptor 与注册表参考

Native provider 是由 C/C++/Rust 宿主向 ZR 发布 module、type、function、method、metadata role
和 callable contract 的机制。它不是 `native extern` 的替代语法，也不是把一个 C 函数指针塞入
全局表：provider 必须通过 `ZrLibModuleDescriptor` 描述公开 surface，经 registry 验证、phase
admission 和 call-binding identity 后，才可被 compiler/VM/AOT/LSP 使用。

本页面向 provider 作者。源级 `native extern` 见[模块导入与 Native FFI](../02-language/module-import-native-ffi-reference.md)，
callback 实现细节见[Native callback 配方](../05-interop/native-callback-recipes.md)。

## Provider 生命周期

```text
static/plugin descriptor
    -> Attach registry to Global
    -> Validate descriptor and public contract
    -> Register module / materialize on demand
    -> Resolver/compiler builds canonical module + call contracts
    -> dispatcher invokes callback with ZrLibCallContext
    -> owner references drain
    -> invalidate plugin source or free Global registry
```

`ZrLibrary_NativeRegistry_Attach(global)` 创建/安装 registry；它应在注册官方或自定义 module
之前完成。`ZrLibrary_NativeRegistry_Free(global)` 通常由 global shutdown 生命周期管理，不能
在仍有 module owner/call binding/worker 使用 descriptor 时提前调用。

动态 descriptor plugin 还多一个安全边界：

```text
EnsureProjectDescriptorPlugin(state, projectDirectory, moduleName)
    -> validate/load descriptor plugin
    -> registry records source path and ownerRefCount
InvalidateDescriptorPluginSource(global, sourcePath)
    -> only succeeds when ownerRefCount == 0
```

测试明确覆盖“module in use 时拒绝 invalidation”。热重载逻辑不能通过强行卸载仍被 compiled
function 或 task 持有的 plugin 来规避引用计数。

## `ZrLibModuleDescriptor` 是什么

公开 descriptor 以字段数组描述 module surface：

| 字段族 | 用途 | 常见错误 |
| --- | --- | --- |
| ABI/name/version | `abiVersion`、`moduleName`、`moduleVersion`、`minRuntimeAbi` | 名称/ABI 不匹配、将 host-private version 当公开 contract。 |
| exports | constants、functions、types、module links | count 与数组不匹配、重复/冲突的导出。 |
| metadata | type hints/JSON、documentation、attribute roles | 仅写 documentation 而不提供可验证 role/schema。 |
| admission | `requiredCapabilities`、`providerPhase`、provider contract role | runtime host 消费 test/compile-only provider。 |
| canonical roles | `canonicalTypeRoles`、public contract hash | 冒充官方 canonical type 或 role 重复。 |
| materialization | `onMaterialize` | 直接暴露未初始化 prototype/closure。 |

descriptor 通常是静态只读数据，内部字符串和数组必须在 registry/module 使用期间有效。注册函数
不深拷贝就意味着不能把 stack-local descriptor 或临时 `malloc` 数组注册后立即释放。

### Function/Method/Type descriptor

```text
ZrLibFunctionDescriptor
  name, min/max arg count, callback, return type, parameter descriptors,
  generic parameters, contract role, dispatch flags

ZrLibMethodDescriptor
  above + static/receiver facts, property name/ref access/ref export metadata

ZrLibTypeDescriptor
  prototype kind, fields, methods/meta methods, generic/protocol facts,
  construction policy, FFI lowering/view/owner/release metadata
```

`minArgumentCount`/`maxArgumentCount` 是 early arity guard，不替代参数 type/passing contract。
`returnTypeName` 和 parameter `typeName` 是 descriptor metadata，真正的 dispatch identity 还会
结合 module signature、member kind、canonical layout、generic specialization 和 relocation
facts。不要将 string type name 当作唯一 ABI key。

### Dispatch flags 是承诺，不是优化提示

native dispatch flags 包括 stack-root context、inline value context、result-always-written、
readonly receiver、blocking detached、no-safepoint critical 等。它们改变 dispatcher 对 stack
anchor、GC safepoint、result slot 和 receiver 的要求：

| flag 类 | provider 必须做到 | 不能做 |
| --- | --- | --- |
| stack root context | 遵循 dispatcher 建立的 root/anchor 边界 | 保存借用 stack pointer 到 callback 后。 |
| result always written | 每个成功回调路径写 `result` | 只 return true 而留下未初始化 result。 |
| readonly receiver | 不修改 receiver/借用规则 | 通过 raw object 旁路写字段。 |
| blocking detached | 按允许的 detached/worker policy 工作 | 在 GC critical stack 上长期阻塞。 |
| no safepoint critical | 不触发不允许的 allocation/safepoint | 在标志区域创建 managed object 或递归 VM call。 |

不确定时，不要标记更激进的 flag。错误 flag 可能只在 GC、AOT、stack relocation 或并发压测下
暴露为内存安全问题。

## 注册与 phase admission

```c
#include "zr_vm_library/native_registry.h"

if (!ZrLibrary_NativeRegistry_Attach(global)) {
    return ZR_FALSE;
}
if (!ZrLibrary_NativeRegistry_ValidateModuleDescriptor(global, &kDemoDescriptor)) {
    return ZR_FALSE;
}
if (!ZrLibrary_NativeRegistry_RegisterModule(global, &kDemoDescriptor)) {
    EZrLibNativeRegistryErrorCode code =
            ZrLibrary_NativeRegistry_GetLastErrorCode(global);
    const TZrChar *message = ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
    /* Log code/message; descriptor remains caller-owned. */
    return ZR_FALSE;
}
```

注册表会校验 ABI/runtime 版本、module name、capability、官方保留名称、provider role、canonical
type role、public contract hash 和重复 provider。失败 message 适合日志，但程序逻辑应以
`EZrLibNativeRegistryErrorCode` 为准。

provider phase 控制某 host 阶段是否可以消费某 module。可通过
`ZrLibrary_State_SetProviderPhase` 设置当前 state 的 host phase，并使用
`ZrLibrary_ProviderPhase_CanConsume(hostPhase, providerPhase)` 预检。例如 runtime host 不应
直接消费 test 或 compile-tool provider；compile-tool host 可以按允许规则消费 runtime/自身
provider。不要通过手工改 descriptor 字段来越权。

## 从 descriptor 到一次调用

```text
source/import or bound module reference
  -> resolver finds registered descriptor
  -> module materialization publishes exports/prototypes
  -> compiler asks for descriptor call-binding contract
  -> registry resolves target against provider identity/generation/layout
  -> native dispatcher builds ZrLibCallContext
  -> callback reads arguments, roots temporary values, writes result/writeback
  -> exception/status is normalized for VM/AOT caller
```

为生成 contract，使用：

```c
SZrCallBindingContract contract;
if (!ZrLibrary_NativeCallBinding_GetDescriptorContract(
        module, type, ZR_NATIVE_CALL_BINDING_FUNCTION,
        &module->functions[index], &contract)) {
    return ZR_FALSE;
}
```

然后由 `ZrLibrary_NativeRegistry_ResolveCallBinding` 解析 target，并读取
`SZrCallBindingDiagnostic`。不要自行重算 RID/hash，也不要将 descriptor callback 地址直接
写入 artifact：artifact 应只保存稳定 contract/relocation 投影，运行时必须重新验证 generation
和 provider identity。

`ZrLibrary_NativeRegistry_GetCallBindingIdentity` 可从 registry 管理的 native closure 反向得到
当前 contract，适用于 debugger/LSP/typed callable 检查；它不把 closure 变成 caller-owned。

## Callback 实现要求

每个 function/method callback 遵循：

```c
static TZrBool demo_add(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 left;
    TZrInt64 right;

    if (!ZrLib_CallContext_CheckArity(context, 2u, 2u) ||
        !ZrLib_CallContext_ReadInt(context, 0u, &left) ||
        !ZrLib_CallContext_ReadInt(context, 1u, &right)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, result, left + right);
    return ZR_TRUE;
}
```

规则：

1. 不要读取 `ZrLibCallContext` 私有 stack fields 来猜参数；用 accessors。
2. 对 `ref/out` 参数使用 `ZrLib_CallContext_WriteBackArgument`，只在 descriptor contract
   允许的 index/mode 上写回。
3. 需要分配对象或跨可能 GC 的调用时，用 `ZrLib_CallContext_BeginTempValueRoot` /
   `ZrLib_TempValueRoot_End` 保活临时值。
4. `InlineArgumentSpan/View` 是借用 metadata/frame 存储，safepoint 或 stack growth 后重新获取。
5. 类型/arity 错误通过 helper raise 或返回失败，让 dispatcher 的异常路径工作；不要吞掉
   exception 后返回随机 result。
6. descriptor 若声明 result-always-written，成功路径必须初始化 `result`。

## 查询、诊断与热重载

| API | 安全用途 | 注意事项 |
| --- | --- | --- |
| `FindModule` / `FindModuleByProviderRole` | 查已注册借用 descriptor | 找到不等于当前 phase 可消费。 |
| `GetModuleInfo` / `GetModuleInfoAt` | inventory/diagnostic | `ownerRefCount` 是状态快照，不是可手工改的 API。 |
| `GetModuleInfoBySourcePath` | plugin source 到 registry record | source path 仍须是已登记的稳定路径。 |
| `ComputeModuleSignatureHash` | contract/artifact identity | 不应替代完整 descriptor validation。 |
| `EnsureProjectDescriptorPlugin` | 按 project 配置加载 plugin | 先校验 project path/module name。 |
| `InvalidateDescriptorPluginSource` | owner count 为零后失效 plugin | in-use 时预期得到 `MODULE_IN_USE`。 |
| `GetLastErrorCode/Message` | 记录 registry 失败 | message 不是稳定 machine protocol。 |

reload 后，旧 native closure、call target、metadata token 和 artifact binding 都可能是 stale
generation。正确做法是按 module/provider identity 重新 resolve；禁止缓存并复用旧 callback
target 指针来“加速”热重载。

## 常见注册失败

| 错误码类别 | 含义 | 修复方向 |
| --- | --- | --- |
| `LOAD` / `SYMBOL` | descriptor plugin 无法加载或缺入口 | 校验平台库、导出 ABI entry 和路径。 |
| `ABI_MISMATCH` / `VERSION_MISMATCH` | plugin/runtime ABI 不兼容 | 使用匹配 SDK 重建 plugin。 |
| `CAPABILITY_MISMATCH` | host 缺 descriptor 要求能力 | 调整 host build/feature，而非伪造 capability。 |
| `MODULE_NAME_MISMATCH` | manifest/request 与 descriptor 不同 | 统一 module name/domain。 |
| `PHASE_MISMATCH` | 当前 host phase 不可消费 provider | 在正确 phase 加载或拆分 provider。 |
| `RESERVED_OFFICIAL_MODULE` | 非官方 provider 占用保留名 | 选择自有 namespace。 |
| `DUPLICATE_OFFICIAL_PROVIDER` / `DUPLICATE_PROVIDER_CONTRACT` | identity/role 冲突 | 移除冲突注册，保持一个权威 provider。 |
| `PROVIDER_CONTRACT_MISMATCH` | public contract hash/role 与预期不同 | 同步 descriptor、artifact 和 lock。 |
| `INVALID_CANONICAL_TYPE_ROLE` | canonical role graph 不合法 | 修正 role/parent/projection，勿绕过 validator。 |

对于 C ABI 对外导出、动态 plugin 入口和逐个字段模板，可继续阅读[原生模块编写](../05-interop/native-module-authoring.md)
和[原生插件加载](../05-interop/native-plugin-loading.md)。
