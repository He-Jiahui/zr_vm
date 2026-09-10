---
related_code:
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍产物、语义接口和 C native 调用方案
  - docs/plans/aot/11-metadata.md
  - docs/module-system/zrm-assembly-container.md
tests:
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_typed_call_binding.c
  - tests/library/test_zrm_container.c
doc_type: api-reference
---

# Artifact Writer、Loader 与元数据投影 C API

本页说明 ZR 编译产物如何从已验证 `SZrFunction`/semantic facts 写入 `.zro`、`.zri`、`.zrs`、
scheduler artifact 和 AOT C/LLVM，并在 runtime 侧重新验证和加载。它面向 IDE、build tool、
AOT glue、package 生成器和高级嵌入式宿主；普通用户应通过 CLI/project 工作流，而不直接拼二进制
section。

## 1. writer 的输入不是源码文本

writer API 的输入通常是已完成 parser/semantic/compiler 流程的 `SZrFunction` 或 `SZrAstNode`：

```text
.zr source
  -> parser AST
  -> semantic/canonical type/CFG/SemIR
  -> SZrFunction + metadata + call bindings
  -> Writer_* output
  -> loader validates schema/hash/identity before materialization
```

因此不能把未初始化 function、部分失败 AST 或手工构造 instruction 数组交给 writer，并期望它
完成类型检查。source compile 的正确入口见 [Parser/Compiler 深度 API](parser-compiler-c-api.md)。

## 2. 输出种类与 API 选择

| 输出 | API | 输入 | 主要消费者 | 成功条件 |
| --- | --- | --- | --- | --- |
| `.zro` binary | `Writer_WriteBinaryFileWithOptions` / `WriteBinaryFile` | function、文件名、可选 module name/hash | VM loader | schema、module identity、binding/metadata 均可写 |
| scheduler `.zri/.zro` artifact | `Writer_WriteSchedulerArtifactFile` | function、artifact kind、diagnostic | scheduler/AOT/runtime | 返回 `EZrArtifactStatus` |
| `.zri` readable intermediate | `Writer_WriteIntermediateFile` | function、文件名 | compiler/LSP/debug | 成功写完整文本/中间投影 |
| `.zrs` syntax tree | `Writer_WriteSyntaxTreeFile` | AST、文件名 | IDE/诊断 | AST 仍归 caller，writer 不接管 |
| AOT C | `Writer_WriteAotCFileWithOptions` / `WriteAotCFile` | function、writer options | C compiler/AOT runtime | lowerable contract 与 option 要求一致 |
| AOT LLVM | `Writer_WriteAotLlvmFileWithOptions` / `WriteAotLlvmFile` | function、writer options | LLVM toolchain | 同上 |

`TZrBool` 返回的 writer 在失败后应由调用方保留相关 runtime/diagnostic 信息；`EZrArtifactStatus`
返回的 scheduler writer 有结构化 artifact diagnostic，必须检查 status，不能将非零 status
当作字节数或 bool。

## 3. `.zro` 与 scheduler artifact

### 3.1 binary writer options

```c
SZrBinaryWriterOptions options = {
    .moduleName = "app.main",
    .moduleHash = "<source-or-build-hash>"
};

if (!ZrParser_Writer_WriteBinaryFileWithOptions(
        state, function, "bin/app.main.zro", &options)) {
    return ZR_FALSE;
}
```

`moduleName` 与 `moduleHash` 是 identity/validation 输入，不是显示标签。空/错误 hash 可能使
incremental cache、loader 或 call-binding stale detection 失去依据。若 build tool 无法可靠计算
hash，应使用已定义的高层 build path，而不是随意填固定字符串。

### 3.2 scheduler contract artifact

```c
SZrArtifactDiagnostic diagnostic = {0};
EZrArtifactStatus status = ZrParser_Writer_WriteSchedulerArtifactFile(
        state, function, "obj/app.main.zri", ZR_ARTIFACT_KIND_INTERMEDIATE,
        &diagnostic);
if (status != ZR_ARTIFACT_STATUS_OK) {
    report_artifact_diagnostic(&diagnostic);
    return ZR_FALSE;
}
```

