---
related_code:
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/ownership.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍 C native 库调用方案和宿主接口
  - docs/core-runtime/index.md
  - docs/library-and-builtins/index.md
tests:
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/parser/test_value_type_runtime.c
  - tests/container/test_temp_value_root.c
doc_type: api-reference
---

# Core 值、字符串、对象与模块 C API

`SZrTypeValue` 是 C API 中传递 ZR 值的基本单元；它既能表达 native 标量，也能携带 managed
object、ownership control 和 weak reference。对象、字符串、模块和 prototype 都可能由 GC 管理，
所以“一个 C 指针能否保存”取决于其 root/pin/owner 状态，而不是该指针的 C 类型。本页说明值
构造、复制、成员访问和 module cache 的正式边界。

先读 [C API 约定](c-api.md) 了解 `ZR_NULL`、`TZrBool` 和 `ZR_OUT`；native callback 的参数
读取和 result 写入见 [Native Callback C 实战配方](native-callback-recipes.md)。GC pin、barrier 和
异常路径见 [Core GC、异常与执行安全 API](core-gc-exception-api.md)。

## 1. `SZrTypeValue` 不变式

公开布局包含 `type`、`value`、`isGarbageCollectable`、`isNative` 和 ownership 元数据。最重要的
规则不是字段值本身，而是字段组合必须由 API 保持一致：

| 值类别 | 推荐构造 API | 可直接保留 C 指针吗 | 释放路径 |
| --- | --- | --- | --- |
| `null` | `ZrCore_Value_ResetAsNull` | 无对象 | 无 |
| 整数/无符号/布尔/浮点 | `InitAsInt`、`InitAsUInt`、`InitAsBool`、`InitAsFloat` | 不涉及 heap object | 无 |
| native pointer | `InitAsNativePointer` | 只表示外部地址，不授予对象所有权 | 外部 owner 负责 |
| managed raw object | `InitAsRawObject` | 仅当前根/调用/GC 安全期 | root/pin/owner API |
| unique/shared/weak/borrowed/loaned 值 | ownership API | 不自动跨 callback/await/thread 安全 | `ReleaseValue` 或相应 ownership 转换 |

禁止只写 `value->type` 或 `value->value` 来构造一个值。这样会遗漏 `isGarbageCollectable`、
`isNative`、ownership control 或写屏障，从而造成 GC 漏标、重复释放或 use-after-move。

### 1.1 标量和 null

```c
SZrTypeValue left;
SZrTypeValue right;
SZrTypeValue result;

ZrCore_Value_InitAsInt(state, &left, 20);
ZrCore_Value_InitAsInt(state, &right, 22);
ZrCore_Value_ResetAsNull(&result);

/* 使用已验证的 callback/object/call-binding 路径填充 result。 */
```

`ZrCore_Value_ResetAsNull` 仅复位目标 slot；它不是 ownership release。若目标之前含
unique/shared/loaned value，先调用 `ZrCore_Ownership_ReleaseValue(state, &value)`，再 reset。
否则 control block 或资源 finalizer 可能被跳过。

### 1.2 copy、比较、hash 和元方法

| API | 语义 | 调用前后条件 |
| --- | --- | --- |
| `ZrCore_Value_CopySlow` | 完整值复制，保留 managed/ownership 语义 | destination 不能与 source 发生未定义重叠；若 destination 有 owner，先按契约清理 |
| `ZrCore_Value_CanFastCopyPlainHeapObject` | 判断是否可用 plain fast copy | 只用于 runtime optimization，不是应用逻辑分支 |
| `ZrCore_Value_Equal` | ZR 相等语义 | 可能走对象/meta 比较并建立错误状态 |
| `ZrCore_Value_CompareDirectly` | 直接比较路径 | 用于已知相容类型，不替代用户语言比较语义 |
| `ZrCore_Value_GetHash` | ZR hash | hash 合约必须与 `Hashable`/container 合约一致 |
| `ZrCore_Value_ConvertToString` | 转换为 managed string | 返回值按 managed object 对待 |
| `ZrCore_Value_CallMetaMethod` | 调用指定 meta 槽 | 需要可用 state 和正确 result slot |
| `ZrCore_Value_CallMetaToString` / `ToDebugString` | 文字/debug 投影 | 仅用于展示，不能当作稳定 identity |

调用 `Equal`、hash 或 meta 可能分配、执行用户覆盖逻辑或抛出异常。因此 native callback 在调用前
必须保护仍需使用的 managed 参数，调用后立即检查自己的成功/异常约定；不能假设“比较永远是
纯函数”。

