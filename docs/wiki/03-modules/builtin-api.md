---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_common/include/zr_vm_common/zr_meta_conf.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/reflection-provider-contract.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_reflection_type_surface.c
  - tests/module/test_module_system.c
doc_type: api-reference
---

# zr.builtin 详细 API

zr.builtin 是所有 Runtime provider 共同依赖的 N0 类型表面。它把基础协议、根对象、
TypeInfo 以及 primitive boxed wrapper 作为一个有版本和 contract role 的 descriptor 发布。
它不是可以随意替换的“默认库”：registry 会检查其官方 provider role，reflection、AOT、
容器和 LSP 都必须看到同一个 canonical 类型身份。

## 1. 模块身份和注册

当前 descriptor 的关键字段如下：

| 字段 | 当前值 |
| --- | --- |
| moduleName | zr.builtin |
| moduleVersion | 1.0.0 |
| providerPhase | Runtime |
| minRuntimeAbi | ZR_VM_NATIVE_RUNTIME_ABI_VERSION |
| providerContractRole | ZR_PROVIDER_CONTRACT_ROLE_BUILTIN_TYPE_SURFACE |
| isContractOnly | false |
| publicContractHash | 当前 descriptor 未单独填字符串；registry 仍计算结构签名 |

静态宿主可直接取得 descriptor；通常在其它 provider 之前注册：

~~~c
const ZrLibModuleDescriptor *descriptor =
        ZrLibrary_BuiltinModule_GetDescriptor();
if (!ZrLibrary_NativeRegistry_RegisterModule(global, descriptor)) {
    const TZrChar *message =
            ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
    fprintf(stderr, "builtin registration failed: %s\n",
            message != ZR_NULL ? message : "unknown");
}
~~~

共享插件入口仍必须使用 ZrVm_GetNativeModule_v1；builtin 官方模块不能由第三方插件以
相同 moduleName 重新注册。provider role、phase 或 canonical type role 不匹配时，registry
返回 PROVIDER_CONTRACT_MISMATCH、PHASE_MISMATCH、DUPLICATE_OFFICIAL_PROVIDER 或
INVALID_CANONICAL_TYPE_ROLE。

## 2. IArrayLike<T>

IArrayLike 是索引协议，不等价于“内存一定连续”。descriptor 声明一个 length 字段和两个
meta operation：

| 成员 | 形状 | 读取/写入规则 |
| --- | --- | --- |
| length | int | 逻辑元素数；不能从负数或 byte size 推断。 |
| GET_ITEM | (index: int) -> T | 只读 receiver；越界由实现报告。 |
| SET_ITEM | (index: int, value: T) -> T | 需要可写 receiver；写回必须经过 barrier。 |

数组、Span、native view 可以实现同一协议，但 layout、ownership 和 bounds contract 仍由
具体类型提供。一个只读 view 可以只实现 GET_ITEM；若 descriptor 宣称 SET_ITEM，semantic
phase 会把 receiver 视为 writable Place，并在借用冲突时拒绝。

~~~zr
fn sum<T extends zr.builtin.IArrayLike<T>>(items: T): int {
    var total: int = 0;
    var i: int = 0;
    while (i < items.length) {
        total += <int>items[i];
        i += 1;
    }
    return total;
}
~~~

上例只展示协议约束；实际 generic parameter 应使用元素类型而不是把容器本身当作 T。
如果 provider 的 index key 不是 int，应发布自己的 protocol/descriptor，不能伪装成
IArrayLike。

## 3. Equality、hash 和 ordering 协议

### IEquatable<T>

签名为 equals(other: T): bool。它描述语义相等，不要求对象地址相同。Map/Set 的 key
策略必须同时满足：

1. equals 为 true 的两个值产生相同 hash；
2. equals 关系在 key 存活期间保持稳定；
3. 改变参与 equality 的字段后，值不能继续作为旧 hash bucket 中的可变 key。

### IHashable

签名为 hashCode(): int。core 的 ZrCore_Value_GetHash 是底层 fallback；自定义对象若
实现 IHashable，应保证返回值在同一个 registry/generation 内稳定。跨 process 的 hash
是否稳定由 provider 另行声明，不应把它当作序列化 ID。

