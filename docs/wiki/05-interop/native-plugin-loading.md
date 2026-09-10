---
related_code:
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，补充 C native 库调用、内置库接口和实现机制
  - docs/library-and-builtins/index.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_native_registry_descriptor_invalidation.c
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
  - tests/language_server/descriptor_plugin_fixture_int.c
doc_type: interop-guide
---

# Native 插件加载与热替换

本页说明 descriptor plugin 从文件名到 ZR module 的完整生命周期。它针对需要把 C/C++
实现编译成动态库、再由 `import("...")` 自动发现的宿主；静态内置模块的 descriptor
结构见 [Native API](../03-modules/native-api.md)，调用绑定和 contract 行为见
[Native Registry 调用绑定](../03-modules/native-registry-call-binding.md)。

## 1. 两种供给方式

ZrVm 把 native provider 分成两类。两类最终都注册同一个
`ZrLibModuleDescriptor`，所以 ZR 代码不需要知道 provider 是静态还是动态：

| 方式 | 入口 | registry 记录 | 适用场景 |
| --- | --- | --- | --- |
| 内置（builtin） | 各库的 `*_GetModuleDescriptor` / `*_Register` | `ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_BUILTIN` | 随主程序或静态库发布，启动时确定 |
| descriptor plugin | 动态库导出 `ZrVm_GetNativeModule_v1()` | `ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN` | 项目、工具或 IDE 按需发现，支持源码替换 |

动态插件不是任意的 C ABI。它只能返回静态存储期的 descriptor；函数指针、类型字段、
参数 passing mode 和 public contract 都由 runtime 再次验证。插件不能直接写 VM 内部
stack、GC 链表或 module cache。

## 2. ABI 和入口约定

公共 ABI 常量位于 `zr_vm_common/zr_abi_conf.h`：

| 常量 | 当前值 | 检查位置 | 含义 |
| --- | ---: | --- | --- |
| `ZR_VM_NATIVE_PLUGIN_ABI_VERSION` | `6` | descriptor `abiVersion` | descriptor 结构及其字段语义 |
| `ZR_VM_NATIVE_RUNTIME_ABI_VERSION` | `4` | descriptor `minRuntimeAbi` | 插件要求的 runtime 能力下限 |

动态库必须导出名称完全匹配的 C 符号：

```c
#include "zr_vm_library/native_binding.h"

#if defined(__cplusplus)
extern "C" {
#endif
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#if defined(__cplusplus)
}
#endif
```

在 C++ 中缺少 `extern "C"` 会因为名称修饰导致 `SYMBOL` 错误。函数返回值必须在
调用期间保持有效；推荐返回文件级 `static const` 对象，不要返回栈上的临时结构。

## 3. 文件名和搜索顺序

runtime 根据请求的 module identity 生成文件名。算法先把每个非字母数字字符替换为
`_`，再添加前缀和平台扩展名：

```text
module:    zr.pluginprobe
sanitized: zr_pluginprobe
Windows:   zrvm_native_zr_pluginprobe.dll
macOS:     zrvm_native_zr_pluginprobe.dylib
Unix:      zrvm_native_zr_pluginprobe.so
```

普通 `import` 的搜索顺序是固定的，先命中的合法 descriptor 会停止搜索：

1. `global->userData` 指向的 `SZrLibrary_Project` 的 `<project>/native`。
2. 当前可执行文件所在目录的 `native` 子目录。
3. 环境变量 `ZR_VM_NATIVE_PATH` 中的目录列表。Windows 使用 `;` 分隔，其他平台使用
   `:` 分隔；空项不会成为有效候选。

候选路径必须是普通文件。runtime 不会把目录、源文件或任意未命名的动态库当作插件。
项目加载器 `ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin` 使用更窄的规则：
它只检查 `<project>/native/zrvm_native_<sanitized-module><ext>`，适合构建系统在已知
项目根目录时强制装载一个 provider。

## 4. 加载状态机

一次成功的加载可以表示为下面的状态转换：

```text
请求 import(module)
    -> registry lookup（builtin 或已有 plugin）
    -> 计算候选路径并检查文件
    -> 复制 shadow binary（若可用）
    -> LoadLibrary/dlopen
    -> 查找 ZrVm_GetNativeModule_v1
    -> 调用入口取得 descriptor
    -> ABI/name/version/capability/descriptor contract 校验
    -> 记录 plugin handle 与 sourcePath
    -> materialize module、type、constant、function、link metadata
    -> module cache 命中并建立 call binding
```

任何箭头失败都返回空 module，并把最后错误写入 registry；模块加载诊断还会附带
`loader=native-plugin`、请求 module 和失败阶段。宿主应在改变状态前读取：

