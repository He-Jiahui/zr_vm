---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
tests:
  - tests/ffi/test_native_extern_contract.c
  - tests/library/test_official_provider_convergence.c
  - tests/library/test_native_registry_descriptor_invalidation.c
  - tests/core/test_call_binding_runtime.c
  - zr_vm_aot/tests/parser/test_meta_call_pipeline.c
plan_sources:
  - user: 2026-09-10 持续扩充 ZrVm Wiki 的语言实现与 C API 说明
  - docs/library-and-builtins/index.md
  - docs/core-runtime/state-lifecycle.md
  - docs/plans/aot/02-typed-value-and-layout.md
doc_type: api-reference
---

# Native Call Context 与回调 C API

**状态：`current`。** 本页是 `native_binding.h` 中 native function、method 和 meta method
callback 的逐项调用约定。它关注单次调用里的参数、结果、GC/root、inline storage、错误和重入；
module descriptor 的注册与插件生命周期另见[Native Module 编写](native-module-authoring.md)、
[Native Provider Descriptor 与注册协议](../03-modules/native-provider-descriptor-reference.md)。

## 1. 回调的唯一入口形状

普通 bound callback 使用：

```c
typedef TZrBool (*FZrLibBoundCallback)(ZrLibCallContext *context,
                                       SZrTypeValue *result);
```

它可用于 `ZrLibFunctionDescriptor`、`ZrLibMethodDescriptor` 和 `ZrLibMetaMethodDescriptor`。
dispatcher 在当前 `SZrState`、当前 call-info 和已解析 descriptor 的范围内构造 `ZrLibCallContext`，
选择 stack-root / fast / inline-pinned 等执行 lane，并在 callback 返回后检查成功值和 thread status。

### 回调返回值的含义

| 返回 | 要求 | dispatcher 后续行为 |
| --- | --- | --- |
| `ZR_TRUE` | `result` 已被赋成有效的 ZR value，state 仍处于可继续状态。 | 将结果交给调用者，必要时同步 stack/self。 |
| `ZR_FALSE` | 已建立语言异常、资源/取消失败，或下层 helper 已报告失败。 | 立即走调用失败路径；不得把 `result` 当作正常值使用。 |
| `RaiseTypeError` / `RaiseArityError` | 这两个 API 标注 `ZR_NO_RETURN`，用于建立标准语言错误。 | 控制不会作为正常 return 回到 callback 之后。 |

不要让 callback 返回 `ZR_TRUE` 而遗漏 result 初始化，也不要在一个 helper 返回 `ZR_FALSE` 后继续
写入 caller 的 object/argument。失败不是“返回 null”的同义词。

## 2. `ZrLibCallContext`：可读字段与不可变约束

公开结构包含 `state`、当前 module/type/function/method/meta descriptor、owner/construct target
prototype、argument/self、function base、stack anchor 和 inline-frame 信息。其字段公开是为了
runtime 与高级 adapter 协作，不等价于允许 provider 任意改写它们。

| 信息 | 应使用的 API | 生命周期 |
| --- | --- | --- |
| 参数数量 | `ZrLib_CallContext_ArgumentCount` | 当前 callback。 |
| 第 N 个一般值 | `ZrLib_CallContext_Argument` | 当前 callback；可能是栈/稳定副本视图。 |
| receiver/self | `ZrLib_CallContext_Self` | instance/meta 调用期间借用。 |
| owner prototype | `ZrLib_CallContext_OwnerPrototype` | descriptor/调用边界内借用。 |
| 构造目标 prototype | `ZrLib_CallContext_GetConstructTargetPrototype` | constructor/meta construction 边界内借用。 |
| 可写回 ref/out 参数 | `ZrLib_CallContext_WriteBackArgument` | 只对允许写回的参数语义有效。 |
| inline 原始 storage | `InlineArgumentSpan` / `InlineArgumentView` | 极短借用期，见第 6 节。 |

provider 不应直接修改 `argumentBase`、`argumentValues`、`argumentValuePointers`、stack anchor、
`inlineFrameBase` 或 `rawArgumentCount`。这些字段会随 dispatch lane、stack grow、重定位和
argument passing mode 改变；使用 accessor 可以让 runtime 维持一致的 self/argument 同步。

## 3. 标准 callback 生命周期

