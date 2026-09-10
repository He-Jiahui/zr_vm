---
related_code:
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/gc
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/gc
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/garbage-collection.md
  - docs/library-and-builtins/native-call-binding.md
tests:
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/ffi/test_ffi_module.c
  - tests/module/test_module_system.c
  - tests/core/test_gc.c
  - tests/meta/test_meta.c
  - tests/library/test_call_binding_native_registry.c
doc_type: api-reference
---

# C 值、GC 与异常生命周期

C 宿主最容易出错的地方不是函数名，而是 borrowed、owned、root、pin 和 exception 的
边界。本页把一个 SZrTypeValue 从创建、复制、传给 native callback、跨 safepoint 到释放
的完整规则写成可执行的调用顺序。所有指针默认只在当前 state/revision 内借用，除非本页
明确标注为 root、pin 或调用者拥有。

## 1. SZrTypeValue 布局

公共 value.h 中的核心结构包含：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| type | EZrValueType | null、bool、signed/unsigned integer、float/double、string、array、object、function、thread、native 等。 |
| value | TZrPureValue | raw object、native scalar 或 native function union。 |
| isGarbageCollectable | TZrBool | value 是否指向 GC 管理的对象。 |
| isNative | TZrBool | value 是否由 native representation 携带。 |
| ownershipKind | EZrOwnershipValueKind | NONE、UNIQUE、SHARED、WEAK、BORROWED、LOANED。 |
| ownershipControl | SZrOwnershipControl | 强/弱 owner 的控制块，不能由宿主直接修改。 |
| ownershipWeakRef | SZrOwnershipWeakRef | weak handle 辅助信息。 |

类型枚举和 ownership 字段必须一起理解。一个 OBJECT value 可能是普通 borrowed
引用，也可能带 unique/shared control；仅检查 value->type 不能决定是否需要 release。

## 2. 创建基础值

core 提供初始化函数，library callback 还提供等价的 Set helper：

~~~c
SZrTypeValue value;
ZrCore_Value_ResetAsNull(&value);
ZrCore_Value_InitAsInt(state, &value, -7);

SZrTypeValue flag;
ZrCore_Value_InitAsBool(state, &flag, ZR_TRUE);

SZrTypeValue ratio;
ZrCore_Value_InitAsFloat(state, &ratio, 3.5);

SZrTypeValue pointer;
ZrCore_Value_InitAsNativePointer(state, &pointer, nativePointer);
~~~

字符串和对象必须由当前 state 创建或解析，然后通过 ZrCore_Value_InitAsRawObject 或
ZrLib_Value_SetObject 写入。不要把普通 C 字符串地址塞进 value union；要使用
ZrLib_Value_SetString 或 ZrCore_String_CreateFromNative。

library helper 的约定：

| helper | 结果 |
| --- | --- |
| ZrLib_Value_SetNull | 清空 type、ownership metadata。 |
| ZrLib_Value_SetBool/SetInt/SetFloat | 写入 scalar 并初始化 native flags。 |
| ZrLib_Value_SetString | 在 state 中创建/查找 string object，再写入 value。 |
| ZrLib_Value_SetStringObject | 使用已有 string object；调用者保证其存活。 |
| ZrLib_Value_SetObject | 写入指定 EZrValueType 的 GC object。 |
| ZrLib_Value_SetNativePointer | 包装非 GC native pointer；释放责任由 provider contract 定义。 |

## 3. Copy、overwrite 和 ownership

ZrCore_Value_Copy 不是 memcpy 的别名。它首先处理 destination 原有 ownership，再根据
source/destination 是否为 plain value、GC object 或 managed owner 选择 fast copy 或
ZrCore_Value_CopySlow。目标槽位覆盖规则如下：

1. destination 与 source 相同则无需动作；
2. destination 有 unique/shared/loaned/weak control 时先 release/reset；
3. source 没有 ownership 且不是需要深处理的 GC object 时可以复制结构；
4. managed source 走 slow path，增加控制块引用或建立正确的 borrowed 状态；
5. materialized stack value 的 unique/shared/loaned/weak ownership 可以转移到最终槽位，
   并把 source 重置为 null。

~~~c
SZrTypeValue destination;
ZrCore_Value_ResetAsNull(&destination);
ZrCore_Value_Copy(state, &destination, sourceValue);