```c
#include "zr_vm_library/native_registry.h"

if (!ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(
        state, projectDirectory, "zr.pluginprobe")) {
    EZrLibNativeRegistryErrorCode code =
        ZrLibrary_NativeRegistry_GetLastErrorCode(state->global);
    const TZrChar *message =
        ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global);
    report_plugin_failure(code, message);
    return ZR_FALSE;
}
```

`EnsureProjectDescriptorPlugin` 如果 registry 已经有同一 module、且记录的
`sourcePath` 与候选路径相等，会直接返回 true；如果同名的是 builtin 或另一来源，
不会静默覆盖它。

## 5. shadow cache 的原因和边界

Windows 不能在 DLL 仍被加载时覆盖原文件；Unix 的 inode 行为也会让“编辑原文件后重新
加载”产生平台差异。loader 因而尽量把候选 binary 复制到临时目录
`zr_vm_native_plugin_cache`，文件名形如：

```text
zrvm_shadow_zr_pluginprobe_<process-id>_<counter>.dll
zrvm_shadow_zr_pluginprobe_<process-id>_<counter>.so
```

真正传给 `LoadLibrary`/`dlopen` 的是 shadow path，但 registry 的 `sourcePath` 仍保存
用户提供的原始候选路径。关闭 handle 后会删除 shadow 文件并释放三份 registry 字符串。
如果临时目录不可创建或复制失败，loader 会退回直接加载 source path；这不是 ABI 变化，
只是热替换能力降低。部署脚本不应把 shadow 文件当成插件安装位置，也不应手工删除仍在
使用的 shadow 文件。

## 6. descriptor 校验层次

校验顺序有意从廉价条件到需要 registry 状态的条件排列：

| 层次 | 检查 | 失败码 |
| --- | --- | --- |
| 动态库 | 文件能否打开 | `ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD` |
| 符号 | 是否导出精确入口名 | `..._SYMBOL` |
| 返回值 | descriptor 非空 | `..._LOAD` |
| ABI | `abiVersion == 6` | `..._ABI_MISMATCH` |
| identity | `descriptor->moduleName` 等于请求名 | `..._MODULE_NAME_MISMATCH` |
| runtime 版本 | `minRuntimeAbi`（0 按当前 runtime ABI 处理）不高于 4 | `..._VERSION_MISMATCH` |
| capability | `requiredCapabilities` 只能使用 runtime 已提供的五类能力 | `..._CAPABILITY_MISMATCH` |
| descriptor shape | 数组指针/count、参数类型、passing mode、canonical role 合法 | `..._LOAD` 或 role 专用错误 |
| provider identity | 官方 role、public contract、重复 provider 约束 | `..._PROVIDER_CONTRACT_MISMATCH`、`..._DUPLICATE_*` |

当前 runtime capability 位包括 `TYPE_HINTS`、`TYPE_METADATA`、
`ENUM_INTERFACE_METADATA`、`SAFE_CALL_HELPERS` 和 `FFI_RUNTIME`。插件若声明未知位，
即使机器上有对应函数，也会 fail closed。`moduleVersion` 是说明和 artifact 版本，
不会替代 ABI 检查；需要破坏性变更时同时更新 `publicContractHash`。

## 7. 一个可工作的最小插件

下面的源码可以作为跨平台 descriptor 骨架。真正的业务 callback 需要使用
[Native API](../03-modules/native-api.md) 中的 root 和异常规则；这里故意把返回值设为
简单整数，便于先验证装载链路。

```c
#include "zr_vm_library/native_binding.h"

static TZrBool plugin_answer(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 0u, 0u)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, result, 42);
    return ZR_TRUE;
}

static const ZrLibFunctionDescriptor kFunctions[] = {
    {
        .name = "answer",
        .minArgumentCount = 0,
        .maxArgumentCount = 0,
        .callback = plugin_answer,
        .returnTypeName = "int",
        .documentation = "Returns the plugin answer.",
    },
};

static const ZrLibModuleDescriptor kModule = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
    .moduleName = "example.native",
    .functions = kFunctions,
    .functionCount = sizeof(kFunctions) / sizeof(kFunctions[0]),
    .documentation = "Example descriptor plugin.",
    .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
    .requiredCapabilities = ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS,
};

ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return &kModule;
}
```

若模块只提供 compile-time contract，可把 `isContractOnly` 设为 true 并提供对应 role；
这种 descriptor 可被 resolver 读取，但 `native_registry_materialize_module` 不会把它
作为普通 runtime module 导入。不要把 `callback` 指向卸载后仍会执行的代码，也不要在
descriptor 中保存 state、stack 或一次调用的 `ZrLibCallContext`。

## 8. provider phase

每个 descriptor 都有 `providerPhase`：`RUNTIME`、`TEST` 或 `COMPILE_TOOL`。宿主 state
的 phase 由 `ZrLibrary_State_SetProviderPhase` 设置；判断规则是：runtime provider 对任意
host 都可消费，非 runtime provider 只有在 phase 完全相等时可消费。