```text
descriptor/call binding resolved
  -> dispatcher prepares call-info, stack anchor and context
  -> callback validates arity
  -> callback reads/validates arguments
  -> callback allocates, calls nested code, or mutates through supported APIs
  -> callback writes result or raises language error
  -> dispatcher restores/reacquires anchors and synchronizes write-back/self
  -> caller consumes result or propagates state failure
```

使用下列顺序可避免绝大多数 native 边界错误：

1. 先 `CheckArity`，失败时 `RaiseArityError`。
2. 使用 `ReadInt`、`ReadFloat`、`ReadBool`、`ReadString`、`ReadObject`、`ReadArray` 或
   `ReadFunction` 读取期望形状。
3. 读取失败时使用 `RaiseTypeError`，不要手写不一致的错误对象。
4. 任何可能分配、进入 callable 或触发 safepoint 的操作前，处理 root 和借用 pointer。
5. 通过 `ZrLib_Value_Set*` 写入 `result`；成功时返回 `ZR_TRUE`。
6. 只通过 `WriteBackArgument` 写回具有 ref/out 语义的参数。

## 4. 参数读取、receiver 与 arity

### 4.1 arity

```c
if (!ZrLib_CallContext_CheckArity(context, 2, 2)) {
    ZrLib_CallContext_RaiseArityError(context, 2, 2);
}
```

`minArgumentCount` 和 `maxArgumentCount` 是包含端点的范围。descriptor 中的最小/最大参数数与
callback 中的检查应保持一致；descriptor 宣称 varargs 或 optional 参数时，callback 仍必须自己
处理实际参数个数，而不是无条件访问不存在的 index。

### 4.2 标量和对象读取

| API | 成功输出 | 典型失败原因 |
| --- | --- | --- |
| `ReadInt` | `TZrInt64` | index 越界或值不是可接受的整数表示。 |
| `ReadFloat` | `TZrFloat64` | index 越界或值不是浮点表示。 |
| `ReadBool` | `TZrBool` | index 越界或不是 bool。 |
| `ReadString` | `SZrString *` | 不是 ZR string；返回对象仍是借用。 |
| `ReadObject` | `SZrObject *` | 不是 object。 |
| `ReadArray` | `SZrObject *` | 不是 array object。 |
| `ReadFunction` | `SZrTypeValue *` | 不是可调用 function value。 |

这些 API 的 false 只说明“无法按请求的形状读取”；若这是用户输入错误，应马上调用
`RaiseTypeError(context, index, "ExpectedType")`。不要把 `ReadObject` 的 null output 当作可
安全传给 `ZrLib_Object_SetFieldCString` 的 object。

### 4.3 完整的标量 callback 例子

```c
static TZrBool native_add(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 left;
    TZrInt64 right;

    if (!ZrLib_CallContext_CheckArity(context, 2, 2)) {
        ZrLib_CallContext_RaiseArityError(context, 2, 2);
    }
    if (!ZrLib_CallContext_ReadInt(context, 0, &left)) {
        ZrLib_CallContext_RaiseTypeError(context, 0, "int");
    }
    if (!ZrLib_CallContext_ReadInt(context, 1, &right)) {
        ZrLib_CallContext_RaiseTypeError(context, 1, "int");
    }

    ZrLib_Value_SetInt(context->state, result, left + right);
    return ZR_TRUE;
}
```

整数溢出、转换规则和语言可见 diagnostic 是否需要额外处理，取决于该 descriptor 声明的 ZR
类型契约；不能以 C 的隐式转换替代语言层检查。

### 4.4 Self、owner 与 construct target

`Self` 只对具有 receiver 的调用有意义；static function 或某些 module-level callback 可返回空。
`OwnerPrototype` 表示当前成员所属的 prototype，`GetConstructTargetPrototype` 用于构造语义中
runtime 已解析的目标。三者都是借用值：不能保存到 global 静态表，也不能跨 module reload、
callback return 或另一个 state 使用。

## 5. 结果、值构造和 object/array helper

`result` 是 dispatcher 提供的输出位置。使用 `ZrLib_Value_Set*` 建立正确的 ZR value：

| API | 写入结果 |
| --- | --- |
| `ZrLib_Value_SetNull` | null。 |
| `ZrLib_Value_SetBool` / `SetInt` / `SetFloat` | 标量。 |
| `ZrLib_Value_SetString` | 从 C string 创建/设置 ZR string 值。 |
| `ZrLib_Value_SetStringObject` | 使用已存在的 `SZrString`。 |
| `ZrLib_Value_SetObject` | 用 object 和指定 runtime value type 建立对象值。 |
| `ZrLib_Value_SetNativePointer` | 受 descriptor/FFI contract 约束的 native pointer。 |

