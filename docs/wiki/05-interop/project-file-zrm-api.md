---
related_code:
  - zr_vm_library/include/zr_vm_library/common_state.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/io.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/src/zr_vm_library/zrm.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍内置库、项目和 C native 调用方案
  - docs/module-system/zrm-assembly-container.md
  - docs/library-and-builtins/index.md
tests:
  - tests/library/test_project_module_specifier.c
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_manifest_v2.c
  - tests/library/test_project_manifest_normalization.c
  - tests/library/test_zrm_container.c
  - tests/library/test_file_list.c
doc_type: api-reference
---

# 项目、文件与 ZRM 包 C API

本页是 C host 管理 `.zrp`、模块 specifier、文件系统和 `.zrm` assembly package 的正式参考。
它覆盖“如何解析/定位/读取”，不替代 native descriptor 注册；provider ABI 见
[Native Module 编写](native-module-authoring.md)，registry/cache generation 见
[Native Registry 调用绑定](../03-modules/native-registry-call-binding.md)。

## 1. 三层入口

| 入口 | 适用场景 | 自动完成的事 | 宿主仍负责的事 |
| --- | --- | --- | --- |
| `ZrLibrary_CommonState_CommonGlobalState_New(configFilePath)` | CLI、小型嵌入 | 默认 allocator、file source loader、global/project 基础路径 | 注册按构建启用的 provider、运行和错误呈现 |
| `ZrCore_GlobalState_New` + Library API | 自定义 allocator/loader/sandbox | 仅 Core global | 注入 loader、构造 project、attach registry |
| `ZrLibrary_Project_New(state, raw, file)` | 已经有 state，manifest 来自内存/服务 | 解析 project 和挂接 project model | 保证 raw/file 字符串、state 生命周期和 free 顺序 |

快速入口与自定义入口不能对同一 global 混合重复初始化。`CommonGlobalState_Free` 应与
`CommonGlobalState_New` 配对；手工创建的 global 则走 Core 的释放流程。

## 2. 模块身份与 specifier

### 2.1 解析结果不是字符串

`ZrLibrary_ModuleSpecifier_Parse` 生成 `SZrLibrary_ModuleSpecifier`：

```c
SZrLibrary_ModuleSpecifier specifier;
TZrChar error[256] = {0};

if (!ZrLibrary_ModuleSpecifier_Parse("@graphics/render", &specifier,
                                    error, sizeof(error))) {
    host_log("invalid module specifier", error);
    return ZR_FALSE;
}
```

解析结果含 `kind`、`identity`、`aliasRoot`、`locator` 和 `relativeParentLevels`。其中
`identity` 又由 module domain、规范 segments 和 package name 组成。公共枚举明确区分：

| `EZrLibrary_ModuleSpecifierKind` | 含义 | 下一步 |
| --- | --- | --- |
| `OFFICIAL_NATIVE` | 保留官方 `zr.*` provider | registry/inventory 解析 |
| `REGISTERED_NATIVE` | 已注册 native provider | registry descriptor 查询 |
| `WORKSPACE` | 当前项目 workspace module | 解析 source/binary/intermediate 路径 |
| `RELATIVE` | 相对于 current module | `ResolveRelative` 或 `ResolveImportModuleKey` |
| `ALIAS` | manifest alias | `ResolveManifestAlias` |
| `PACKAGE` | package export | `ResolvePackageExport` |
| `FILE` | 明确 file locator | provider location / file loader |

同名文本不保证同一身份：`zr.math`、workspace `math` 和 package `@math/...` 属于不同 domain。
调用方不得通过去掉前缀或自行替换 `/`、`.` 来合并它们；使用 `ModuleIdentity_Equals` 比较。

### 2.2 规范化与相对解析

| API | 输入 | 输出/失败 |
| --- | --- | --- |
| `Project_NormalizeModuleKey` | 原始 logical key | 写入调用方 buffer；失败表示格式或空间错误 |
| `Project_DeriveCurrentModuleKey` | project、source name、可选显式 key | 给当前编译单元建立 canonical key；附带 error buffer |
| `Project_ResolveImportModuleKey` | current key、raw import | 解析 relative/alias/package 语义；附带 error buffer |
| `ModuleSpecifier_ResolveRelative` | current identity、relative specifier | 返回 resolved identity；不读 filesystem |
| `ResolveManifestAlias` | project、alias specifier | 返回 manifest target specifier |
| `ResolvePackageExport` | project、package specifier | 返回 package export target |