### IComparable<T>

签名为 compareTo(other: T): int。结果只看符号：负数表示小于，零表示等价，正数表示
大于。实现不应返回未限制的浮点差值作为 int，避免溢出；排序算法只依赖符号和传递性。

### IComparer<T>

签名为 compare(left: T, right: T): int，是无 receiver 的比较策略对象。builtin callback
会先 unbox 两侧 primitive wrapper，再按 numeric、bool、string 或 hash fallback 比较。
因此 comparer 与对象自定义 meta 的调用顺序不同：它是显式策略，不会自动查找 left 的
compareTo。

## 4. Object、Module 与 TypeInfo

### Object

Object 是所有 builtin class 的根类，提供两个静态方法：

| 方法 | 签名 | 实现行为 |
| --- | --- | --- |
| type | type(value: object): string | 对 object/array 返回 prototype name；null 返回 string null；primitive 返回 core value type label。 |
| box | box(value: object): Object | primitive 创建对应 wrapper；已有 object/array 直接保留对象值；不支持的值返回 null。 |

type 的返回是标签，不是 canonical TypeId。需要做类型身份比较时使用 typeid(TypeRef) 或
reflection TypeInfo。box 的 wrapper 会在对象隐藏字段 __zr_builtin_boxed_value 中保存
原始 SZrTypeValue；这个字段不是用户可见 API，不能直接读取或修改。

~~~zr
let objectType = zr.builtin.Object.type(value);
let boxed = zr.builtin.Object.box(42);
let typeIdentity = typeid(MyRecord);
let runtimeDescriptor = typeof(value);
~~~

### Module

Module 继承 Object，是 loaded module object 的根类型。它没有额外公开 method；module
export 应通过 import/module resolver 取得。C 宿主若需要读取导出，使用
ZrLib_Module_GetLoaded 和 ZrLib_Module_GetExport，不能把 Module 当作普通 hash object。

### TypeInfo

TypeInfo 继承 Object，字段来自 builtin descriptor：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| name | string | 短类型名。 |
| qualifiedName | string | 含 module 前缀的完整名。 |
| kind | string | class/struct/interface 等类别标签。 |
| hash | UInt64 | 当前 metadata identity 的稳定 hash。 |
| owner | TypeInfo | 所属类型或成员 owner，可能为空。 |
| module | TypeInfo | 所属 module reflection 对象，可能为空。 |

TypeInfo.box(value) 与 Object.box 使用同一 boxed wrapper contract；TypeInfo 本身不能被
当作任意可构造 class，也不能用字段名推断 layout offset。reflection 页面中的 token 和
generation 检查仍然适用。

## 5. Primitive wrapper

builtin descriptor 发布以下 class：Integer、Float、Double、String、Bool、Byte、Char、
UInt64。它们都是 Object 子类，主要用于 reflection、generic protocol 和需要对象身份的
边界；普通数值运算仍优先使用 unboxed SZrTypeValue。

| wrapper | 来源 value type | 协议 |
| --- | --- | --- |
| Integer | INT8/16/32/64、UINT16/32 | IEquatable、IComparable、IHashable |
| Float | FLOAT | IEquatable、IComparable、IHashable |
| Double | DOUBLE | IEquatable、IComparable、IHashable |
| String | STRING | IEquatable、IComparable、IHashable |
| Bool | BOOL | IEquatable、IHashable |
| Byte | INT8/UINT8 | IEquatable、IComparable、IHashable |
| Char | 字符 value type | IEquatable、IComparable、IHashable |
| UInt64 | UINT64 | IEquatable、IComparable、IHashable |

每个 wrapper 都有：

~~~text
equals(other: T): bool
compareTo(other: T): int
hashCode(): int
~~~

wrapper callback 会先尝试解包隐藏字段，再调用 core 的 CompareDirectly/Equal/GetHash。
这使 boxed 42 与 unboxed 42 在 wrapper 方法中得到一致的语义结果，但并不意味着两个
SZrTypeValue 的 value type 自动变成相同；跨类型 equality 仍应由 semantic/provider contract
明确处理。