构造一般 object 或 array 时，可使用：

```c
SZrObject *object = ZrLib_Object_New(context->state);
SZrObject *array = ZrLib_Array_New(context->state);
```

随后使用 `ZrLib_Object_SetFieldCString`、`ZrLib_Object_GetFieldCString`、`ZrLib_Array_PushValue`、
`ZrLib_Array_Length`、`ZrLib_Array_Get` 操作其公共语言表示。它们比直接写 raw object internals
安全，因为 runtime 能维持对象布局、成员语义和 managed-edge/write-barrier 要求。

按 type name 创建实例时使用 `ZrLib_Type_NewInstance` 或先通过 `ZrLib_Type_FindPrototype` 找到
prototype 再调用 `ZrLib_Type_NewInstanceWithPrototype`。type name 找不到或构造 contract 不允许时，
必须按 state failure 返回，而不是把空 prototype 继续交给 setter。

## 6. Inline 参数：地址只是一张短期借条

有些类型不以独立 object 传递，而是以 frame 内 inline storage 传递。此时：

- `ZrLib_CallContext_InlineArgumentSpan` 返回 `ZrLibInlineSpan`：address、byteSize、byteAlign、
  typeLayoutId 和 `available`；
- `ZrLib_CallContext_InlineArgumentView` 额外给出 `SZrTypeLayout *` 和 layout registry view。

头文件的约束是明确的：**metadata 和 frame storage 都是 borrowed；在 stack growth 或 safepoint
之后必须重新获取。** 因此正确模式是：

```text
获取 InlineArgumentView
  -> 用 byteSize/byteAlign/typeLayout 校验并立即读取或写入
  -> 若要分配、调用 ZR、进入 GC/safepoint：丢弃 address/view
  -> 返回后重新调用 InlineArgumentView
```

不能把 `span.address` 缓存在 provider state、线程局部变量、异步任务或嵌套 callback 中。也不能
把它解释成任意 C struct：layout id、alignment、copy/drop/scan 行为由 ZrVm TypeLayout 决定。若
需要长期保存一个语言值，创建恰当的 value/root/owned representation，而不是保存 inline address。

## 7. `ref`/`out` 参数的写回

`ZrLib_CallContext_Argument` 返回的是当前调用的值视图；修改其内存并不等价于语言层写回。
具有可写回语义时，使用：

```c
SZrTypeValue replacement;

ZrLib_Value_SetInt(context->state, &replacement, 42);
if (!ZrLib_CallContext_WriteBackArgument(context, 0, &replacement)) {
    return ZR_FALSE;
}
```

调用方的参数是否真正可写、是否是 inline/ref/out、是否需要 stack relocation，均由 binding/call
contract 决定。`WriteBackArgument` 失败时不要再直接解引用参数 slot；失败可能已经伴随异常或
thread status 改变。源语言的 `ref`/`out`/spread 调用形状见[表达式、构造与调用形状参考](../02-language/expression-construction-reference.md)。

## 8. 临时 root：跨分配保护新建值

callback 可能先创建 object，再创建 string/array，或嵌套调用其他 ZR function。若第一个 managed
值尚未写入一个已被 VM 扫描的位置，后续分配/safepoint 可能让它失去可达性。此时使用
`ZrLibTempValueRoot`：

```c
ZrLibTempValueRoot root;

if (!ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
    return ZR_FALSE;
}

/* 创建 managed value 后立即写入 root；其后可继续进行会分配的工作。 */
ZrLib_TempValueRoot_SetObject(&root, object, objectValueType);

/* 将最终值写入 result 或 object field 后，结束 root。 */
ZrLib_TempValueRoot_End(&root);
```

若没有 call context，可使用 `ZrLib_TempValueRoot_Begin(state, &root)`。可用操作为：

| API | 作用 |
| --- | --- |
| `TempValueRoot_Value` | 读取 root 中的值槽。 |
| `TempValueRoot_SetValue` | 把已有 `SZrTypeValue` 复制到 root。 |
| `TempValueRoot_SetObject` | 把 object 以指定 runtime type 根住。 |
| `TempValueRoot_SetNull` | 清空 root 内容。 |
| `TempValueRoot_End` | 释放/恢复 root；每个成功 Begin 必须恰好对应一次 End。 |

