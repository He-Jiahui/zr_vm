---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/typed_call_binding.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_binding_call_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_call_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_encoding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding_hash.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
plan_sources:
  - user: 2026-09-09 继续完善 ZrVm Wiki，要求 C 语言调用接口和实现机制达到说明书级别
  - docs/library-and-builtins/index.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_call_binding_relocation.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_typed_call_binding.c
doc_type: api-reference
---

# Native Registry 与 Call-Binding

Native descriptor、VM 函数、AOT thunk 和 typed function value 不能只靠函数名连接。
ZrVm 使用两层合同：`ZrLibrary_NativeRegistryState` 保存 provider/closure 的实时索引，
`SZrCallBindingContract` 保存编译产物可验证的标量 identity。调用点在运行时用合同重新
解析目标，因而可以检测模块替换、类型布局变化、AOT reload 和 stale generation，而不是
静默跳到同名但不兼容的函数。

## 1. 两层模型

```
ZrLibModuleDescriptor
        | register/materialize
        v
NativeRegistry: module records + binding entries + plugin handles
        | resolve
        v
SZrCallBindingTarget (VM function / native callback / AOT thunk)
        ^
SZrCallBindingContract (artifact/cache 中的稳定字段)
```

registry 保存指针和闭包，生命周期跟随 `SZrGlobalState`；contract 可以写入 `.zro`、
`.zri`、AOT call-binding section 和 LSP cache。`SZrCallBindingTarget` 中的
`callableObject` 由 GC trace，不得由 artifact 持久化，也不能由 provider 直接 free。

## 2. Contract 字段

公共定义位于 `zr_vm_core/call_binding.h`：

| 字段 | 含义 | 校验要求 |
| --- | --- | --- |
| `bindingKind` | NONE、DIRECT、VIRTUAL、INTERFACE、TYPED_FUNCTION | 持久合同必须在 DIRECT 到 TYPED_FUNCTION 之间 |
| `targetMetadataToken` | member definition/reference token | typed function 可为 0，其余必须是 member token |
| `signatureToken` | 签名表 token | 必须是 signature table token |
| `ownerTypeToken` | 接收者 type definition/reference/spec | 非 0 时必须同时有 layout version/hash |
| `signatureHash` | owner/name/返回类型/参数形状的稳定 hash | 不能为 0 |
| `moduleSignatureHash` | provider descriptor 的稳定 hash | 不能为 0 |
| `layoutVersion`/`layoutHash` | receiver prototype layout 合同 | 无 owner 时都必须为 0；有 owner 时都不能为 0 |
| `dispatchSlot` | virtual/interface 槽位 | 只有两种多态 binding 必须提供；其它 kind 必须是 `ZR_CALL_BINDING_SLOT_NONE` |
| `operation` | CALL、GET、SET、META | 不能超过 `ZR_CALL_BINDING_OPERATION_META` |
| `reserved0`/`reserved1` | ABI 扩展保留位 | 当前必须为 0 |

`ZrCore_CallBinding_CheckContract` 先验证这些不变量，再让 resolver 查目标。不要用 C
结构体内存镜像做磁盘格式；头文件明确规定 contract 必须逐字段编码。

## 3. Kind、operation 和 target

### 3.1 Binding kind

| kind | 典型源表达式 | 解析策略 |
| --- | --- | --- |
| `DIRECT` | `module.fn()`、已知静态 method | 由 metadata token + signature 直接定位 |
| `VIRTUAL` | `receiver.virtualFn()` | 先验证 owner layout，再按 dispatch slot 取实现 |
| `INTERFACE` | `interfaceValue.fn()` | 验证 interface relation 和 slot，允许具体类型替换 |
| `TYPED_FUNCTION` | `let f: fn(int) -> int; f(1)` | 没有静态 target token，按 callable value 的签名合同重新核对 |
| `NONE` | 未链接 cache | 只表示尚未形成合同，不能执行 |

Native provider 暴露的 descriptor 通常生成 DIRECT contract；当 native method 被投影为
property 时，operation 可能是 GET。virtual/interface 的 slot 会在 receiver prototype
上再次检查，因为同一 provider descriptor 可被多个 concrete prototype 使用。

### 3.2 Target kind