/* Reassigning a managed value releases the previous destination owner. */
ZrCore_Value_Copy(state, &destination, replacementValue);

/* Explicitly release when the value leaves its owner scope. */
ZrCore_Ownership_ReleaseValue(state, &destination);
ZrCore_Value_ResetAsNull(&destination);
~~~

不要对带 ownershipControl 的 value 使用结构赋值或 free。也不要在 callback 返回后继续
使用 argument pointer；需要写回时调用 ZrLib_CallContext_WriteBackArgument。

## 4. Native callback context

ZrLibCallContext 同时描述 descriptor、receiver、参数和 stack anchor。常用的安全入口：

| API | 用途 |
| --- | --- |
| ZrLib_CallContext_ArgumentCount | 读取实际参数数。 |
| ZrLib_CallContext_Self | 取得 receiver；静态函数可能为空。 |
| ZrLib_CallContext_Argument | 取得第 index 个参数的 borrowed value。 |
| ZrLib_CallContext_CheckArity | 按 descriptor 的 min/max 做一致检查。 |
| ZrLib_CallContext_ReadInt/ReadFloat/ReadBool | 验证类型后读取 scalar。 |
| ZrLib_CallContext_ReadString/ReadObject/ReadArray/ReadFunction | 验证 GC/native kind 并返回 borrowed pointer。 |
| ZrLib_CallContext_RaiseTypeError/RaiseArityError | 设置结构化 VM exception；函数不返回。 |
| ZrLib_CallContext_WriteBackArgument | 按 passing mode 写回 ref/out 参数。 |

参数数组可能来自普通 frame，也可能来自 inline value frame。descriptor 的
INLINE_VALUE_CONTEXT/READONLY_INLINE_VALUE_CONTEXT 决定 callback 是否可以取得
ZrLibInlineArgumentView。inline span 和 metadata pointer 都是 borrowed；stack growth 或
safepoint 后必须重新调用 InlineArgumentView。

## 5. Temp root 和 GC safepoint

任何可能触发分配、nested ZR call、异常或 safepoint 的 native callback，都应把临时
SZrTypeValue 放入 ZrLibTempValueRoot：

~~~c
ZrLibTempValueRoot root;
if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
    return ZR_FALSE;
}

SZrTypeValue *temporary = ZrLib_TempValueRoot_Value(&root);
ZrLib_Value_SetString(context->state, temporary, "temporary result");

/* Nested calls and allocation are safe while root is active. */
ZrCore_Value_Copy(context->state, result, temporary);
ZrLib_TempValueRoot_End(&root);
return ZR_TRUE;
~~~

所有成功和失败路径都必须调用 End。多个 root 按嵌套顺序结束；不要把 root 结构复制到
另一个线程。root 保护的是 value 的 GC 可达性，不保证 native pointer payload 的线程安全。

core 还提供显式 native call pin：

~~~c
SZrGcNativeCallPin pin;
if (!ZrCore_Gc_NativeCallPinValue(state, value, &pin)) {
    return ZR_FALSE;
}
/* Use the object only for the bounded native call. */
ZrCore_Gc_NativeCallUnpin(global, &pin);
~~~

Pin 的生命周期必须严格包住 native call；不能把 pin 作为长期缓存。多个 pin 可以嵌套，
但必须逐一 unpin。对 raw object 使用 NativeCallPinObject；若只需要 C 侧遍历而不需要
地址稳定，优先使用 temp root。

## 6. Write barrier 和 safepoint

把 GC value 写入已存在的 object field 时，必须让 core 看到 owner/value 关系：

~~~c
ZrCore_Object_SetValue(state, object, key, value);
/* For low-level direct storage, call the matching barrier. */
ZrCore_Gc_WriteBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), value);
ZrCore_Gc_SafePoint(state);
~~~

高层 Object_SetValue、Object_SetMember、Array_PushValue 已包含相应的写屏障；只有使用
unchecked/no-write-barrier 变体时，调用者才承担证明责任。safepoint 可能移动对象、运行
finalizer 或处理 pending exception；之后应重新取得所有 borrowed object/string pointer。

## 7. 对象、数组和模块 helper

library 层 helper 适合 native module callback：

~~~c
SZrObject *object = ZrLib_Object_New(state);
SZrObject *array = ZrLib_Array_New(state);
if (object == ZR_NULL || array == ZR_NULL) {
    return ZR_FALSE;
}