所有 `buffer` 参数由调用方分配。API 不会 resize，也不返回可释放字符串；失败时不要使用部分
输出。建议每次调用前清零 error buffer，成功后也不要把 buffer 当成长期 identity store，真正
需要长期保存时让 project/semantic model 持有 managed `SZrString`。

## 3. `.zrp` project 生命周期

### 3.1 构造与释放

```c
SZrLibrary_Project *project =
        ZrLibrary_Project_New(state, manifestText, "E:/work/demo/demo.zrp");
if (project == ZR_NULL) {
    return ZR_FALSE;
}

/* project fields are inspection data; use resolver APIs for decisions. */
const SZrLibrary_Project *attached = ZrLibrary_Project_GetFromGlobal(state->global);

ZrLibrary_Project_Free(state, project);
```

`Project_New` 解析 manifest、依赖、alias、package export、feature switch、resource、AOT 和
preserve 元数据，并关联到 state/global 所属内存模型。`Project_GetFromGlobal` 返回 `const` 借用
指针，不能 free，也不能跨 global/project reload 保存。`Project_Free` 只释放该 project model；
调用前先确保没有执行、loader 或异步任务仍使用其路径、archive 或 dependency 指针。

### 3.2 manifest 可观察字段

`SZrLibrary_Project` 可公开查看但不应由 host 直接改写。重要字段包括：

| 分组 | 字段/概念 | 解释 |
| --- | --- | --- |
| identity | `name`、`assemblyName`、`version`、`packageIdentity` | package/provider identity 与版本选择基础 |
| layout | `file`、`directory`、`source`、`binary`、`entry`、`assemblyOutput` | project 根与标准产物定位 |
| dependencies | `manifestDependencies`、`manifestBuildDependencies`、`dependencyPackages` | runtime/build 依赖和已打开 ZRM package |
| routing | `pathAliases`、`manifestAliases`、`packageExports` | import specifier 重写规则 |
| build | `featureSwitches`、`aotMode`、`preserveRules`、`exportDeclarations` | compiler/AOT 保留和 feature contract |
| execution | `supportMultithread` | thread provider 与项目准入条件 |

用 `ProjectManifestV2_Write` 生成 schema 兼容 manifest 文本；依赖 lock 使用
`ProjectManifestV2_WriteDependencyLock` 和 `ReadDependencyLock`。lock entry 含 resolved version、
content hash、transitive identity、provider source kind/phase，不应仅凭 package 名绕过 hash 校验。

## 4. import provider 位置与 AOT 请求

### 4.1 provider location

```c
SZrLibrary_ProjectImportProviderLocation location;
TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
TZrChar error[512] = {0};

if (!ZrLibrary_Project_ResolveImportProviderLocation(
        project, "app.main", "@graphics/render", resolved, sizeof(resolved),
        &location, error, sizeof(error))) {
    host_log("provider resolution failed", error);
    return ZR_FALSE;
}
```

结果包含 artifact kind（project 或 ZRM）、provider phase、assembly/version range、archive/entry
借用引用、public contract hash 和 source/binary/intermediate path。host 必须先检查 phase、
contract hash 和 artifact kind，再把结果交给 loader；不能只优先一个非空 path。`archive` 和
`entry` 的寿命归 project/dependency package，`Project_Free` 后全部失效。

### 4.2 AOT provider 请求

`Project_ResolveImportProviderAotLoadRequest` 在普通 location 上增加 backend kind、descriptor
module name、library path 与 resolved module key。AOT loader 应验证：

1. backend 与请求的 `EZrAotBackendKind` 一致；
2. ZRM/项目 artifact 仍可用且 phase 可被当前 host 消费；
3. public contract hash、module identity 和依赖版本匹配；
4. dynamic library load 后 descriptor ABI 与 native registry 一致。

任一条件失败时，返回 error buffer 给用户或降级到明确允许的 source/runtime 路径；不要把
不匹配的 AOT library 当成普通 native module 直接执行。

## 5. 项目路径与执行 API

| API | 写入/返回内容 | 常见用途 |
| --- | --- | --- |
| `ResolveSourcePath` | `.zr` source location | source loader、IDE |
| `ResolveBinaryPath` | `.zro` location | binary-first runtime |
| `ResolveIntermediatePath` | `.zri` location | compiler/LSP/artifact inspection |
| `ResolveAssemblyOutputPath` | assembly output directory/path | pack/AOT output |
| `ResolveZrmModuleEntry` | 打开 archive 与模块 entry | package provider reader |
| `GetDependencyImportVersionRange` | assembly/version min/max | resolver diagnostic 与 provider gate |
| `Project_Run` | `EZrThreadStatus` 和 result value | 高层 project entry 执行 |
| `Project_Do` | project execution convenience path | CLI/embedded loop；不等于异常吞没 |