`SZrCallBindingTarget.targetKind` 分为 `VM`、`NATIVE`、`AOT`。VM 保存
`SZrFunction *`，native 保存 `FZrNativeFunction`，AOT 同时保存 entry thunk、method
info 和 reflection invoker。`targetGeneration` 必须与函数或 native closure 的
`callBindingGeneration` 相等；不相等时返回 `STALE_GENERATION` 并清空 target。

### 3.3 Operation

| operation | 用途 | 常见 lowering |
| --- | --- | --- |
| `CALL` | 调用 callable/function/method | `CALL_STATIC`、`CALL_MEMBER`、native dispatcher |
| `GET` | 读取 property 或成员 | getter descriptor、PIC/cache |
| `SET` | 写入 property 或成员 | setter descriptor、write barrier |
| `META` | 运算符、转换、constructor 等元方法 | `ZrCore_Value_CallMetaMethod` 或 AOT meta bridge |

## 4. 稳定 identity 如何计算

### 4.1 Native member identity

`native_binding_call_binding.c` 按 descriptor 数组顺序分配 member/signature RID：模块
级函数先占用序号，然后是 constants、module links、type、method，最后是每个 type 的
meta method。函数签名 hash 的输入包括 owner name、member name、return type、member
kind、声明参数数、最小参数数和每个参数的 type name；前缀固定为
`zr.md.native.sig.v1`。因此只改参数类型、最小 arity 或返回类型就会使合同失效。

### 4.2 Module signature hash

`ZrLibrary_NativeRegistry_ComputeModuleSignatureHash` 使用稳定 hash 前缀
`zr.native.contract.v1`，并按以下顺序吸收 descriptor 内容：

1. module name/version/public contract hash、ABI、minimum runtime ABI、capability、
   provider phase、provider contract role、`isContractOnly`。
2. 每个 function 的 name、min/max arity、return type、参数 name/type/passing mode、
   generic constraint、contract role、dispatch flags。
3. 每个 constant 的 name、type name、kind；每个 module link 的 name/target。
4. 每个 type 的 prototype kind、继承/实现、generic、protocol mask、construction flags、
   constructor、enum/FFI lowering 信息、field/method/meta/enum member 完整列表。
5. attribute role 和 canonical type role 的名称、ID、target/retention、parent、surface、
   projection 信息。

只要公开 ABI 或 descriptor 语义发生变化，hash 就变化。不要手工填写一个“看起来唯一”
的整数，也不要把指针地址纳入 hash。

## 5. Core resolver 的行为

### 5.1 合同检查

`ZrCore_CallBinding_CheckContract` 的顺序是：参数非空 → kind/hash → metadata token table
→ reserved/operation → owner/layout 配对 → virtual/interface slot。这样诊断能区分
`MISSING_CONTRACT`、`INVALID_TOKEN` 和 `INVALID_SLOT`，而不是统一返回 false。

### 5.2 Candidate resolve

`ZrCore_CallBinding_Resolve(expected, candidates, count, generation, binding, diagnostic)`
先清空旧 target，再按 `targetMetadataToken` 选择候选：

1. 没有候选返回 `TARGET_NOT_FOUND`。
2. 多于一个匹配返回 `AMBIGUOUS_TARGET`。
3. 找到一个后逐字段调用 `CompareContracts`，检查 module/signature/layout/kind/operation/slot。
4. 复制 candidate generation/target，最后调用 `Validate` 验证传入 generation。

`CompareContracts` 对 module hash 返回 `MODULE_MISMATCH`，对 signature token/hash 返回
`SIGNATURE_MISMATCH`，对 owner/layout 返回 `LAYOUT_MISMATCH`，对 kind/operation 返回
`TARGET_KIND_MISMATCH`。诊断结构还带 `targetMetadataToken`、instruction index、candidate
index、expected/actual 数值，宿主应记录这些字段。

### 5.3 Generation 和失效

`ZrCore_CallBinding_AdvanceGeneration` 遍历函数树，递增每个函数的
`callBindingGeneration` 并调用 `ZrCore_CallBinding_Invalidate` 清空 cache target。
native provider 重新注册时，registry 会把旧 closure 的 direct dispatch callback 置空；
下一次调用重新走 `ResolveCallBinding`。这保证“旧闭包仍被 GC root 保留”和“旧函数指针
不再执行”可以同时成立。

## 6. Native Registry API

### 6.1 attach、注册和查询

