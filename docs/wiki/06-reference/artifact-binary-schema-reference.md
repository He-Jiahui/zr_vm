---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_library/include/zr_vm_library/zrm.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_library/src/zr_vm_library/zrm.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/plans/aot/11-metadata.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/parser/test_call_binding_artifact.c
  - tests/library/test_zrm_container.c
  - tests/artifact/test_manifest_roundtrip.c
doc_type: reference
---

# Canonical Artifact 二进制 Schema 参考

`.zrs`、`.zri`、`.zro` 使用 `zr_vm_core/artifact_schema.h` 定义的 canonical artifact
容器；`.zrm` 是带 manifest/modules/resources 的 package 容器。两者有不同的 schema、
identity、读写 API 和所有权规则，不能把一个文件扩展名的字节直接当成另一个容器解析。

本页描述当前 artifact schema version 5 的公共模型。它是实现/工具参考，不是鼓励外部工具
手工拼接字节的许可：跨版本产物应由 `ZrCore_Artifact_*`、writer 和 loader API 读写。

## 1. 格式家族

| 产物 | `EZrArtifactKind` | 主要内容 | 常见消费者 |
| --- | --- | --- | --- |
| `.zrs` | `ZR_ARTIFACT_KIND_ZRS = 1` | syntax tree / source-oriented projection | IDE、诊断、工具 |
| `.zri` | `ZR_ARTIFACT_KIND_ZRI = 2` | intermediate canonical semantic/artifact facts | compiler、AOT、增量工具 |
| `.zro` | `ZR_ARTIFACT_KIND_ZRO = 3` | executable/code/relocation/call binding facts | VM、binary loader、AOT input |
| `.zrm` | package `zr.zrm/v1` | assembly manifest、modules、resources、compile tools | project resolver/library loader |
| TestManifest | 独立 typed contract | Test phase entry/case metadata | CLI runner、LSP/DAP |

artifact header 的 encoded size 为 112 bytes，section directory entry 为 24 bytes；最多 32
section、每个表最多 1,048,576 row、总字节上限为 67,108,864。signature 解析还限制最大深度
为 128、最大 child count 为 1,048,576。这些限制是输入防御边界，而非建议值：reader 必须
在分配或偏移计算之前执行它们。

## 2. 容器形状

```text
artifact bytes
  -> fixed header (kind, schema, public identity, section count ...)
  -> section directory (kind, flags, byte offset, byte length, count, element size)
  -> section payloads
```

header 中有 section count offset 常量 `ZR_ARTIFACT_HEADER_SECTION_COUNT_OFFSET = 16`，但外部
工具不应依赖 C struct packing 或直接把 native struct memcpy 到文件。encoded size 和 endianness
由 `ZrCore_Artifact_Write`/`Read` 管理。读者必须验证 magic、schema version、kind、directory
边界、section overlap、row count、element size、token、blob、signature 和 trailing bytes；
不能在遇到未知 mandatory section 时静默跳过。

`EZrArtifactStatus` 将失败分类为 `BAD_MAGIC`、`UNSUPPORTED_VERSION`、`INVALID_KIND`、
`TRUNCATED`、`COUNT_LIMIT`、`UNKNOWN_MANDATORY_SECTION`、`DUPLICATE_SECTION`、
`FORBIDDEN_SECTION`、`SECTION_OVERLAP`、`ILLEGAL_TOKEN`、`INVALID_SIGNATURE`、各种 hash/layout
不匹配以及 scheduler/transport mismatch。`SZrArtifactDiagnostic` 同时携带 section kind、row
index、byte offset、expected/actual token、version、hash，调用方应呈现这些结构化字段而非仅
输出一个“load failed”。

## 3. Public identity

每个 `SZrArtifactDocument` 和 `SZrArtifactView` 都有 `SZrArtifactPublicIdentity`：

| 字段 | 用于验证什么 |
| --- | --- |
| `canonicalTypeId` | canonical type graph 的本地 identity |
| `typeRefToken` / `typeSpecToken` / `signatureToken` | metadata token 的引用链 |
| `typeRefHash` / `typeSpecHash` / `signatureHash` | 类型/签名文本或编码对应的稳定 hash |
| `layoutVersion` / `layoutHash` | 值布局、GC scan、owner map 的兼容性 |
| `callableContractHash` | 参数、return、receiver/effect 的调用约定 |
| `moduleHash` | producer module identity |