## 2. ownership 值 API

`ownership.h` 是 ZR `own/share/borrow/loan/degrade/wake/detach` 语义在 C 层的实现入口：

| 函数 | 源级概念 | destination 的状态 | 典型失败 |
| --- | --- | --- | --- |
| `ZrCore_Ownership_InitUniqueValue` | 创建 unique owner | 新 unique control | 目标不是可拥有 managed object |
| `ZrCore_Ownership_UniqueValue` | 取得/转换 unique | 旧源可能被 move | alias 或借用仍存活 |
| `ZrCore_Ownership_BorrowValue` | borrow | 短期 borrowed view | escape 到 owner/frame 之外 |
| `ZrCore_Ownership_LoanValue` | loan | 临时 loaned view | loan 同时跨写入、return 或 await |
| `ZrCore_Ownership_ReturnLoanValue` | 归还 loan | 恢复可用 owner | 不匹配的 loan control |
| `ZrCore_Ownership_ShareValue` | shared ownership | strong-ref 增加 | 资源/类型约束不允许共享 |
| `ZrCore_Ownership_SharePlainValue` | plain -> shared | 封装为共享表示 | 值不满足可装箱规则 |
| `ZrCore_Ownership_DegradeValue` | shared -> weak | weak reference | 源没有 compatible control |
| `ZrCore_Ownership_WakeValue` | weak -> strong 尝试 | 成功才得到 live owner | 目标已释放；这不是异常恢复 |
| `ZrCore_Ownership_DetachValue` | 从 owner 关系脱离 | 新的 transfer state | 不允许的 alias/region |
| `ZrCore_Ownership_ReturnToGcValue` / `IntoGcBoxValue` | owner -> GC/box | 托管表示 | layout 或 ownership 不兼容 |
| `ZrCore_Ownership_ReleaseValue` | 释放 value owner | destination 应不再使用旧 owner | double release、仍有借用 |

`ZrCore_Ownership_AssignValue` 是赋值的完整 ownership-aware 入口；它优先于 `memcpy` 或裸
assignment。强引用观察器由 global setter 安装，`GetStrongRefCount` 适合诊断和测试，不能用来
写竞态敏感的业务逻辑。

## 3. managed 字符串

### 3.1 创建和读取

```c
SZrString *name = ZrCore_String_CreateFromNative(state, "plugin.answer");
if (name == ZR_NULL) {
    return ZR_FALSE;
}

const TZrChar *bytes = ZrCore_String_GetNativeString(name);
TZrSize byteLength = ZrCore_String_GetByteLength(name);
```

| API | 说明 | 生命周期 |
| --- | --- | --- |
| `ZrCore_String_Create` | 按 byte length 创建 ZR string | 返回值可能在之后 GC；根/pin 后才可跨 allocation 保存 |
| `ZrCore_String_CreateFromNative` | `strlen` 版本的便利 wrapper | 输入必须是 NUL 结尾 native string |
| `ZrCore_String_CreateTryHitCache` | 允许命中 global cache | 不把返回地址当作永久 interned identity |
| `GetNativeString` / `GetByteLength` | 读取 UTF-8 bytes/长度 | 返回 bytes 借用 string；string 移动或释放后失效 |
| `GetCodePointLength` | 计算 Unicode code-point 数 | 失败时检查 return，不要把 byte 长度当字符数 |
| `CodePointCountToByteOffset` | code point -> UTF-8 offset | UI/LSP 切片应使用这个而不是直接索引 bytes |
| `ToByteArray` | 物化为 ZR array | array 是 managed object |
| `Equal` | 内容比较 | 与 native `strcmp` 的 null/生命周期规则不同 |

`String_Concat` 和 `String_ConcatSafe` 使用 VM 栈/运行时约定，主要给 interpreter 使用。native
业务代码若只是拼接两项，优先 `ZrCore_String_ConcatPair` 或 `ConcatStringAndNative`；两者仍可能
分配和触发 GC。不要保存 `GetNativeString` 的指针并随后调用 concat、container mutation 或
native module import。

## 4. 对象和成员访问

### 4.1 构造对象