```c
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global);
ZR_LIBRARY_API void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_RegisterModule(
        SZrGlobalState *global, const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(
        SZrGlobalState *global, const TZrChar *moduleName);
```

`Attach` 安装 registry、module loader 和 provider-name resolver；通常在创建 global 后
调用一次。`RegisterModule` 会先验证 official identity、phase、ABI、capability、参数
passing mode 和 canonical role，再写入 module record。返回 descriptor 的指针由 registry
借用，provider 必须保证静态存储或在 registry 生命周期内有效。

canonical role 有三个查询入口：

```c
ZrLibrary_NativeRegistry_FindCanonicalTypeRole(global, role, &registered);
ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(global, "Object", &registered);
ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByProjection(
        global, providerRole, projectionKind, &registered);
```

`ZrLibRegisteredCanonicalTypeRole` 返回 provider 和 role descriptor；它是跨 module 的
协议 identity，不等同于某个 `SZrObjectPrototype *` 地址。

### 6.2 模块信息和错误

`GetModuleInfo`、`GetModuleInfoAt`、`GetModuleCount`、`GetModuleRefCount` 和
`GetModuleInfoBySourcePath` 用于诊断、LSP 索引和 reload UI。返回的
`ZrLibRegisteredModuleInfo` 包含 descriptor、moduleName、sourcePath、registrationKind、
`isDescriptorPlugin` 和 ownerRefCount。查询失败不一定修改错误码；加载/注册失败后应立刻
读取：

```c
EZrLibNativeRegistryErrorCode code =
        ZrLibrary_NativeRegistry_GetLastErrorCode(global);
const TZrChar *message =
        ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
fprintf(stderr, "native registry error %d: %s\n",
        (int)code, message != ZR_NULL ? message : "<none>");
```

错误码包括 LOAD、SYMBOL、ABI_MISMATCH、VERSION_MISMATCH、CAPABILITY_MISMATCH、
MODULE_NAME_MISMATCH、MODULE_IN_USE、PHASE_MISMATCH、RESERVED_OFFICIAL_MODULE、
DUPLICATE_OFFICIAL_PROVIDER、PROVIDER_CONTRACT_MISMATCH、INVALID_CANONICAL_TYPE_ROLE 和
DUPLICATE_PROVIDER_CONTRACT。错误消息是诊断文本，不应被脚本当作稳定枚举。

## 7. 从 descriptor 取得 contract 并 resolve

Native author 不应自己重算 RID/hash。使用 library helper：

```c
#include "zr_vm_library/native_binding_call_binding.h"
#include "zr_vm_library/native_registry.h"

TZrBool resolve_first_function(SZrState *state,
                               const ZrLibModuleDescriptor *module) {
    SZrCallBindingContract contract;
    SZrCallBindingTarget target;
    SZrCallBindingDiagnostic diagnostic;
    EZrCallBindingStatus status;

    if (state == ZR_NULL || module == ZR_NULL || module->functionCount == 0u) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_NativeCallBinding_GetDescriptorContract(
                module, ZR_NULL, ZR_NATIVE_CALL_BINDING_FUNCTION,
                &module->functions[0], &contract)) {
        return ZR_FALSE;
    }

    status = ZrLibrary_NativeRegistry_ResolveCallBinding(
            state, &contract, &target, &diagnostic);
    if (status != ZR_CALL_BINDING_OK) {
        fprintf(stderr, "resolve %s: %s (expected=%llu actual=%llu)\n",
                module->functions[0].name,
                ZrCore_CallBinding_StatusName(status),
                (unsigned long long)diagnostic.expected,
                (unsigned long long)diagnostic.actual);
        return ZR_FALSE;
    }
    /* target.native.function is owned by the registry; invoke through the
     * normal native dispatcher so ZrLibCallContext and GC roots are set up. */
    return target.targetKind == ZR_CALL_BINDING_TARGET_NATIVE &&
           target.native.function != ZR_NULL;
}
```

`ResolveCallBinding` 的实现先按 module signature hash 找 provider，并在需要时 materialize
模块；随后按 metadata/signature identity 查 binding entry，重新生成 descriptor contract
并调用 core compare。provider hash 相同但 identity 重复时返回
`AMBIGUOUS_TARGET`；identity 存在但 signature 不同则返回 `SIGNATURE_MISMATCH`。

