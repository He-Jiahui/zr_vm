---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_binding_call_binding.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，补充 C native 库调用方案和接口用例
  - docs/library-and-builtins/index.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_native_binding_direct_call.c
  - tests/library/test_call_binding_native_registry.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/parser/test_typed_call_binding.c
doc_type: interop-recipes
---

# Native Callback C 实战配方

本页专门回答“一个 C native 函数如何读取 ZR 参数、分配 managed 值、处理异常并安全返回”。
descriptor 字段和注册流程见 [Native Module 编写](native-module-authoring.md)，插件发现见
[Native 插件加载](native-plugin-loading.md)，这里聚焦 callback 内部的调用约定。若需要按
`native_binding.h` 逐项查询参数、temp root、inline storage 和 nested call 的生命周期，请转到
[Native Call Context 与回调 C API](native-call-context-reference.md)。

## 1. callback 的唯一入口

native function/method/meta-method 的慢路径统一使用：

```c
typedef TZrBool (*FZrLibBoundCallback)(
    ZrLibCallContext *context,
    SZrTypeValue *result);
```

`context` 和 `result` 都只在当前 callback 调用期间有效。成功返回 `ZR_TRUE` 时，result
必须是完整初始化的 `SZrTypeValue`；失败返回 `ZR_FALSE` 时，runtime 会读取当前 exception/
diagnostic 状态，callback 不得留下半初始化 managed pointer。若 descriptor 设置
`ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN`，即使逻辑结果为 null，也必须显式
调用 `ZrLib_Value_SetNull(result)`。

最小 callback：

```c
static TZrBool answer_callback(
    ZrLibCallContext *context,
    SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 0u, 0u)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, result, 42);
    return ZR_TRUE;
}
```

descriptor 中的 `minArgumentCount`/`maxArgumentCount` 是 resolver 的静态准入；callback
仍应再次检查 arity，因为同一 callback 可能通过 dynamic/native call path 进入。检查失败
后不要继续读取 argument slot。

## 2. 参数读取和 receiver

### 2.1 positional arguments

```c
static TZrBool add_callback(
    ZrLibCallContext *context,
    SZrTypeValue *result) {
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

读 helper 会验证 runtime value type 并写出 C 标量：

| helper | 输出 | 注意 |
| --- | --- | --- |
| `ReadInt` | `TZrInt64` | 不等于所有 ZR integer layout；按 descriptor 转换 |
| `ReadFloat` | `TZrFloat64` | NaN/Infinity 由 provider 另行处理 |
| `ReadBool` | `TZrBool` | 不把整数 0/1 自动当 bool |
| `ReadString` | 借用 `SZrString *` | 只在 callback/current state 有效 |
| `ReadObject` | 借用 `SZrObject *` | 不取得额外 owner 引用 |
| `ReadArray` | 借用数组 object | 仍需检查元素 type/length |
| `ReadFunction` | 借用 callable value pointer | 调用前必须 root 相关参数 |

失败的 read 不会把未定义数据写进 out 参数。`ReadString` 返回的 string storage 不能直接
交给异步线程、C 库长期保存或在 callback 结束后使用；需要持久化时创建 managed copy 或
复制 UTF-8 bytes。

### 2.2 receiver、owner 和 constructor target

method callback 的 receiver 可通过 `ZrLib_CallContext_Self(context)` 取得；owner prototype
和构造目标分别由：

```c
SZrTypeValue *self = ZrLib_CallContext_Self(context);
SZrObjectPrototype *owner = ZrLib_CallContext_OwnerPrototype(context);
SZrObjectPrototype *target =
    ZrLib_CallContext_GetConstructTargetPrototype(context);