writer 会把 provider token、signature、layout 和 module identity 连接到 source fact，而不是沿用
旧式裸 TypeRef serialization。这一设计意味着 artifact 若缺失 provider/canonical fact，应被
拒绝而不是在 runtime 靠 display name 猜类型。

## 4. artifact projection API

`artifact_projection.h` 是“从 semantic/runtime facts 构造稳定 row”的 API，不直接写文件：

| API | 建立的事实 | 调用时机 |
| --- | --- | --- |
| `ArtifactCallBinding_BuildRows` | 每个 function 的 call-binding rows | function/code 生成完毕后 |
| `ArtifactLayout_ApplyNativeCapabilities` | native descriptor capability -> layout row | provider type 能力已验证后 |
| `ArtifactType_WriteSignature` | `TypeId` 的稳定 bytes signature | semantic context 可查询 canonical type 后 |
| `ArtifactType_InternSignature` | signature -> canonical `TypeId` | loader/semantic interning 阶段 |
| `ArtifactType_BuildPublicIdentity` | public type identity、token、layout/callable hash | API/export projection 阶段 |
| `ArtifactMetadata_BuildState` | preservation/member/property/meta state row | metadata retention 策略确定后 |

这些函数都返回 `EZrArtifactStatus` 并可填 `SZrArtifactDiagnostic`。调用方必须将 diagnostic
当成借用/调用期结果处理，按其 public schema 复制需要持久化的文本/row；不要把失败的
`outTypeId`、row count 或 identity 当作有效值继续写出。

## 5. AOT writer options 与保持规则

`SZrAotWriterOptions` 把源码/二进制身份、嵌入模块 blob、full-AOT 要求、symbol stripping、
fallback warning、preserve function/generic root/export declaration 汇总到写入合同：

| 选项组 | 关键字段 | 效果 |
| --- | --- | --- |
| identity | `moduleName`、`sourceHash`、`zroHash`、`inputKind`、`inputHash` | 将生成代码与输入 artifact 绑定 |
| embedding | `embeddedModuleBlob`、`embeddedModuleBlobLength` | 让 generated output 附带 module bytes；blob 必须在 writer 调用期有效 |
| strictness | `requireExecutableLowering`、`requireFullAot` | 不允许 runtime fallback/不完整 lowering 时 fail closed |
| stripping | `enableCodeStripping`、`stripGeneratedSymbols` | 只保留 reachability/preserve 合同允许的符号 |
| warnings | `suppressRuntimeFallbackWarnings`、reason mask、annotation suppression | 仅改变诊断可见性，不会增加可 AOT 的能力 |
| preserve | function flat indices、generic roots、export declarations | 防止反射/动态入口被错误裁剪 |

`requireFullAot` 与 `suppressRuntimeFallbackWarnings` 不是同一个开关。前者要求生成期拒绝不可
lower 的路径；后者只隐藏或标记诊断。生产构建不要用 suppress 来伪装 full-AOT 成功。

`Writer_ResolveTopLevelCallableFlatIndex` 可将可调用名称解析为 preserve 所需 flat index；失败时
不要猜 index 0 或按函数数组顺序替代，因为代码裁剪会改变物理布局。

## 6. native helper 的可序列化边界

```c
TZrUInt64 helperId =
        ZrParser_Writer_GetSerializableNativeHelperId(nativeHelper);
if (helperId == 0u) {
    return ZR_FALSE; /* 该函数没有稳定的 artifact helper identity。 */
}
```

原生函数指针本身不能写入 portable artifact；writer 只允许映射到稳定 helper ID 的已知 helper。
runtime 通过 `ZrCore_Io_GetSerializableNativeHelperFunction(helperId)` 恢复。第三方 native module 的
普通 callback 不因地址可见而自动可序列化：应通过 descriptor/module identity/call-binding contract
重新解析。

## 7. loader 端：先校验，后 materialize

runtime 入口组合 `ZrCore_Io_LoadSource`、`LoadEntryFunctionToRuntime`、`Io_ReadCallBindings`、
`ZrCore_Module_OpenCanonicalArtifact` 与 module loader。安全顺序：