`ZrLibTempValueRoot` 内部保存 stack/call-info anchors，正是为了应对 stack relocation；但 root
本身也只在当前 state、当前调用同步范围内有效。不要把它放入 heap struct 后跨线程/异步使用。

## 9. 嵌套调用、模块 export 与重入

native code 需要调用语言 callable 时可使用：

```c
TZrBool ZrLib_CallValue(SZrState *state,
                        const SZrTypeValue *callable,
                        const SZrTypeValue *receiver,
                        const SZrTypeValue *arguments,
                        TZrSize argumentCount,
                        SZrTypeValue *result);

TZrBool ZrLib_CallModuleExport(SZrState *state,
                               const TZrChar *moduleName,
                               const TZrChar *exportName,
                               const SZrTypeValue *arguments,
                               TZrSize argumentCount,
                               SZrTypeValue *result);
```

这是 re-entrancy 边界：它可能增长 stack、触发 GC、执行任意 ZR/native 代码、改变 thread status
并使先前借用的参数/self/inline view 失效。调用前先 root 需要保留的 managed values；返回后先检查
boolean/state，再重新通过 call context 获取任何借用指针。

`ZrLib_Module_GetLoaded` 和 `ZrLib_Module_GetExport` 可查询已加载 module/export，但返回对象/值仍
是当前 state 的借用视图。跨调用或跨 reload 保存 export 时，需要走项目/registry 提供的稳定
生命周期，而不是缓存裸 `SZrTypeValue *`。

## 10. 快速 lane 与 callback 作者不应依赖的实现细节

native dispatcher 为常见参数形状选择 stack-root、fast、inline-pinned 等 lane。某些 lane 会把
self/argument 复制为稳定值，某些 lane 用 stack anchor 重获位置，调用后还会同步 self。它们是
runtime 的性能实现，不是 callback 可以观察或假设的 ABI：

- 不要根据 `argumentValues` 是否为空选择行为；
- 不要假定同一函数总在 fast lane 或总有 stack backing；
- 不要修改 context 内部 pointer 来“优化”写回；
- 不要把某次 callback 的 raw address 用于下一次 callback；
- 不要依赖 slot 连续性来实现 varargs/inline aggregate。

只使用本页的 accessor、value/root、object/array 和 nested-call API，provider 即可在不同 dispatch
lane、解释器和 AOT caller 下保持相同语义。

## 11. 与 descriptor、FFI 和插件 ABI 的关系

callback 的参数/返回行为必须与 `ZrLibFunctionDescriptor`、`ZrLibMethodDescriptor` 或
`ZrLibMetaMethodDescriptor` 的最小/最大参数数、parameter descriptor、return type、generic/contract
role 和 module ABI 相匹配。registry 根据 descriptor 生成/验证 call binding；插件版本、能力、
phase 和 public contract hash 由 `ZrLibModuleDescriptor` 与 native registry 管理。

`ZrLib_Value_SetNativePointer` 不是绕过类型系统的通行证。需要把 C ABI 暴露给 ZR 时，仍应建立
明确的 `native extern`/FFI contract、pointer lifetime、release hook 和 callback 约束。详见
[FFI Contract](ffi-contract.md) 与 [FFI Runtime Handle 深度参考](../03-modules/ffi-runtime-handle-reference.md)。

## 12. Native callback 审查清单

1. arity 与 descriptor 一致，并且在访问参数前验证。
2. 所有 type mismatch 通过 `RaiseTypeError`，所有参数数错误通过 `RaiseArityError`。
3. 成功路径总是初始化 `result`，失败路径返回 `ZR_FALSE` 或进入 `ZR_NO_RETURN` 错误 helper。
4. 仅通过 `WriteBackArgument` 改写 ref/out 参数。
5. 跨分配、GC、嵌套 call 或 safepoint 的 managed 值使用 temp root/pin；不保存裸 object/stack pointer。
6. inline span/view 在任一 stack growth 或 safepoint 后重新获取。
7. 通过 object/array/value helper 写 managed edge，不直接改 runtime private storage。
8. callback 不跨 state、global、GC domain 或线程保存 context/prototype/argument view。
9. 所有 `BeginTempValueRoot` 都有一次且仅一次 `End`。
10. 变更后同时验证 descriptor binding、解释器 callback、AOT caller 和异常路径。