| API | 作用 | 使用边界 |
| --- | --- | --- |
| `ZrCore_Object_New(state, prototype)` | 按 prototype 创建普通对象 | prototype 必须属于同一 global/type graph |
| `ZrCore_Object_NewCustomized` | 创建指定大小/内部类型对象 | runtime/provider 实现用；调用方负责遵守 internal type layout |
| `ZrCore_Object_NewInlineArray` | 创建 inline array object | element layout/type contract 必须已验证 |
| `ZrCore_Object_Init` / `Deconstruct` | 初始化/解构对象字段 | 仅由 object owner/runtime 调用；不是任意 object reset |
| `ZrCore_Object_CloneStruct` | 依据 struct layout 复制 | managed fields 会走相应 copy/drop 规则 |

新对象并不自动成为长期 C root。若 native callback 只把它马上写入 `result` 或一个被正确
barrier 的 managed container，运行时路径会建立可达性；若中间还要分配、调用用户代码或跨出
callback，先阅读 [C 值与 GC 生命周期](c-api-value-lifecycle.md)。

### 4.2 字段、属性和方法

| API | 层级 | 何时使用 |
| --- | --- | --- |
| `ZrCore_Object_SetValue` / `GetValue` | 原始 object key/value 表 | 动态对象或 runtime 结构 |
| `ZrCore_Object_GetMember` / `SetMember` | prototype 成员语义 | native 访问公开 member 的默认入口 |
| `ZrCore_Object_InvokeMember` | 查找并调用 member | 希望遵循方法解析、receiver/meta 的调用方 |
| `ResolveMemberCallable` + `InvokeResolvedFunction` | 拆分解析和调用 | runtime cache/调试器需要保留解析结果时 |
| `TryInvokePropertyGetter/SetterWithReceiverSource` | property fast/path-aware dispatch | compiler/runtime glue；`outHandled` 区分“未处理”和“失败” |
| `GetByIndex` / `SetByIndex` | index contract | 遵循 `Indexable`/meta 语义 |

`SetExistingPairValueUnchecked`、`SetMemberCachedDescriptorUnchecked` 和
`*NoWriteBarrier*` 是性能内部接口。它们的前提包括 descriptor 已验证、对象刚分配或目标
generation/remembered set 已被维护。third-party native 模块不应把它们当作普通 setter；优先
`SetMember` 或 descriptor callback helper。

### 4.3 Array/iterator fast path

`SuperArray*` 和 `TryIter*Cached*` API 主要服务 compiler 已证明的 inline-array 布局与 iterator
缓存。`Try...` 带 `outApplicable` 的函数并非“失败就继续用 result”：

1. 函数返回 false 时，先处理运行时错误；
2. 返回 true 而 `outApplicable == false` 时，走通用 `GetByIndex`/`IterMoveNext` 路径；
3. 只有返回 true 且 applicable 才读取 fast-path 结果。

这条三态规则防止 native 模块把 layout mismatch 误当作空数组或有效 null。

## 5. prototype 与协议描述

构造类型/descriptor 的核心 API：

| API | 作用 |
| --- | --- |
| `ZrCore_ObjectPrototype_New` / `ZrCore_StructPrototype_New` | 创建对象或 struct prototype |
| `ZrCore_ObjectPrototype_SetSuper` | 设置继承关系 |
| `InitMetaTable` / `AddMeta` | 初始化并填充 operator/meta 槽 |
| `AddMemberDescriptor` / `FindMemberDescriptor` | 注册/查询可见成员描述 |
| `ZrCore_StructPrototype_AddField` | 注册 struct 字段及 offset |
| `AddManagedField` | 声明需要 GC/ownership 扫描的字段 |
| `SetIndexContract` / `SetIterableContract` / `SetIteratorContract` | 声明 index/iterator 协议投影 |
| `AddProtocol` / `ImplementsProtocol` | 声明/查询 protocol bit |
| `Object_IsInstanceOfPrototype` | 运行时 instance 检查 |

prototype 的结构、字段 offset、managed field 表和 protocol 位都是 ABI/布局合同。修改已有
prototype 后使用 `ZrCore_ObjectPrototype_MarkMutation`，并让 call-binding/cache generation
按 runtime 规则失效。不要在已有对象被并发读取时修改 prototype。

## 6. module 对象与缓存

### 6.1 模块构造

```c
SZrObjectModule *module = ZrCore_Module_Create(state);
SZrString *moduleName = ZrCore_String_CreateFromNative(state, "host.example");

ZrCore_Module_SetInfo(state, module, moduleName, /* version/type fields from owner */);
/* Register exports through descriptor or explicit Module_AddPubExport. */
```