`GetCallBindingIdentity(global, closure, &contract)` 反向从 native closure 读取当前
module/member identity，适用于 LSP hover、调试器和 typed callable 检查。闭包必须仍由
global registry 管理，调用方不能在返回 contract 后释放它。

## 8. Artifact 编码和 relocation

当前 wire 常量：

| 常量 | 值 | 作用 |
| --- | --- | --- |
| `ZR_CALL_BINDING_SCHEMA_VERSION` | `1` | 行版本 |
| `ZR_CALL_BINDING_SLOT_NONE` | `0xffffffff` | 无槽位/无索引哨兵 |
| `ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE` | `64` | 合同字段编码字节数 |
| `ZR_CALL_BINDING_LOCATION_ENCODED_SIZE` | `16` | location 编码字节数 |
| `ZR_CALL_BINDING_SECTION_MAGIC` | `0x444e4243` | CBND section magic |
| `ZR_CALL_BINDING_SECTION_ROW_SIZE` | `84` | contract + location + row header |

`SZrCallBindingLocation` 的四个字段是 `kind`、`targetIndex`、`ownerDepth`、`flags`。当前
relocation kind 为 NONE、CONSTANT、MODULE、AOT、VM_MODULE。artifact loader 还执行两条
结构约束：

* 非 typed-function binding 的 kind 必须在合法范围内，`targetIndex` 不能是
  `SLOT_NONE`，`ownerDepth` 不能是 `SLOT_NONE`。
* typed-function 必须使用 relocation NONE、`targetIndex == SLOT_NONE`、`ownerDepth == 0`；
  它的真实 callable 只能在运行时依据 value contract 选择。

编码示例：

```c
TZrByte encoded[ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE];
SZrCallBindingContract decoded;

if (!ZrCore_CallBinding_EncodeContract(
            &contract, encoded, sizeof(encoded))) {
    /* contract 未通过 CheckContract，不能写入 artifact。 */
}
if (!ZrCore_CallBinding_DecodeContract(
            encoded, sizeof(encoded), &decoded)) {
    /* schema/字段/token 不合法。 */
}
```

编码函数逐字段写入 little-endian 标量；不要 `memcpy` `SZrCallBinding`，因为其中有函数
指针、union、GC object 和平台相关填充。

## 9. Typed function 的特殊规则

typed function 没有静态 target metadata token，contract 仍必须有 signature token/hash
和 module hash。`ZrCore_CallBinding_PrepareTypedCall`（由 compiler/runtime 的 callable
路径调用）每次对当前 value 的 canonical signature 建立 witness；上一次 witness 不能
替代新 closure。若 callable 的参数 passing mode、返回类型或 generic specialization 改变，
应返回 `SIGNATURE_MISMATCH`，而不是尝试隐式转换。

## 10. 状态码和排障顺序

| 状态 | 典型原因 | 排查顺序 |
| --- | --- | --- |
| `MISSING_CONTRACT` | hash/layout/owner 配对缺失 | 打印 contract 全字段 |
| `INVALID_TOKEN` | token table/RID 为 0 或类型错误 | 检查 metadata writer |
| `TARGET_NOT_FOUND` | module 已找到但 entry 未注册 | 检查 materialize 和 descriptor 数组 |
| `AMBIGUOUS_TARGET` | 同 hash 或 token 有多个 provider | 检查重复注册/插件路径 |
| `SIGNATURE_MISMATCH` | 参数/返回/签名 hash 变化 | 重建 artifact 或同步 descriptor |
| `MODULE_MISMATCH` | provider version/公开 surface 变化 | 比较 module signature hash |
| `LAYOUT_MISMATCH` | receiver layout version/hash 变化 | 重新构建 prototype/AOT |
| `INVALID_SLOT` | virtual/interface slot 越界或不该存在 | 检查 owner prototype dispatch table |
| `STALE_GENERATION` | reload 后仍使用旧 target | 重新 link，勿复用旧 target 指针 |
| `TARGET_KIND_MISMATCH` | VM/native/AOT 或 operation 不一致 | 检查 lowering route |
| `INVALID_RELOCATION` | artifact location 与 binding kind 冲突 | 按 schema 重新写 section |

推荐先用 `ZrCore_CallBinding_StatusName` 取得稳定短名，再结合
`SZrCallBindingDiagnostic` 的 token/index/expected/actual 输出完整上下文。只有确认
contract、provider hash、layout 和 generation 全部一致后，才可以把错误归因于 callback
本身。