```

`self` 是当前 call window 中的 value，不代表可跨调用保存的 strong reference。constructor
需要区分继承链上的实际 target 时，应使用 `GetConstructTargetPrototype`，不要从
`owner->name` 字符串猜测。`NO_SELF_REBIND` dispatch flag 会禁止 runtime 把 receiver
重新绑定到另一个 prototype；descriptor 必须和 callback 的预期一致。

## 3. `in`、`out` 和 `ref`

passing mode 同时存在于源语法、descriptor parameter 和 call-binding contract：

| mode | callback 看到的内容 | 是否写回 |
| --- | --- | --- |
| value | 独立 value view | 不写回原 place |
| `in` | 只读借用 value | 不写回，不能保存 pointer |
| `out` | 可写但初始值未定义/由 contract 约束 | 必须显式 `WriteBackArgument` 或 result path |
| `ref` | 原 place 的受约束借用 | 写回会触发类型、loan 和 write barrier |

写回示例：

```c
static TZrBool increment_out(
    ZrLibCallContext *context,
    SZrTypeValue *result) {
    SZrTypeValue replacement;
    if (!ZrLib_CallContext_CheckArity(context, 1u, 1u)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, &replacement, 1);
    if (!ZrLib_CallContext_WriteBackArgument(context, 0u, &replacement)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}
```

`WriteBackArgument` 失败表示原 place 不可写、类型不匹配、loan 已失效或 callback 不允许
该 mode；不要把失败当作“调用完成”。对 `in` 参数调用写回是错误，应在 descriptor/semantic
阶段拒绝，而不是依赖 callback 的运行时分支。写回发生在 callback 返回前，runtime 才能把
它纳入异常回滚和 GC barrier。

## 4. managed object、array 和 string

### 4.1 先分配，再 root，再触发可能分配的操作

任何 `ZrLib_Object_New`、`ZrLib_Array_New`、`ZrLib_Type_NewInstance` 或模块调用都可能
触发 GC/safepoint。新对象在下一次分配前必须进入 temp root：

```c
static TZrBool make_pair(
    ZrLibCallContext *context,
    SZrTypeValue *result) {
    ZrLibTempValueRoot root;
    SZrObject *object;
    SZrTypeValue left;
    SZrTypeValue right;
    TZrInt64 leftValue;
    TZrInt64 rightValue;

    if (!ZrLib_CallContext_CheckArity(context, 2u, 2u) ||
        !ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
        return ZR_FALSE;
    }

    object = ZrLib_Object_New(context->state);
    if (object == ZR_NULL) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }
    ZrLib_TempValueRoot_SetObject(&root, object, ZR_VALUE_TYPE_OBJECT);

    if (!ZrLib_CallContext_ReadInt(context, 0u, &leftValue) ||
        !ZrLib_CallContext_ReadInt(context, 1u, &rightValue)) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, &left, leftValue);
    ZrLib_Value_SetInt(context->state, &right, rightValue);
    ZrLib_Object_SetFieldCString(context->state, object, "left", &left);
    ZrLib_Object_SetFieldCString(context->state, object, "right", &right);

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}
```

上例中的 `SZrTypeValue` 初始化应按项目 value helper 约定完成；实际代码可先读取到独立
`TZrInt64`，再用 `ZrLib_Value_SetInt` 写入 value，避免直接依赖 union layout。关键顺序是：
创建 -> root -> 可能分配的字段/模块操作 -> result 写入 -> End。root 结束后，result 或
调用 frame 必须仍持有对象的 managed 引用。

### 4.2 Array helper

```c
static TZrBool make_numbers(
    ZrLibCallContext *context,
    SZrTypeValue *result) {
    ZrLibTempValueRoot root;
    SZrObject *array;
    SZrTypeValue item;
    TZrInt64 value;

    if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
        return ZR_FALSE;
    }
    array = ZrLib_Array_New(context->state);
    if (array == ZR_NULL) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }
    ZrLib_TempValueRoot_SetObject(&root, array, ZR_VALUE_TYPE_ARRAY);
    for (TZrSize i = 0; i < ZrLib_CallContext_ArgumentCount(context); ++i) {
        if (!ZrLib_CallContext_ReadInt(context, i, &value)) {
            ZrLib_TempValueRoot_End(&root);
            return ZR_FALSE;
        }
        ZrLib_Value_SetInt(context->state, &item, value);
        if (!ZrLib_Array_PushValue(context->state, array, &item)) {
            ZrLib_TempValueRoot_End(&root);
            return ZR_FALSE;
        }
    }
    ZrLib_Value_SetObject(context->state, result, array, ZR_VALUE_TYPE_ARRAY);
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}
```

`ZrLib_Array_Get` 返回借用的元素指针；读取后若要保存，调用 `ZrCore_Value_Copy` 或写入
另一个 rooted value。不要在持有 `Array_Get` 指针期间扩容数组、触发可能移动 backing
storage 的操作或把该指针交给异步 callback。

## 5. 调用另一个 ZR 函数或模块 export

### 5.1 module export

```c
static TZrBool call_export(
    ZrLibCallContext *context,
    SZrTypeValue *result) {
    SZrTypeValue argument;
    TZrInt64 input;
    if (!ZrLib_CallContext_ReadInt(context, 0u, &input)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, &argument, input);
    return ZrLib_CallModuleExport(context->state,
                                  "app.math",
                                  "doubleValue",
                                  &argument,
                                  1u,
                                  result);
}
```

`ZrLib_CallModuleExport` 会重新执行 module lookup、arity/type 检查和 call-binding；它不是
绕过 contract 的 C 函数指针跳转。传入的 arguments 数组在调用返回后失效，result 由调用
方提供。若 export 触发异常，函数返回 false，宿主应读取当前 VM exception，不要覆盖成
通用“call failed”。

### 5.2 任意 callable value

`ZrLib_CallValue(state, callable, receiver, arguments, count, result)` 用于 callback 参数
或动态 callable。receiver 可以为 `ZR_NULL`；但 callable value 和 arguments 中的 managed
object 在 call 期间必须被当前 frame/root 持有。不要缓存 `SZrTypeValue *` 指针跨越 call，
因为 nested call 可能扩展 stack 或触发 GC。

## 6. type prototype 和 module metadata

```c
SZrObjectPrototype *prototype =
    ZrLib_Type_FindPrototype(state, "app.Point");