路径 API 不创建目录也不检查文件内容；得到的路径仍要通过 file API 或 loader 打开。`Project_Run`
可能执行代码并改变 state exception/status，因此 C host 应建立 `TryRun`/高层错误边界，结果 value
按 managed value 生命周期处理。

## 6. 文件系统 API

### 6.1 路径和元数据

| API | 契约 |
| --- | --- |
| `File_Exist` | 返回 not-exist/file/directory/other 枚举 |
| `File_IsAbsolutePath` | 仅判断 path 形式，不授予 sandbox 权限 |
| `File_NormalizePath` | 规范化到调用方 buffer；检查 buffer size |
| `File_GetDirectory` | 提取 parent path |
| `File_QueryInfo` | 填 `SZrLibrary_File_Info`：size、时间、existence、路径组成 |
| `File_PathJoin` | 拼接 path；调用方必须给足结果空间 |

`NormalizePath` 不是安全授权机制。若 host 有 workspace sandbox，应先规范化，再确认结果位于已
批准根目录，最后才调用 open/delete/move。不要把 user literal 直接传给递归 delete。

### 6.2 创建、移动和枚举

| API | 副作用 | 调用前必须明确 |
| --- | --- | --- |
| `CreateDirectorySingle` / `CreateDirectories` | 创建目录 | 目标根和权限 |
| `CreateEmpty(path, recursively)` | 创建空文件/父目录 | `recursively` 是否允许扩展范围 |
| `Delete(path, recursively)` | 删除文件或目录树 | 精确绝对/已验证 target，不能用未解析 glob |
| `Copy` / `Move` | 写/覆盖目标 | `overwrite` 策略及源/目标不同 |
| `ListDirectory` / `Glob` | 分配 list entries | 成功后必须 `File_List_Free` |

这些是 Library 的真实 filesystem 副作用 API。native module 若对最终用户暴露文件能力，还应
受 `zr.system.fs` 的 provider capability、resource lifecycle 和 project sandbox 策略约束。

### 6.3 stream handle

```c
SZrLibrary_File_StreamOpenResult stream;
TZrSize written = 0;

if (!ZrLibrary_File_OpenHandle(path, "wb", &stream)) {
    return ZR_FALSE;
}
if (!ZrLibrary_File_WriteHandle(stream.handle, bytes, byteCount, &written) ||
    written != byteCount ||
    !ZrLibrary_File_FlushHandle(stream.handle)) {
    ZrLibrary_File_CloseHandle(stream.handle);
    return ZR_FALSE;
}
ZrLibrary_File_CloseHandle(stream.handle);
```

`OpenHandle` 归一化 mode 并填充 read/write/append 标志；`CloseHandle` 必须与每次成功 open
配对。`ReadHandle`/`WriteHandle` 的实际字节数通过 out 参数返回，短读/短写不是自动成功。
`SeekHandle` 使用 `offset + origin`，`GetHandlePosition/Length` 与 `SetHandleLength` 都需要检查
布尔返回。不要把 `TZrLibrary_File_Handle` 当作跨进程或跨项目 reload 的稳定 ID。

`ReadAll` 和 `OpenRead/CloseRead` 使用 global 分配域；返回 native string/reader 的生命周期以
其 API 说明为准。自定义 source loader 应采用 `File_SourceLoadImplementation`、
`SourceReadImplementation`、`SourceCloseImplementation` 的同一 read/close 协议。

## 7. ZRM assembly package

### 7.1 格式不变式

| 常量 | 值/用途 |
| --- | --- |
| `ZR_LIBRARY_ZRM_FILE_EXTENSION` | `.zrm` |
| `ZR_LIBRARY_ZRM_FORMAT` | `zr.zrm/v1` |
| manifest entry | `META-INF/zrm.json` |
| module/resource/compile-tool prefix | `modules/`、`resources/`、`compile-tools/` |
| compression | `STORE` 或 `DEFLATE` |
| provider phase | Runtime、Test、CompileTool |

