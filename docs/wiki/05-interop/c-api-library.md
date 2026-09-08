---
related_code:
  - zr_vm_library/include/zr_vm_library/common_state.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/include/zr_vm_library/task_runtime.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_module_specifier.c
  - tests/library/test_zrm_container.c
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_file_list.c
doc_type: api-reference
---

# Library C API

## Common state

`ZrLibrary_CommonState_CommonGlobalState_New(configPath)` 是宿主快速启动入口：读取 `.zrp`、
安装默认 allocator/source loader、创建 global 并 attach native registry。它不会自动注册所有
可选 provider；provider 仍需由 CLI/宿主按需调用各自的 `*_Register`。对应
`ZrLibrary_CommonState_CommonGlobalState_Free` 会关闭 global 和默认资源。需要自定义宿主时
直接调用 `ZrCore_GlobalState_New`，再注入 loader/registry。

## Project/module resolver

`ZrLibrary_Project_New/Free/GetFromGlobal` 管理 manifest。解析 API：
`ZrLibrary_ModuleSpecifier_Parse`、`ModuleIdentity_Equals`、`ModuleSpecifier_ResolveRelative`、
`Project_NormalizeModuleKey`、`DeriveCurrentModuleKey`、`ResolveImportModuleKey`、
`ResolveManifestAlias`、`ResolvePackageExport` 和 `ResolveImportProviderLocation`。
解析结果区分 official-native、registered-native、workspace、relative、alias、package、file
七种 kind；identity 不能由普通字符串拼接替代。source/binary/intermediate/assembly 路径
分别由 `Project_ResolveSourcePath/ResolveBinaryPath/ResolveIntermediatePath/ResolveAssemblyOutputPath`
产生。

本页的 `A/B/C` 写法仅用于把同一 API 家族排在一处，**不是 C 符号名**；调用时必须使用完整
符号。最小 manifest 生命周期如下：

```c
SZrLibrary_Project *project =
    ZrLibrary_Project_New(state, manifestText, manifestPath);
if (project == ZR_NULL) { return ZR_FALSE; }

/* Project_GetFromGlobal returns a borrowed const pointer. */
const SZrLibrary_Project *attached =
    ZrLibrary_Project_GetFromGlobal(state->global);

ZrLibrary_Project_Free(state, project);
```

路径解析函数写入调用方提供的 buffer，并以 `TZrBool` 报告成功；`ModuleSpecifier_Parse`、
`Project_DeriveCurrentModuleKey` 和 `Project_ResolveImportModuleKey` 还接收 error buffer，
宿主应在失败时保留其内容，而非再按字符串规则自行推断 identity。

## 文件和 `.zrm`

文件 API 先用 `ZrLibrary_File_NormalizePath/QueryInfo`，再选择
`OpenHandle/ReadHandle/WriteHandle/SeekHandle/FlushHandle`。目录枚举使用
`ListDirectory/Glob`，结果由 `ZrLibrary_File_List_Free` 释放。source loader 三件套为
`File_SourceLoadImplementation`、`SourceReadImplementation`、`SourceCloseImplementation`。

`.zrm` API `ZrLibrary_Zrm_WriteArchive/Open/OpenBytes/Close/FindModule/FindResource/ReadEntry`
支持 `zr.zrm/v1`；`OpenBytes` 的输入在 Close 前必须保持 immutable。entry 名称由
`BuildModuleEntryName/BuildResourceEntryName/BuildCompileToolExecutableEntryName` 构造，
逻辑名必须通过 `ValidateLogicalName`。

## Native registry

```c
ZrLibrary_NativeRegistry_Attach(global);
ZrLibrary_NativeRegistry_RegisterModule(global, descriptor);
const ZrLibModuleDescriptor *m =
    ZrLibrary_NativeRegistry_FindModule(global, "zr.container");
ZrLibrary_NativeRegistry_GetLastErrorCode(global);
ZrLibrary_NativeRegistry_Free(global);
```

注册器还提供 `FindModuleByProviderRole`、`FindCanonicalTypeRole*`、
`ValidateModuleDescriptor`、module info/count、`ResolveCallBinding` 和 phase admission。
Attach 会组合宿主已有 loader/resolver，Free 时恢复并调用 cleanup；descriptor plugin 的 source
失效使用 `InvalidateDescriptorPluginSource`。

## AOT/task bridge

`ZrLibrary_AotRuntime_ConfigureGlobal` 安装 AOT loader；`AotRuntime_ModuleLoader`、
`ExecuteEntry`、`BeginGeneratedFunction` 和 `ResolveGeneratedModuleContext` 管理 generated
module。任务桥接 API `TaskRuntime_PrepareJob/ExecutePreparedJob/CompletePreparedJob/`
`FaultPreparedJob/ReleasePreparedJob/RegisterAwaitHook/AwaitProviderTask` 负责消费 Job、root
Task、执行和 fault；所有 prepared handle 必须成对 release。
