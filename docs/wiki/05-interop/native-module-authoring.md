---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/conf.h
  - zr_vm_library/src/zr_vm_library/native_binding
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - CMakeLists.txt
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_official_provider_convergence.c
  - tests/ffi/test_native_extern_contract.c
  - tests/CMakeLists.txt
doc_type: integration-guide
---

# 编写 Native Module

本页面向希望把 C 库暴露给 ZR 的开发者。目标不是“把 C 函数包一层”，而是发布一个
可被 resolver、AOT、reflection、LSP 和 runtime 同时理解的 descriptor。推荐先完成静态
descriptor 和 callback 单元测试，再接入动态插件加载。callback 内每个 API 的借用期、临时
root 和重入规则由[Native Call Context 与回调 C API](native-call-context-reference.md)统一说明；
本页重点保留模块级设计和注册步骤。

## 目录和构建

典型模块至少包含：

~~~text
my_module/
  include/my_module/module.h
  src/module.c
  src/runtime.c
  CMakeLists.txt
~~~

公共头文件包含 zr_vm_library/native_binding.h 和自己的导出宏；CMake 目标必须链接
zr_vm_core、zr_vm_library 以及实际使用的 provider 依赖。静态构建导出
MyModule_GetModuleDescriptor/MyModule_Register；共享构建额外导出
ZrVm_GetNativeModule_v1。不要在插件中复制 ZrLibModuleDescriptor 定义或重定义 TZr* 基础类型。

## 第一步：设计 source contract

先写一张 source API 表，明确每个参数的：

- 类型名称和 generic arguments；
- VALUE、IN、OUT、REF passing mode；
- 默认值对应的 min/max arity；
- 返回类型和 RESULT_OPTIONAL/READONLY_RECEIVER 等 flags；
- ownership、layout、protocol、FFI lowering；
- provider phase、capability 和 public contract hash。

ZR 源码中的 native extern 必须使用同一形状：

~~~zr
native extern("my_module") {
    fn checksum(data: ref u8, length: int): u64;
}
~~~

ref/out 的 place 和逃逸规则由 compiler 检查；C callback 不能把 argument pointer 保存
到调用结束以后。

## 第二步：写参数和 callback

~~~c
static const ZrLibParameterDescriptor kChecksumParameters[] = {
    {"data", "ref u8", "Input byte view.",
     ZR_LIB_PARAMETER_PASSING_MODE_REF},
    {"length", "int", "Byte count.",
     ZR_LIB_PARAMETER_PASSING_MODE_VALUE}
};

static TZrBool my_checksum(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *data;
    TZrInt64 length;
    if (!ZrLib_CallContext_CheckArity(context, 2u, 2u)) {
        ZrLib_CallContext_RaiseArityError(context, 2u, 2u);
    }
    data = ZrLib_CallContext_Argument(context, 0u);
    if (data == ZR_NULL) {
        ZrLib_CallContext_RaiseTypeError(context, 0u, "ref u8");
    }
    if (!ZrLib_CallContext_ReadInt(context, 1u, &length) || length < 0) {
        ZrLib_CallContext_RaiseTypeError(context, 1u, "int");
    }
    /* Validate the view and read only length bytes. */
    ZrLib_Value_SetInt(context->state, result, 0);
    return ZR_TRUE;
}
~~~

读取 helper 失败后必须 raise typed error；不要读取 union 中未验证的字段。需要写回
out/ref 时使用 ZrLib_CallContext_WriteBackArgument，而不是直接改 argumentValues
数组。成功 callback 必须遵守 descriptor 的 result flag；设置了 RESULT_ALWAYS_WRITTEN
时所有成功路径都要写 result。

## 第三步：处理 GC 和 reentrancy

callback 中可能触发分配、另一个 ZR call、GC safepoint 或异常。规则如下：

1. 把尚未发布的 SZrTypeValue 放入 ZrLibTempValueRoot；
2. 保存的 object/string 只作为 borrowed view 使用，safepoint 后重新获取；
3. 不保存 ZrLibCallContext、argument pointer、inline span 或 result 地址；
4. 不在 callback 中 longjmp、退出当前 mutator 或切换线程；
5. 阻塞操作设置 BLOCKING_DETACHED，临界无分配路径才使用 NO_SAFEPOINT_CRITICAL；
6. 返回前结束所有 temp root；错误路径也要结束。

