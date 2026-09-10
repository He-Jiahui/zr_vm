---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/2026-07-19-10-native-ffi-module-package-design.md
  - docs/library-and-builtins/index.md
tests:
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_official_provider_convergence.c
  - tests/ffi/test_native_extern_contract.c
  - tests/module/test_module_system.c
doc_type: api-reference
---

# Native descriptor 与调用 API

本页描述编写 ZrVm native library 时真正使用的 C contract。descriptor 是编译器、runtime、
反射、AOT 和 LSP 共享的 schema；不要只注册一个函数指针就把模块视为完成。每个公开名称、
参数 mode、泛型约束、layout、provider phase 和 contract role 都会参与 admission 与 call
binding hash。

## 模块 descriptor 层级

~~~text
ZrLibModuleDescriptor
  ├─ constants[]      (ZrLibConstantDescriptor)
  ├─ functions[]      (ZrLibFunctionDescriptor)
  ├─ types[]          (ZrLibTypeDescriptor)
  │    ├─ fields[]     (ZrLibFieldDescriptor)
  │    ├─ methods[]    (ZrLibMethodDescriptor)
  │    └─ metaMethods[](ZrLibMetaMethodDescriptor)
  ├─ moduleLinks[]    (ZrLibModuleLinkDescriptor)
  ├─ typeHints[]/JSON
  ├─ attributeRoles[]
  └─ canonicalTypeRoles[]
~~~

### Module 字段

| 字段 | 作用 | 约束 |
| --- | --- | --- |
| `abiVersion` | descriptor ABI 版本 | 必须与 runtime 支持版本一致。 |
| `moduleName` | 完整 identity，如 `zr.math` | 非空、与加载请求一致；官方保留名不能伪造。 |
| `constants/functions/types` | 公开 surface 和计数 | count>0 时指针必须非空；名称不能重复。 |
| `moduleVersion` | provider 版本 | 参与 package/contract admission。 |
| `minRuntimeAbi` | 最低 runtime ABI | 高于 host 时拒绝。 |
| `requiredCapabilities` | TYPE_HINTS、FFI 等 capability 位 | host 缺位时拒绝。 |
| `providerPhase` | Runtime/CompileTool/Test | phase 不能被低权限 host 消费。 |
| `publicContractHash` | 稳定 contract 标识 | 与结构化 signature hash 一起校验。 |
| `providerContractRole` | canonical provider owner | 同一 role 只能有一个官方 owner。 |
| `onMaterialize` | lazy materialize callback | 只在 registry 完成校验后调用。 |
| `isContractOnly` | 只提供 role/projection metadata | contract-only provider 不能被普通 loader materialize。 |

### Function/Method descriptor

`ZrLibFunctionDescriptor` 和 `ZrLibMethodDescriptor` 包含 name、min/max arity、callback、
return type string、documentation、parameter array、generic parameter array、contract role
和 dispatch flags。Method 另有 `isStatic`、propertyName、reference access 和 writable-ref
标志。参数结构如下：

~~~c
typedef struct ZrLibParameterDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
    EZrLibParameterPassingMode passingMode; /* VALUE, IN, OUT, REF */
} ZrLibParameterDescriptor;
~~~

`minArgumentCount`/`maxArgumentCount` 是真实调用 arity；默认参数通过 min<max 表达，不能
只在 documentation 中说明。return type/name 必须能由 resolver 解析；`T?` 只可出现在 hint
文档的可空记法，不是 source TypeRef 后缀。

### Type descriptor

`ZrLibTypeDescriptor` 还描述 prototype type、fields/methods/meta methods、extends、implements、
enum members、constructor signature、generic parameters、protocol mask 和 construction flags。
FFI type 可额外设置 lowering kind、view type、underlying type、owner mode 和 release hook。
interface/resource/ref-like 类型应关闭不适合的 `allowValueConstruction`/
`allowBoxedConstruction`，否则 reflection/FFI 会错误地尝试构造。

## callback context

所有绑定 callback 的统一签名是：

~~~c
typedef TZrBool (*FZrLibBoundCallback)(ZrLibCallContext *context,
                                        SZrTypeValue *result);
~~~

`ZrLibCallContext` 的关键字段：