archive 的 assembly info 还包含 name/version/culture/public key token/kind/entry module/public
contract hash。解析器必须把 package metadata 当不可信输入：验证 logical name、entry count/size、
hash/CRC、phase 和 contract 后才 materialize module。

### 7.2 写包

```c
SZrLibrary_ZrmPackRequest request = {0};
TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH] = {0};

request.outputPath = "out/graphics.zrm";
request.assembly.name = "graphics";
request.assembly.version = "1.0.0";
request.assembly.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
request.modules = modules;
request.moduleCount = moduleCount;

if (!ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error))) {
    host_log("zrm write failed", error);
    return ZR_FALSE;
}
```

每个 `SZrLibrary_ZrmPackModule` 指定 module key、source path、hash，可选 compile-tool executable
source/hash；resource 指定 logical name、source path、hash 与 compress。调用 `WriteArchive` 前
用 `ZrLibrary_Zrm_ValidateLogicalName` 和 `Build*EntryName` 预检名称，避免在构建末尾才发现
冲突或路径逃逸。

### 7.3 读包与 bytes 所有权

```c
SZrLibrary_ZrmArchive archive = {0};
TZrByte *bytes = ZR_NULL;
TZrSize byteCount = 0;

if (!ZrLibrary_Zrm_Open("out/graphics.zrm", &archive, error, sizeof(error))) {
    return ZR_FALSE;
}
const SZrLibrary_ZrmEntryInfo *entry =
        ZrLibrary_Zrm_FindModule(&archive, "graphics/render");
if (entry != ZR_NULL &&
    ZrLibrary_Zrm_ReadEntry(&archive, entry->entryName, &bytes, &byteCount,
                            error, sizeof(error))) {
    consume_module_copy(bytes, byteCount);
    ZrLibrary_Zrm_FreeBytes(bytes);
}
ZrLibrary_Zrm_Close(&archive);
```

| API | 返回对象 | 释放者 |
| --- | --- | --- |
| `Zrm_Open` | archive 及其 entry metadata | `Zrm_Close` |
| `Zrm_OpenBytes` | archive，借用 caller bytes | caller 保持 bytes immutable/live 至 `Close` |
| `FindModule/Resource/CompileToolExecutable` | `const EntryInfo *` 借用 archive | 不单独 free |
| `ReadEntry` | 新分配的 `TZrByte *` copy | `Zrm_FreeBytes` |
| `Zrm_Close` | archive 的 zip/metadata | 调用一次，随后 entry 指针均失效 |

不要在 `Zrm_Close` 后读取 `entry->logicalName`，也不要让 package loader 把 `OpenBytes` 的临时
network buffer 交给异步模块解析后提前释放。

## 8. resolver 到 loader 的完整流程

```text
raw import literal
  -> ModuleSpecifier_Parse
  -> ResolveImportModuleKey(current module)
  -> ResolveImportProviderLocation / AotLoadRequest
  -> phase + version + public contract validation
  -> source / zro / zri / zrm entry / native descriptor loader
  -> module cache publication and call-binding link
```

这一顺序很关键。跳过 provider location 直接按字符串 open 文件，会绕过 alias、package export、
version lock、phase 和 public contract；跳过 registry 则会绕过 native ABI/provider role。

## 9. 常见失败与处理

| 症状 | 真实原因 | 行动 |
| --- | --- | --- |
| `Project_New` 返回 null | manifest schema、路径、dependency 或 metadata 不合法 | 记录 manifest/source path，使用对应 test fixture 缩小问题 |
| relative import 指向错误模块 | current module key 未规范化 | 先 `DeriveCurrentModuleKey`，再 resolve import |
| ZRM entry 找不到 | logical key 与 archive entry naming 不同 | 使用 `BuildModuleEntryName`/`FindModule`，不要手拼 ZIP 路径 |
| package 打开后崩溃 | close 后继续用 entry 指针，或 OpenBytes backing 已释放 | 保持 owner，到完整消费后再 close |
| file list 内存泄漏 | 忘记 `File_List_Free` | 将 list free 放在单一 cleanup 分支 |
| AOT library 载入但行为不对 | 忽略 phase/hash/module identity | 使用 Aot load request 与 registry validation 完整校验 |

## 10. 继续阅读

- [模块、项目与产物](../07-modules-projects-artifacts.md)
- [产物与格式](../06-reference/artifacts.md)
- [Parser/Compiler 深度 API](parser-compiler-c-api.md)
- [Native 插件加载与热替换](native-plugin-loading.md)
- [C 宿主集成指南](c-host-guide.md)