~~~c
ZrLibTempValueRoot root;
if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
    return ZR_FALSE;
}
SZrTypeValue *temporary = ZrLib_TempValueRoot_Value(&root);
ZrLib_Value_SetString(context->state, temporary, "result");
/* Any allocation or nested call is safe while root is active. */
ZrCore_Value_Copy(context->state, result, temporary);
ZrLib_TempValueRoot_End(&root);
return ZR_TRUE;
~~~

## 第四步：发布 descriptor

函数 descriptor 当前没有专用的 `ZR_LIB_FUNCTION_DESCRIPTOR_INIT` 宏；可以使用 C99
designated initializer，或按 `native_binding.h` 中的字段顺序初始化。下面的例子明确列出
全部字段，便于在 ABI 增加字段时由编译器报告遗漏：

~~~c
static const ZrLibFunctionDescriptor kFunctions[] = {
    {
        .name = "checksum",
        .minArgumentCount = 2u,
        .maxArgumentCount = 2u,
        .callback = my_checksum,
        .returnTypeName = "u64",
        .documentation = "Checksum a byte view.",
        .parameters = kChecksumParameters,
        .parameterCount = 2u,
        .genericParameters = ZR_NULL,
        .genericParameterCount = 0u,
        .contractRole = 0u,
        .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN
    }
};
~~~

无论是否使用宏，都要填写 function/type/constant count 和指针的一致性。参数、generic
constraint、property reference、module link、attribute role 和 canonical role 也会进入 hash，
不能只填核心函数。module descriptor 必须声明 moduleVersion、minRuntimeAbi、requiredCapabilities、
providerPhase、publicContractHash 和 providerContractRole。

## 第五步：注册和 admission

~~~c
const ZrLibModuleDescriptor *MyModule_GetModuleDescriptor(void) {
    return &kModule;
}

TZrBool MyModule_Register(SZrGlobalState *global) {
    return ZrLibrary_NativeRegistry_RegisterModule(global, &kModule);
}
~~~

宿主通常先调用 ZrLibrary_NativeRegistry_Attach(global)，再注册依赖。注册失败后立即读取
GetLastErrorCode/GetLastErrorMessage。错误包括 ABI/version/capability/phase/module-name
mismatch、reserved official、duplicate provider/contract 和 invalid canonical role。

共享插件入口必须返回非空 descriptor：

~~~c
ZR_LIBRARY_PLUGIN_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &kModule;
}
~~~

加载器在调用入口后仍会验证 moduleName、abiVersion、minRuntimeAbi、requiredCapabilities、
providerPhase、publicContractHash 和结构化 signature hash；插件不能通过改字符串绕过检查。

## 第六步：测试和契约升级

至少覆盖：

| 测试 | 需要证明 |
| --- | --- |
| callback arity/type | 过少、过多、错误类型会产生结构化 TypeError。 |
| GC/reentrancy | nested call 或强制 GC 后结果和 root 仍有效。 |
| ownership | ref/out 不越界、不逃逸，close/finalizer 幂等。 |
| registry | 重复官方 provider、phase/capability/ABI 失配被拒绝。 |
| call binding | module/type/function/layout generation 变化会失效并重新 resolve。 |
| shared loading | 缺 symbol、路径错误和 source invalidate 返回明确错误。 |

公开 API 改动包括名称、参数 mode、返回类型、generic constraint、dispatch flag、layout、
protocol 或 module link；必须递增 moduleVersion，更新 publicContractHash、AOT/artifact/
LSP metadata 和文档。只改隐藏 payload 也要保持 finalizer 和 global close 顺序兼容。

## 资源关闭顺序

~~~text
stop callbacks/debug hooks
  -> close script/native handles
  -> invalidate descriptor plugin source
  -> release temporary roots and prepared jobs
  -> unregister/free native registry
  -> State_Exit/State_Free
  -> GlobalState_Free
~~~

不要在 global 已释放后调用 descriptor callback、GetModuleInfo 或 error message。更多细节见
[Native descriptor API](../03-modules/native-api.md)、[C 宿主指南](c-host-guide.md) 和
[通用 C API 约定](c-api.md)。