| 字段/辅助函数 | 生命周期和用途 |
| --- | --- |
| `state` | 当前 VM state；只在 callback 期间有效。 |
| module/type/function/method/meta descriptor | 当前 binding metadata；只读借用。 |
| `ownerPrototype`、`constructTargetPrototype` | method/constructor dispatch 目标。 |
| `argumentValues`、`argumentValuePointers`、`argumentCount` | 已按 descriptor 归一化的参数视图。 |
| `selfValue` | receiver；static function 可能为 null。 |
| stack pointers/anchors | bridge 用于 GC/safepoint 后恢复 frame；callback 不得改写。 |
| `inlineFrameFunction`/`inlineFrameBase` | inline value dispatch 的 borrowed storage。 |

context、argument pointer、inline span 和 result slot 都是 borrowed。任何会触发分配、GC、
另一个 ZR call 或线程切换的操作后，必须重新取得 view；不能保存旧地址。

## 参数读取和错误

~~~c
static TZrBool increment(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 value;
    if (!ZrLib_CallContext_CheckArity(context, 1u, 1u)) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, 0u, &value)) {
        ZrLib_CallContext_RaiseTypeError(context, 0u, "int");
    }
    ZrLib_Value_SetInt(context->state, result, value + 1);
    return ZR_TRUE;
}
~~~

常用 helper：

~~~c
ZrLib_CallContext_ArgumentCount(context);
ZrLib_CallContext_Self(context);
ZrLib_CallContext_Argument(context, index);
ZrLib_CallContext_WriteBackArgument(context, index, &value);
ZrLib_CallContext_ReadInt/ReadFloat/ReadBool(context, index, &out);
ZrLib_CallContext_ReadString/ReadObject/ReadArray/ReadFunction(context, index, &out);
ZrLib_CallContext_RaiseTypeError(context, index, "ExpectedType");
ZrLib_CallContext_RaiseArityError(context, min, max);
~~~

`RaiseTypeError` 和 `RaiseArityError` 声明为 `ZR_NO_RETURN`。callback 在 raise 后不要继续
写 result 或释放已由 bridge 管理的 frame。

## result、对象和数组构造

~~~c
SZrObject *object = ZrLib_Object_New(context->state);
SZrObject *array = ZrLib_Array_New(context->state);
SZrTypeValue field;
ZrLib_Value_SetString(context->state, &field, "native");
ZrLib_Object_SetFieldCString(context->state, object, "kind", &field);
ZrLib_Array_PushValue(context->state, array, &field);
ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
~~~

可用 setter 包括 `SetNull`、`SetBool`、`SetInt`、`SetFloat`、`SetString`、
`SetStringObject`、`SetObject` 和 `SetNativePointer`。返回 managed object 前若 callback 还要执行可能触发 GC
的操作，先把 object/result 放入 temp root。

## Temp value root

~~~c
ZrLibTempValueRoot root;
if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
    return ZR_FALSE;
}
SZrTypeValue *slot = ZrLib_TempValueRoot_Value(&root);
ZrLib_Value_SetString(context->state, slot, "temporary");
/* allocate/call other VM APIs here; slot remains traced */
ZrCore_Value_Copy(context->state, result, slot);
ZrLib_TempValueRoot_End(&root);
return ZR_TRUE;
~~~

也可使用 `ZrLib_TempValueRoot_Begin(state, &root)`。`SetValue`、`SetObject`、`SetNull` 用于
更新 root slot；End 必须在所有正常/错误路径调用。root 不能跨 callback 或线程保存。

## dispatch flags

| flag | 作用 |
| --- | --- |
| `STACK_ROOT_CONTEXT` | bridge 使用 stack anchor 保持参数/结果可追踪。 |
| `NO_SELF_REBIND` | 禁止把 receiver 重新绑定到 owner prototype。 |
| `INLINE_VALUE_CONTEXT` | 参数来自 inline value frame。 |
| `RESULT_ALWAYS_WRITTEN` | callback 必须在成功路径写 result。 |
| `READONLY_INLINE_VALUE_CONTEXT` | inline storage 只读。 |
| `RESULT_OPTIONAL` | 允许 callback 返回 null/optional result。 |
| `READONLY_RECEIVER` | method 不得修改 receiver。 |
| `BLOCKING_DETACHED` | callback 可能阻塞，bridge 使用 detached safepoint 策略。 |
| `NO_SAFEPOINT_CRITICAL` | 临界区禁止 safepoint；只允许极短、无分配路径。 |