loader 用 `ZrCore_Artifact_ValidatePublicIdentity` 比较 artifact 与当前 expected identity。它
不是“可选优化”：type signature、layout、callable contract、module 中任何一项失配，都可能
让旧 bytecode/AOT 照着错误 layout 或 native target 执行。应重新编译/重新 resolve，而不是
强制跳过验证。

## 4. Section 目录

当前 schema 定义以下 section kind：

| 组 | sections | 用途 |
| --- | --- | --- |
| 文本与类型 | string heap、type def、type ref、type spec、signature heap | 名称、canonical type identity、签名编码 |
| 成员与行为 | member def、property def、contract、code table | 可调用成员、property、code body/约定 |
| 装载与调试 | relocation binding、debug map、syntax tree、semantic IR | 目标重定位、源映射、工具投影 |
| 并发/跨域 | domain transfer、scheduler contract | transfer kind、schema、policy、Send/Sync requirements |
| metadata | metadata state、metadata record、metadata blob、layout map heap | reflection 保留和布局投影 |
| 绑定 | call binding table | serialized `SZrCallBindingContract`/location rows |

section flags 目前只有 `MANDATORY`（0）和 `OPTIONAL`（bit 0），未知 flag 会被拒绝。每种
table row 也有固定 encoded size，例如 TypeDef 48、MemberDef 40、PropertyDef 48、Contract
40、Layout 48、Relocation 40、CallBinding 96、DomainTransfer 48、SchedulerContract 48、
MetadataState 64、MetadataRecord 40 bytes。固定大小用于安全地计算 `rowIndex * elementSize`，
不表示 C declaration 的 native `sizeof()` 必然相等。

## 5. 关键 row 契约

### 5.1 类型、成员与 layout

`SZrArtifactTypeDefRow` 连接 type token、canonical type id、constructor token/signature 和
hash。`SZrArtifactMemberDefRow` 连接 owner type、signature、name offset、member contract；
`SZrArtifactPropertyDefRow` 额外记录 getter/setter/initializer token 和 property flags。

`SZrArtifactLayoutRow` 有 type token、layout version、byte size、alignment、GC scan kind、
ownership map offset/length、layout hash、stable slot contract hash。type flag 可表示 value、
GC、resource、readonly、ref-like、drop、value-constructible、interface、abstract、enum；
它们是后端交接的摘要，仍需要当前 runtime 的 type layout registry 认可。

### 5.2 Call contract 与 FFI lowering

`SZrArtifactContractRow` 记录 member token、signature token、parameter count、receiver effect、
ref export effect、escape flags、ABI lowering kind、contract hash。ABI lowering 值为：

| 值 | 含义 |
| --- | --- |
| `NONE` | 没有特定 ABI lowering |
| `ZR_VALUE_FRAME` | 通过 ZR value frame 调用 |
| `NATIVE_MARSHALLED` | 经过 native marshalling |
| `NATIVE_DIRECT` | 已验证的 direct native path |

artifact 的 passing form 是 value/in/ref/ref-readonly/out；call site marker 是 none/ref/out。
它们必须和 source semantic facts、native descriptor、FFI contract 的 callable hash 一致。
一个 binary 里存在 `NATIVE_DIRECT` 标志并不允许 loader 跳过 ABI、layout、provider generation
检查。

### 5.3 Scheduler 与 domain transfer

domain-transfer table 区分 forbidden、value copy、structured clone、immutable handle、resource
move；`DROP_ON_FAILURE` flag 控制失败时的 cleanup 协议。scheduler contract table 保存 scheduler/
task/job token、ABI version、attached/isolated policy mask、Send/Sync requirement flags、transport
contract hash、scheduler contract hash。这些 row 让 artifact 可以拒绝在不匹配的 task provider
或 domain transport 上运行，而不是运行到半途发现一个未定义的跨线程对象。

### 5.4 Metadata 与 call binding

metadata state 记录 identity-only/members/full preservation、reflection category、generation、
retained members/properties/records 与 hashes；metadata record 记录 attribute/user/source/flag
payload、retention（runtime/test/compile-tool）及 record hash。AOT stripping 或 compile-tool
projection 必须尊重这些事实，不能把“当前不会用”的 metadata 悄悄当作可删除。

call binding table 把 schema version、function index、cache index、instruction index 与
`SZrCallBindingContract`/location 编码为 96 byte row。读取后仍需通过 native registry resolve
当前 target；缓存命中只是一项优化，provider contract/hash/generation 变化时必须重新绑定。

## 6. C 写入与读取