```c
ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
if (!ZrLibrary_ProviderPhase_CanConsume(
        ZrLibrary_State_GetProviderPhase(state), descriptor->providerPhase)) {
    /* Do not materialize a module from a different phase. */
}
```

phase 检查发生在 descriptor 找到之后、materialize 之前。这样可以让 Test provider 在
测试 runner 中工作，同时阻止它污染 production runtime module cache；CompileTool provider
也不会被普通 `import` 偷渡到运行时。官方模块的 phase/tier 清单见
[Provider 矩阵](../03-modules/provider-matrix.md)。

## 9. 失效、重载和 owner 引用

文件观察器或构建系统完成新 binary 后，可以调用：

```c
TZrBool ok = ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(
    global, changedSourcePath);
```

该操作先寻找 source path 相等的 descriptor plugin；如果没有精确命中但 registry 中存在
任何 descriptor plugin，当前实现仍会把请求视为插件失效请求。这是一个重要边界：调用方
应只在确认自己拥有插件 reload 事件时调用，不要把任意文件删除通知直接转发进去。

失效前会检查所有 descriptor-plugin record 的 `ownerRefCount`。只要有一个非零，就返回
`ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE`，保留原 module、handle 和 cache；调用方应
等待 module owner、AOT function、LSP snapshot 和 native closure 都释放后再重试。

检查通过后，当前实现会：

1. 移除每个 descriptor plugin module 的 module cache 条目。
2. 关闭所有 plugin handles，删除可删除的 shadow 文件。
3. 删除所有 descriptor-plugin module records（不是只删除精确 source 的一条）。
4. 清除 last error，下一次 import 重新走搜索和校验。

所以 reload 是 registry 级操作，不是单个函数指针替换。任何跨 reload 保存的
`SZrCallBinding`、module metadata 或 borrowed descriptor 都必须在 reload 前失效；新的
generation 会在下一次 link/materialize 时建立。详见 [值与 GC 生命周期](c-api-value-lifecycle.md)
和 [调用绑定](../03-modules/native-registry-call-binding.md)。

## 10. 错误处理清单

| 现象 | 首先读取 | 常见原因 | 修复方向 |
| --- | --- | --- | --- |
| `LOAD` | last error message | 文件不存在、依赖 DLL 缺失、权限不足 | 检查候选路径和动态库依赖 |
| `SYMBOL` | symbol error text | C++ 名称修饰、导出宏缺失、入口拼写错误 | 使用 `extern "C"` 和正确导出属性 |
| `ABI_MISMATCH` | descriptor ABI/current ABI | 用旧头文件编译插件 | 用同一 checkout 的 public headers 重编 |
| `MODULE_NAME_MISMATCH` | requested/exported names | 文件名与 descriptor identity 不一致 | 修正 descriptor 或 import 名称 |
| `VERSION_MISMATCH` | required/provided ABI | 使用了更新 runtime 才有的字段语义 | 降低要求或升级 runtime |
| `CAPABILITY_MISMATCH` | unsupported bit mask | 声明了 host 未实现的能力 | 删除未使用位或升级 host |
| `PHASE_MISMATCH` | required/actual phase | Test/CompileTool provider 进入 runtime | 设置正确 phase 或改依赖类型 |
| `MODULE_IN_USE` | module name/ref count | 仍有 live closure/AOT/LSP owner | 释放 owner 后再 invalidate |
| `DUPLICATE_*` | provider role/contract | 两个 provider 争用官方 identity | 保留一个 owner，其他改为 contract-only 或不同模块名 |

不要只记录布尔失败。至少记录错误码、message、候选 source path、请求 module、当前
provider phase 和 runtime/plugin ABI；这些字段足以把大多数部署问题定位到 loader、ABI、
contract 或生命周期层。

## 11. 发布检查表

发布一个 descriptor plugin 前逐项检查：

- 用目标平台和当前 `zr_abi_conf.h` 编译，确认导出符号没有名称修饰。
- descriptor 的 module name、文件名、moduleVersion 和 public contract hash 已锁定。
- 所有数组的 count 与指针一致；没有把临时字符串或栈地址放进 descriptor。
- callback 对 arity、类型、异常和临时 GC root 有明确处理。
- `providerPhase` 与 `.zrp/.zrm` 依赖声明一致，runtime 不依赖 Test/CompileTool provider。
- 在干净进程中验证项目目录、可执行目录和 `ZR_VM_NATIVE_PATH` 三条搜索路径。
- 运行 descriptor invalidation 测试，确认 live owner 会拒绝 reload，释放后才成功。
- 记录 plugin ABI、runtime ABI、public contract hash 和依赖动态库版本，便于回滚。

Native module 编写的字段级说明见 [Native Module 编写](native-module-authoring.md)；本页
描述的 loader 只负责发现和准入，不能替代 C callback 的值生命周期约束。