```text
open bytes/IO
  -> validate artifact kind/schema/section boundaries
  -> validate module identity, provider phase, ABI and hashes
  -> intern canonical type signatures and metadata tokens
  -> materialize prototypes/functions/module exports
  -> resolve call bindings / relocation against current registry generation
  -> publish module as ready
```

artifact reader 应拒绝未知 schema、截断、过大 count、非法 token、trailing bytes、hash mismatch、
provider phase mismatch、layout/signature mismatch 和 stale call-binding generation。不得为了兼容
而跳过影响语义的 section row；能安全忽略的扩展必须由 schema 明确声明。

## 8. writer/loader 所有权

| 资源 | 谁拥有 | 何时失效 |
| --- | --- | --- |
| `SZrFunction *` 输入 | compiler/module graph | function/module/state 释放或 reload 后 |
| `SZrAstNode *` 输入 | parser caller | `Ast_Free` 后；writer 不接管 |
| `SZrAotWriterOptions` 指针及数组/blob | 调用方 | writer 返回后即可回收，除非调用方另行保存 |
| artifact diagnostic | 调用方传入/调用期填充 | 下次重用/释放/状态变化后不可借用旧字段 |
| `SZrIoSource` | Core IO API | `ReadSourceFree` 后 |
| loaded module/function | runtime/module cache | module reload、state/global teardown 或 generation 失效后 |

写文件建议采用“临时文件 -> fsync/close -> 原子 move”策略，并在失败时清除临时文件；不要让
下一次增量构建把半写入 `.zro/.zri` 当成有效 cache。Library file API 细节见
[项目、文件与 ZRM 包 API](project-file-zrm-api.md)。

## 9. build tool 配方

```c
/* 1. Compile source by the parser/compiler API. */
SZrFunction *function = ZrParser_Source_Compile(state, source, sourceLength, sourceName);
if (function == ZR_NULL) {
    return ZR_FALSE;
}

/* 2. Write canonical intermediate artifact for inspection/cache. */
if (!ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath)) {
    return ZR_FALSE;
}

/* 3. Write executable binary with the same canonical module identity. */
if (!ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath,
                                                &binaryOptions)) {
    return ZR_FALSE;
}

/* 4. Optionally produce AOT only after checking all required lowering facts. */
if (fullAot && !ZrParser_Writer_WriteAotCFileWithOptions(state, function,
                                                          cPath, &aotOptions)) {
    return ZR_FALSE;
}
```

示例省略 parser/compiler state 构造和 error reporting；真实工具还需让 `sourceName`、current
module key、project feature、provider registry 与 writer options 来自同一次解析/解析结果，避免
跨项目复用 function 或 hash。

## 10. 常见错误

| 错误 | 后果 | 正确做法 |
| --- | --- | --- |
| 将 native function 指针直接写进 artifact | 换进程/ASLR/插件 reload 后无效 | 使用 serializable helper ID 或 descriptor call binding |
| 忽略 `EZrArtifactStatus` | 部分 row/diagnostic 被当作成功 | 显式比较 OK 并报告 diagnostic |
| 用 display type name 替代 canonical signature | generic/owner/provider identity 丢失 | `ArtifactType_WriteSignature/InternSignature` |
| suppress warning 来通过 full AOT | runtime fallback 在生产才暴露 | `requireFullAot` fail closed |
| reload 后复用旧 function/token/call row | generation stale 或跳到旧 provider | 重新打开/resolve artifact 与 bindings |
| 将 `.zrm` entry bytes 作为永久 module buffer | Close/OpenBytes backing 后悬垂 | 按 ZRM ownership 复制或维持 archive owner |

## 11. 继续阅读

- [产物与格式](../06-reference/artifacts.md)
- [模块、项目与产物](../07-modules-projects-artifacts.md)
- [AOT ABI](aot-abi.md)
- [Native Registry 调用绑定](../03-modules/native-registry-call-binding.md)
- [Parser/Compiler 深度 API](parser-compiler-c-api.md)