`BLOCKING_DETACHED` 与 `NO_SAFEPOINT_CRITICAL` 不能随意同时设置；provider validator 会检查
不相容组合。inline span/view 是 borrowed metadata，stack growth 或 safepoint 后必须重新获取。

## 调用其它 callable/export

~~~c
const SZrTypeValue *exportValue =
    ZrLib_Module_GetExport(state, "zr.math", "sqrt");
SZrTypeValue args[1];
SZrTypeValue result;
ZrLib_Value_SetFloat(state, &args[0], 9.0);
if (!ZrLib_CallValue(state, exportValue, ZR_NULL, args, 1u, &result)) {
    return ZR_FALSE;
}
~~~

`ZrLib_CallModuleExport` 是按 module/export name 的快捷入口。callable、arguments 和 result
都是 state-owned value；调用期间可能 GC，调用方应在调用前 root 仍需存活的临时对象。读取
native return type 可用 `ZrLib_CallableValue_GetNativeBindingReturnTypeName`，但它是诊断辅助，
不能取代完整 type checking。

## Registry admission

~~~c
ZrLibrary_NativeRegistry_Attach(global);
if (!ZrLibrary_NativeRegistry_ValidateModuleDescriptor(global, descriptor)) {
    return ZR_FALSE;
}
if (!ZrLibrary_NativeRegistry_RegisterModule(global, descriptor)) {
    EZrLibNativeRegistryErrorCode code =
        ZrLibrary_NativeRegistry_GetLastErrorCode(global);
    const TZrChar *message = ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
    (void)code; (void)message;
    return ZR_FALSE;
}
~~~

registry 检查 ABI、runtime ABI、capability、module name、phase、官方保留名、provider role、
canonical type role、public contract 和重复 provider。常见 error code：LOAD、SYMBOL、
ABI_MISMATCH、VERSION_MISMATCH、CAPABILITY_MISMATCH、MODULE_NAME_MISMATCH、MODULE_IN_USE、
PHASE_MISMATCH、RESERVED_OFFICIAL_MODULE、DUPLICATE_OFFICIAL_PROVIDER、
PROVIDER_CONTRACT_MISMATCH、INVALID_CANONICAL_TYPE_ROLE、DUPLICATE_PROVIDER_CONTRACT。

## provider phase 和 source lifetime

`ZrLibrary_State_SetProviderPhase(state, phase)` 切换当前消费上下文；
`ZrLibrary_ProviderPhase_CanConsume(host, provider)` 在建立 binding 前检查权限。descriptor
plugin 的 source path 失效应调用 `ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource`，
否则 registry 可能继续引用已卸载的动态库。`GetModuleInfo` 返回的 descriptor/sourcePath
是 borrowed，需在 registry/global 存活期间使用。

## 最小 module 骨架

~~~c
static const ZrLibParameterDescriptor kParams[] = {
    {"value", "int", "Input integer.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE}
};
static const ZrLibFunctionDescriptor kFunctions[] = {
    {"increment", 1u, 1u, increment, "int", "Increment.",
     kParams, 1u, ZR_NULL, 0u, 0u, ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN}
};
~~~

实际 module 初始化器字段顺序以当前头文件为准；推荐使用仓库中的 descriptor macros，避免
新增字段时 positional initializer 错位。共享库导出 `ZrVm_GetNativeModule_v1()`，静态库
导出模块专用 `*_GetModuleDescriptor` 和 `*_Register`。

## contract hash 与升级

`ZrLibrary_NativeRegistry_ComputeModuleSignatureHash` 会把 module name/version/hash、ABI、
capability/phase/role 以及 functions/constants/links/types/roles 的顺序和字段纳入 hash。
改变公开名称、参数 mode、返回类型、generic constraint、dispatch flag、layout 或 protocol
relation 都是 breaking contract：更新 moduleVersion/publicContractHash、AOT registration、
artifact metadata、LSP cache 和本页接口表。隐藏 finalizer payload 不属于可移植 API，但仍要
在 global 关闭前释放。
