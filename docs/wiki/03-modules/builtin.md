---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/reflection.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/reflection-provider-contract.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_reflection_type_surface.c
  - tests/module/test_module_system.c
doc_type: module-detail
---

# `zr.builtin`

**状态：`current`；Runtime N0 provider，descriptor 版本 `1.0.0`，官方角色
`BUILTIN_TYPE_SURFACE`。**

`zr.builtin` 是 ZrVm 的最小类型与协议根。它不是普通工具库：注册表在创建 compiler
和 runtime module 之前就安装它，parser、类型推断、容器、反射和 AOT 都从这一个
descriptor 取得 canonical protocol/type role。宿主通常不需要显式 `import`；显式导入
只适用于需要访问 `Object` 或 `TypeInfo` 的源码。

## 公开类型

### 协议

| 类型 | 形状 | 运行时/编译器作用 |
| --- | --- | --- |
| `IArrayLike<T>` | `length: int`；索引 GET/SET meta operation | 统一 fixed array、`Array<T>` 和可索引 native view；GET/SET 的索引和值类型由 canonical `TypeId` 检查。 |
| `IEquatable<T>` | `equals(other: T): bool` | `Map`/`Set` 和显式比较使用的语义相等。 |
| `IHashable` | `hashCode(): int` | 哈希容器的稳定 hash；相等对象必须保持相同 hash。 |
| `IComparable<T>` | `compareTo(other: T): int` | 排序和有序比较；返回负数/零/正数表示小于/相等/大于。 |
| `IComparer<T>` | `compare(left: T, right: T): int` | 将比较策略作为值传递给算法。 |

`IArrayLike` 的索引操作在 descriptor 中是 meta method，而不是用户可覆写的普通
`get`/`set` 方法；parser 会把 `a[i]` 投影为 `ZR_META_GET_ITEM` 或
`ZR_META_SET_ITEM`。因此只声明同名函数不能获得该能力，必须通过 protocol fact 或
官方 provider 的 descriptor 注册。

### 根对象与反射对象

| 类型 | 成员 | 说明 |
| --- | --- | --- |
| `Object` | 静态 `type(value: object): string`；静态 `box(value: object): Object` | `type` 返回运行时值类别标签；`box` 将 primitive 包装成对应 wrapper。 |
| `Module` | 无额外成员 | 所有已物化 module object 的共同原型。 |
| `TypeInfo` | `name: string`、`qualifiedName: string`、`kind: string`、`hash: UInt64`、`owner: TypeInfo`、`module: TypeInfo`；静态 `box(value: object): Object` | 反射快照的最小稳定字段；`owner`/`module` 为空时表示没有对应声明归属。 |

`TypeInfo` 是反射 provider 的 builtin root，不等于 `zr.reflection.Type`。后者由
reflection contract provider 提供构造和 invocation 能力，详见[反射与池化](reflection-pooling.md)。

### Primitive wrapper

`Integer`、`Float`、`Double`、`String`、`Bool`、`Byte`、`Char` 和 `UInt64` 都是
继承 `Object` 的 wrapper class。它们共享以下实例方法：

| 方法 | 签名 | 语义 |
| --- | --- | --- |
| `equals` | `equals(other: T): bool` | 解包后按 ZrVm value equality 比较。 |
| `compareTo` | `compareTo(other: T): int` | 解包后按 primitive 的数值/字典序比较。 |
| `hashCode` | `hashCode(): int` | 解包后计算 value hash。 |

映射关系由 `EZrValueType` 决定：`int8..uint32` 使用 `Integer`，`uint64` 使用
`UInt64`，`float`/`double` 分别使用 `Float`/`Double`，字符串、布尔、字节和字符
分别使用同名 wrapper。wrapper 内部的 `__zr_builtin_boxed_value` 字段是实现细节，
不会出现在用户可移植的字段列表中。

## 类型与协议机制

1. 注册表为每个协议分配稳定 `EZrProtocolId` 位；canonical type graph 以协议位和
   `TypeId` 保存约束，不通过字符串白名单判断 `IHashable` 等能力。
2. 数组/容器 descriptor 声明 `implements` 和 meta method 后，compiler 将其适配到
   `IArrayLike<T>`、`IEquatable<T>` 等 protocol。缺少 GET/SET、元素类型不一致或
   receiver readonly 规则不满足时，错误在 binding/semantic 阶段产生。
3. `Object.box` 和 `TypeInfo.box` 走同一个 native dispatch role
   `BUILTIN_BOX`。调用前会把 primitive 复制到 wrapper 的 GC-traced hidden field；
   wrapper 作为普通对象参与 GC，解包时不会借用已失效的栈 slot。
4. 反射 metadata 将 `TypeInfo` 的 `name/qualifiedName/kind/hash` 作为只读投影。
   module reload 会更新 generation，不能用旧 `hash` 或旧 `TypeId` 访问新一代对象。

## 注册与覆盖边界

`zr.builtin` 是 N0 官方 owner，不能被插件替换，也不能由工作区源码声明同名
`zr.builtin.*` module。注册时会校验 native plugin ABI、provider phase、canonical role
和 parent graph；缺失父角色、重复 protocol 或错误 projection 都拒绝注册。普通 host
loader 可以继续被 registry 组合，但不能覆盖 builtin role。

## C 调用接口

```c
const ZrLibModuleDescriptor *ZrLibrary_BuiltinModule_GetDescriptor(void);
TZrBool ZrLibrary_NativeRegistry_RegisterModule(
    SZrGlobalState *global,
    const ZrLibModuleDescriptor *descriptor);
const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(
    SZrGlobalState *global,
    const TZrChar *moduleName);
TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(
    SZrGlobalState *global,
    const TZrChar *canonicalName,
    ZrLibRegisteredCanonicalTypeRole *outRole);
```

典型宿主流程是先 attach registry，再取 descriptor；不要复制 descriptor 数组或提前
释放其静态存储：

```c
SZrGlobalState *global = ZrCore_GlobalState_New(allocator, userData, uniqueNumber, callbacks);
if (global == ZR_NULL) {
    return;
}
if (!ZrLibrary_NativeRegistry_Attach(global)) {
    ZrCore_GlobalState_Free(global);
    return;
}
const ZrLibModuleDescriptor *builtin =
    ZrLibrary_NativeRegistry_FindModule(global, "zr.builtin");
/* builtin is borrowed; use it only while global is alive. */
ZrLibrary_NativeRegistry_Free(global);
ZrCore_GlobalState_Free(global);
```

宿主若要调用 `Object.type` 或 `Object.box`，使用通用
`ZrLib_CallModuleExport`/`ZrLib_CallValue` 或 core member invocation；不存在绕过
descriptor 的公开 `builtin_box` C 函数。返回值写入 `SZrTypeValue`，并遵循
[C API 通用约定](../05-interop/c-api.md)的临时 root 和 GC safepoint 规则。

## 失败语义

- 未 attach registry：canonical role 查询返回 false，compiler 不能构造依赖该 role 的
  类型。
- 伪造 `zr.builtin` descriptor：返回 provider-contract/duplicate/role 错误并保留原 owner。
- wrapper 输入不属于可包装 primitive：native call 返回失败并设置 call/类型诊断，
  不产生半初始化对象。
- 旧 artifact 携带过期 builtin module signature：module load 阶段报告
  `assembly_signature_mismatch`，不会静默降级到字符串比较。