if (prototype == ZR_NULL) {
    return ZR_FALSE;
}
SZrObject *point =
    ZrLib_Type_NewInstanceWithPrototype(state, prototype);
```

`FindPrototype` 返回当前 registry/generation 的借用 prototype；不要把它写入全局静态变量。
`ZrLib_Type_NewInstance`/`NewInstanceWithPrototype` 只负责创建 managed object，构造器是否
需要调用、字段是否初始化、类型是否 abstract 由 descriptor/compiler contract 决定。模块
读取用 `ZrLib_Module_GetLoaded`/`ZrLib_Module_GetExport`，不要直接访问 module object 的
hidden storage。

## 7. inline value 和高性能 dispatch

对 struct/数学值，runtime 可能把参数放在 inline frame，而不是 boxed object。此时使用：

```c
ZrLibInlineArgumentView view;
if (!ZrLib_CallContext_InlineArgumentView(context, 0u, &view) ||
    !view.span.available) {
    return ZR_FALSE;
}
/* Read only for this short window. Do not retain view.span.address. */
consume_inline_bytes(view.span.address,
                     view.span.byteSize,
                     view.span.byteAlign,
                     view.span.typeLayoutId);
```

`InlineArgumentSpan`/`InlineArgumentView` 的 address、layout 和 registry 都是 borrowed。栈
增长、nested call、GC safepoint 或 dispatch lane 变化后必须重新取得 view。descriptor 应
根据实际 callback 选择 flags：

| flag | 适用场景 | 约束 |
| --- | --- | --- |
| `STACK_ROOT_CONTEXT` | callback 需要 stack-root context | 不把 raw stack pointer 外传 |
| `INLINE_VALUE_CONTEXT` | 读取 inline aggregate | 必须处理 unavailable/fallback |
| `READONLY_INLINE_VALUE_CONTEXT` | 只读 inline aggregate | 不得 WriteBack |
| `RESULT_ALWAYS_WRITTEN` | callback 总是写 result | null 也要显式写入 |
| `RESULT_OPTIONAL` | 允许不写 result | 与 caller contract 一致 |
| `READONLY_RECEIVER` | receiver 不可变 | 不调用写字段 helper |
| `BLOCKING_DETACHED` | 明确允许阻塞调用 | 不持有 VM stack borrow |
| `NO_SAFEPOINT_CRITICAL` | 临界区禁止 safepoint | 只能执行短、无分配操作 |

`BLOCKING_DETACHED` 与 `NO_SAFEPOINT_CRITICAL` 是互斥的 safepoint policy；错误组合应在
descriptor validation 阶段拒绝。inline fast callback（例如 readonly property get）必须
和普通 callback 保持相同的 contract result 与异常语义，不能只在快路径“猜”类型。

## 8. 错误、异常和 no-return helper

参数类型或 arity 错误可调用 no-return helper：

```c
if (!ZrLib_CallContext_ReadString(context, 0u, &name)) {
    ZrLib_CallContext_RaiseTypeError(context, 0u, "string");
}
```

`ZrLib_CallContext_RaiseTypeError` 和 `RaiseArityError` 标记为 `ZR_NO_RETURN`；调用后不要
继续访问 context/result。业务失败若已有 VM exception，应返回 false；不要在 callback 内
直接 `longjmp`、`abort` 或写 stderr 绕过 runtime handler。exception 传播会恢复 call-info、
stack top、handler depth 和 pending-control 状态。

callback 中如果调用可能抛错的 module/export，失败路径应先结束 temp root，再返回 false。
root 的 End 本身必须幂等但不能重复调用同一个 active root；推荐单一 cleanup label：

```c
TZrBool ok = ZR_FALSE;
ZrLibTempValueRoot root;
if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
    return ZR_FALSE;
}
/* ... operations that may fail ... */
ok = ZR_TRUE;
ZrLib_TempValueRoot_End(&root);
return ok;
```

## 9. 线程、阻塞和异步边界

普通 native callback 在进入时绑定当前 `SZrState`、stack/frame 和 GC domain。不能把
`ZrLibCallContext`、`SZrTypeValue *`、`SZrObject *`、inline span 或 `SZrString *` 交给另一个
线程。需要异步工作时：

1. 在 callback 内复制 primitive/bytes 或创建拥有明确 owner 的 transfer value；
2. 结束当前 callback、root 和 borrowed view；
3. 由 task/thread provider 在合法 scheduler/domain 中执行；
4. 回到 owner state 后重新构造 result，并通过 Task/Channel/transfer contract 发布。

阻塞的 foreign call 只有在 descriptor 声明 `BLOCKING_DETACHED` 且实现不持有 VM borrow 时
才可执行。`zr.task`/`zr.thread` 的 Send/Sync、isolated quota 和 Task bridge 规则见
[Task API](../03-modules/task-api.md) 与 [Thread API](../03-modules/thread-api.md)。

## 10. callback 单元测试模板

测试 native callback 时不要只测“返回 42”。至少覆盖：

| 用例 | 断言 |
| --- | --- |
| 正常 arity/type | result type/value 正确，exception 清空 |
| 少/多参数 | false 或 structured arity error，不读取越界 slot |
| 错误类型 | type error code，out value 未被伪造 |
| GC/safepoint | rooted object 在分配和 nested call 后仍有效 |
| out/ref | place 写回、barrier 和失败回滚 |
| inline | available/fallback 两条路径，layout id 被检查 |
| nested exception | root cleanup、stack/call-info 恢复 |
| reload/generation | 旧 callable 被拒绝，新的 binding 重新解析 |

测试 fixture 可以使用 `tests/library/test_native_binding_direct_call.c` 的 descriptor 和
runtime helper；不要 include private registry struct 来模拟公共 API 的成功条件。需要验证
call-binding contract 时，再加入 `native_binding_call_binding.h` 并断言 module/signature/
layout hash。

## 11. 释放顺序速查

```text
read borrowed args
  -> begin temp root
  -> allocate/call nested functions
  -> write back out/ref
  -> set result
  -> end temp root
  -> return true/false
```

callback 返回后，runtime 负责 call frame、temporary argument window 和 native binding lookup
cache；模块作者负责自己创建的 external resource、foreign callback、library handle 和
线程。跨 reload 或 global close 时，先停止外部线程/回调，再释放 provider/module，最后才
销毁 state/global。更高层生命周期见 [C 宿主集成指南](c-host-guide.md) 和
[C 值与 GC 生命周期](c-api-value-lifecycle.md)。