由于 `SetInfo` 的完整参数体现 runtime module metadata，应用代码应以 `module.h` 当前签名为准，
不要从旧版本示例复制字段顺序。一般 native provider 应通过 `ZrLibModuleDescriptor` 与 registry
注册，不手工拼 module；手工 API 主要服务 Core loader、AOT glue 和测试。

| API | 作用 | 注意 |
| --- | --- | --- |
| `ZrCore_Module_Create` | 创建 module object | 由 state/global GC 域管理 |
| `SetInfo` | 设置 module identity/metadata | 统一由 loader 设置，避免 identity 与 cache key 脱节 |
| `AddPubExport` / `AddProExport` | 添加 public/protected export | value 必须满足 module owner/lifetime |
| `GetPubExport` / `GetProExport` | 查询 export | 返回 `const SZrTypeValue *` 是 module 借用引用 |
| `RegisterExportDescriptor` / `SetExportDescriptorReady` | 注册并完成 export metadata | 支持循环依赖/延迟 materialization 的内部阶段 |
| `SetInitializationState` | 变更 module init 状态 | 不可跳过 loader 的状态机 |
| `AttachMetadataRuntime` / `GetMetadataRuntime` | 关联反射/metadata runtime | metadata lifetime 与 module 同步 |

### 6.2 cache、artifact 与 import

| API | 说明 |
| --- | --- |
| `ZrCore_Module_CalculatePathHash` | 计算 path cache hash；仅同一 global/cache identity 内有意义 |
| `GetFromCache` / `AddToCache` / `RemoveFromCache` | 操作 global module cache；不要自行释放 cache 命中的 module |
| `OpenCanonicalArtifact` | 以 canonical artifact 形式打开模块 | 返回 `EZrArtifactStatus`，不能只看 pointer |
| `ImportByPath` | 按路径 import | 可触发 source/native/AOT loader，失败原因由 module diagnostic/exception 给出 |
| `ImportNativeEntry` / `ImportGuardNativeEntry` | VM native entry | compiler/runtime 生成用，不是 C host 的 import API |

cache key 是 canonical module identity/path，不是 display string。host 若要 reload plugin/source，必须
经过 registry/plugin invalidation 和 module 生命周期处理，不能只 `RemoveFromCache` 后强行关闭
动态库。

## 7. C native callback 中的安全范式

```c
static TZrBool host_set_tag(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrState *state = context->state;
    SZrTypeValue tag;
    SZrString *key = ZrCore_String_CreateFromNative(state, "tag");

    if (key == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(state, &tag, 7);

    /* 用 callback helper 取得 receiver 后，调用 checked member API。 */
    if (!ZrCore_Object_SetMember(state, /* receiver */, key, &tag)) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(result);
    return ZR_TRUE;
}
```

这个示例刻意省略 receiver 提取，因为 receiver 是否存在、对象类型和 parameter passing mode 必须
由 descriptor 决定。不要从 `context` 的内部字段猜参数地址；使用 `native_binding.h` 的 argument
view/helper。调用 `SetMember` 后若还要继续使用新建 string 或 receiver 并可能分配，应先建立
合适 root/pin。

## 8. 高危误用清单

| 误用 | 后果 | 替代 |
| --- | --- | --- |
| `memcpy` 复制 `SZrTypeValue` | ownership control、weak ref 或 GC barrier 丢失 | `ZrCore_Value_CopySlow` 或 ownership assign |
| 缓存 `GetNativeString()` 返回地址 | GC/对象生命周期后悬垂 | 复制 bytes 或持有 rooted `SZrString` |
| 使用 `Unchecked` setter 写普通对象 | remembered set/descriptor 前提不成立 | checked `Get/SetMember` 或 descriptor helper |
| 以 `Object *` 地址作持久 identity | moving GC、reload、owner 变化会破坏假设 | 使用 runtime token/handle/descriptor identity |
| 手写 prototype field offset | layout/GC scan/ABI 不一致 | 通过 prototype/type-layout API 建立 |
| 把 module export 的借用 value 跨 cache reload 保存 | module 可能失效 | 复制值、root 所需对象，或重新解析 export |

## 9. 继续阅读

- [C 值与 GC 生命周期](c-api-value-lifecycle.md)
- [Core 宿主状态、执行与预算 C API](core-runtime-host-api.md)
- [Native Module 编写](native-module-authoring.md)
- [Native Callback C 实战配方](native-callback-recipes.md)
- [模块、项目与产物](../07-modules-projects-artifacts.md)