## 6. Canonical type roles

zr.builtin 同时发布 metadata root 和其子 role：

| canonicalName | role | projection |
| --- | --- | --- |
| zr.builtin.TypeInfo | BUILTIN_METADATA_ROOT | metadata members |
| Class | BUILTIN_METADATA_CLASS | erased |
| Struct | BUILTIN_METADATA_STRUCT | erased |
| Function | BUILTIN_METADATA_FUNCTION | callable members |
| Field | BUILTIN_METADATA_FIELD | erased |
| Method | BUILTIN_METADATA_METHOD | callable members |
| Property | BUILTIN_METADATA_PROPERTY | erased |
| Parameter | BUILTIN_METADATA_PARAMETER | erased |
| Object | BUILTIN_METADATA_OBJECT | erased |

role 的 parentRole、surfaceFlags 和 projectionKind 会参与 registry 的 canonical lookup。
reflection 查询得到的同名类型如果来自另一 provider、另一 generation 或另一 projection，
必须视为不兼容，不能只比较字符串。

## 7. C 侧 wrapper 和调用

native callback 通常不直接构造 wrapper 字段，而是使用 library helper：

~~~c
SZrTypeValue source;
SZrTypeValue boxed;
ZrCore_Value_InitAsInt(state, &source, 42);

/* Object is a module export; box is a static member on that export. */
const SZrTypeValue *objectExport =
        ZrLib_Module_GetExport(state, "zr.builtin", "Object");
SZrTypeValue objectReceiver;
SZrString *boxName = ZrCore_String_CreateFromNative(state, "box");
ZrCore_Value_ResetAsNull(&boxed);
if (objectExport == ZR_NULL || boxName == ZR_NULL) {
    return ZR_FALSE;
}
ZrCore_Value_Copy(state, &objectReceiver, objectExport);
if (!ZrCore_Object_InvokeMember(state, &objectReceiver, boxName,
                                &source, 1u, &boxed)) {
    /* inspect the current VM exception */
}
~~~

更低层的 native module callback 可以调用 ZrLib_Type_FindPrototype、
ZrLib_Type_NewInstanceWithPrototype、ZrLib_Object_SetFieldCString 和
ZrLib_TempValueRoot_SetObject，但必须把新对象放入 root 后再触发可能分配的操作。隐藏
字段名是实现细节；模块作者不应依赖 __zr_builtin_boxed_value 的布局。

注册表查询 builtin role 的典型流程：

~~~c
ZrLibRegisteredCanonicalTypeRole role;
if (ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
            global, ZR_CANONICAL_TYPE_ROLE_BUILTIN_METADATA_ROOT, &role)) {
    const ZrLibModuleDescriptor *provider = role.provider;
    const ZrLibCanonicalTypeRoleDescriptor *typeRole = role.typeRole;
    printf("%s -> %s\n", provider->moduleName, typeRole->canonicalName);
}
~~~

## 8. 失败与兼容性

| 情况 | 结果 |
| --- | --- |
| 第三方重复注册 zr.builtin | registry 拒绝 DUPLICATE_OFFICIAL_PROVIDER。 |
| wrapper 传入不兼容 other | equals 返回 false 或 compare callback 走明确 fallback；不会强制转换。 |
| TypeInfo token 跨 generation 使用 | reflection 返回 stale/not-found 类失败。 |
| 直接修改隐藏 boxed 字段 | 破坏 wrapper equality/hash，属于未定义的内部操作。 |
| 只实现 equals 不实现 hashCode | 可以作为线性比较对象，但不应作为 Map/Set key。 |
| 用 type() 字符串替代 TypeId | reload、generic 或跨 module 时可能发生误判。 |
要查看该模块在完整官方清单中的 tier、phase 和注册限制，请参阅[官方 Provider 矩阵](provider-matrix.md)；
要实现自定义协议，请参阅[Native Module 编写](../05-interop/native-module-authoring.md)。