ZrLib_Object_SetFieldCString(state, object, "name", value);
if (!ZrLib_Array_PushValue(state, array, value)) {
    return ZR_FALSE;
}

const SZrTypeValue *first = ZrLib_Array_Get(state, array, 0u);
TZrSize length = ZrLib_Array_Length(array);
~~~

object/array 返回值在下一次可能分配的调用后可能失效，除非已经 root/pin。模块和导出
查询同样返回 borrowed：

~~~c
SZrObjectModule *module = ZrLib_Module_GetLoaded(state, "zr.system");
const SZrTypeValue *exportValue =
        ZrLib_Module_GetExport(state, "zr.system.console", "printLine");
if (module != ZR_NULL && exportValue != ZR_NULL) {
    SZrTypeValue args[1];
    ZrLib_Value_SetString(state, &args[0], "hello");
    SZrTypeValue callResult;
    ZrLib_CallValue(state, exportValue, ZR_NULL, args, 1u, &callResult);
}
~~~

如果需要按名字创建类型实例，先 ZrLib_Type_FindPrototype，再
ZrLib_Type_NewInstanceWithPrototype；不要把用户字符串直接转换为 prototype pointer。

## 8. 异常边界

core exception API 使用当前 state 的 thread status 和 payload：

~~~c
EZrThreadStatus status = ZrCore_Exception_TryRun(state, tryFunction, arguments);
if (ZrCore_Exception_IsStausError(status)) {
    /* Normalize status/payload before returning to the host. */
    ZrCore_Exception_NormalizeStatus(state, status);
}

ZrCore_Exception_RaiseNamedRuntimeError(
        state, "zr.system.exception.TypeError",
        "native argument has wrong type", callInfo);
~~~

callback 若已经通过 RaiseTypeError/RaiseArityError 设置 exception，应立即返回失败路径，
不能再写一个“成功” result。宿主在边界处区分三类结果：

| 结果 | 含义 |
| --- | --- |
| callback 返回 true，result 合法 | 普通成功。 |
| callback 返回 false，state 有 exception | 可报告给 ZR caller 的失败。 |
| callback 返回 false，state 无 exception | provider bug 或未完成 contract；宿主应记录诊断，不应读取 result。 |

未处理异常可用 ZrCore_Exception_PrintUnhandled 或 LogUnhandled；这些函数读取的是当前
state snapshot，不能在 State_Exit 后调用。

## 9. 调用、root 和释放时序

一个可复用的 native operation 可以按此时序实现：

~~~text
validate context and arity
  -> read borrowed arguments
  -> begin temp roots / native pins
  -> allocate or nested-call
  -> write result or write-back ref/out
  -> end roots and unpin
  -> return true
on failure:
  -> raise/normalize exception
  -> end roots and unpin
  -> return false
~~~

关闭 state 时，顺序应是停止 callback/debug/task/network activity，释放 provider-owned
objects and handles，结束仍在使用的 roots/pins，释放 module registry，最后
ZrCore_State_Exit/Free 与 GlobalState。任何跨 state 的 raw object、SZrString、prototype、
module export 或 error message 都属于悬空指针。

## 10. 违规模式速查

| 违规写法 | 为什么危险 | 替代方案 |
| --- | --- | --- |
| 保存 ZrLib_CallContext 或 Argument 指针 | frame/stack 可增长或回收 | 复制 value，或在调用内 root/pin。 |
| memcpy SZrTypeValue | 遗漏 ownership 引用/释放 | ZrCore_Value_Copy 或 library Set helper。 |
| 直接写 object field | GC 看不到新边 | Object_SetValue/SetMember 或显式 WriteBarrier。 |
| pin 后忘记 unpin | 阻止回收、泄漏 global 状态 | 单一 cleanup 路径成对释放。 |
| 把 borrowed string 当 C 永久字符串 | 下一次分配可能移动/释放 | 复制到宿主 buffer，并记录长度。 |
| 错误后继续调用普通 API | pending exception 会污染后续 frame | 立即 unwind/normalize，按失败路径返回。 |
| 用 type name 代替 generation/token | reload 后身份可能复用 | 使用 reflection TypeId 和 generation 检查。 |

这些规则与[Native descriptor API](../03-modules/native-api.md)、[C 宿主集成指南](c-host-guide.md)
和[FFI Contract](ffi-contract.md)共同构成 C 调用方的最小安全边界。