```c
SZrArtifactDiagnostic diagnostic = {0};
TZrSize required = 0U;

if (ZrCore_Artifact_GetEncodedSize(&document, &required, &diagnostic) !=
    ZR_ARTIFACT_STATUS_OK) {
    /* diagnostic identifies the rejected document row/section. */
}

/* Allocate `required` bytes with the caller's chosen ownership policy. */
if (ZrCore_Artifact_Write(&document, bytes, required, &written, &diagnostic) !=
    ZR_ARTIFACT_STATUS_OK) {
    /* Do not publish a partial output as a cache hit. */
}

if (ZrCore_Artifact_Read(bytes, written, &view, &diagnostic) ==
        ZR_ARTIFACT_STATUS_OK &&
    ZrCore_Artifact_ValidatePublicIdentity(&view, &expectedIdentity, &diagnostic) ==
        ZR_ARTIFACT_STATUS_OK) {
    /* Find sections, then use the typed row readers. */
}
```

用 `ZrCore_Artifact_FindSection` 找到 section 后，调用相应的 `ReadTypeDefRow`、
`ReadMemberDefRow`、`ReadContractRow`、`ReadLayoutRow`、`ReadMetadata*Row`、
`ReadDomainTransferRow`、`ReadSchedulerContractRow`、`ReadRelocationRow` 或
`ReadCallBindingRow`。不要将 section `data` 直接强制转换为 native row struct 并解引用；
typed reader 才会执行 row size、endian、bounds 和 field 验证。

`ZrCore_Artifact_HashBytes` 可为已验证字节求稳定 hash；它不能证明 bytes 来自可信 producer。
在收到网络、缓存或 `.zrm` 输入时，先 `Read`/validate，再把 hash 用作 cache key。

## 7. Writer、AOT 与 text projection

writer 入口分工如下：

| API | 输出 |
| --- | --- |
| `ZrParser_Writer_WriteBinaryFile*` | legacy/general binary writer path |
| `ZrParser_Writer_WriteSchedulerArtifactFile` | canonical `.zri/.zro` scheduler contract artifact |
| `ZrParser_Writer_WriteIntermediateFile` | 可读 intermediate projection |
| `ZrParser_Writer_WriteSyntaxTreeFile` | 可读 `.zrs` syntax projection |
| `ZrParser_Writer_WriteAotCFile*` | AOT C 和 registration metadata |
| `ZrParser_Writer_WriteAotLlvmFile*` | AOT LLVM IR |

`SZrAotWriterOptions` 还带 module/source/zro/input hash、embedded module blob、full-AOT/stripping
要求、fallback warning policy、manifest preserve roots 与 export declarations。AOT 输出的
“能生成”不表示它可在另一个 runtime ABI 或 provider inventory 上加载；详见
[ABI 与兼容性参考](abi-compatibility-reference.md)。

## 8. `.zrm` package 边界

`.zrm` 格式名为 `zr.zrm/v1`，manifest entry 为 `META-INF/zrm.json`；module 位于 `modules/`，
resource 位于 `resources/`，compile-tool executable 位于 `compile-tools/`。assembly 信息包括
name/version/culture/public key token/kind/entry module/provider phase/public contract hash。entry
可为 STORE 或 DEFLATE，archive entry metadata 记录 logical/entry name、hash、压缩前后 size、
CRC32 与 compression。

`ZrLibrary_Zrm_OpenBytes` 借用传入 bytes，调用方必须让其在 `ZrLibrary_Zrm_Close` 前保持
不可变且地址有效；`ReadEntry` 返回副本，随后调用 `ZrLibrary_Zrm_FreeBytes`。这两个 API 的
所有权不同，错误地提前释放 OpenBytes input 或用错释放函数会让 resolver 读取悬垂内存。

## 9. 实务检查表

1. 写入前从 compiler/semantic facts 建 `SZrArtifactDocument`，不要手填 hash/token。
2. 写入后只在 `Write` 成功且完整写入时原子发布到 cache/output。
3. 读取时先验证容器，再验证 public identity、section 语义、native/provider contract。
4. unknown mandatory section、schema/ABI/layout/contract mismatch 都应停止加载并触发重新编译。
5. `.zrm` entry 路径必须经 `ZrLibrary_Zrm_ValidateLogicalName`/builder API 生成，不能接受路径
   traversal 或任意 zip entry name。

文件后缀、项目 manifest 和 archive resolver 的更高层行为见 [产物与格式](artifacts.md) 与
[项目、文件与 ZRM API](../05-interop/project-file-zrm-api.md)。
