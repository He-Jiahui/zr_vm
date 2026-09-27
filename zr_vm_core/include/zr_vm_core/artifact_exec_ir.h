#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_H

#include "zr_vm_core/conf.h"

/* A small, pointer-free on-disk contract for ExecIR/ExecBC artifacts.  This
 * contract is intentionally separate from runtime structs: every field is
 * encoded little-endian and all references are IDs or byte offsets. */
#define ZR_ARTIFACT_EXEC_IR_MAGIC ((TZrUInt32)0x31495245u) /* "ERI1" */
#define ZR_ARTIFACT_EXEC_IR_SCHEMA_VERSION ((TZrUInt16)1u)
#define ZR_ARTIFACT_EXEC_IR_HEADER_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_EXEC_IR_SECTION_SIZE ((TZrUInt32)24u)
#define ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE ((TZrUInt32)32u)
#define ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS ((TZrUInt32)32u)
#define ZR_ARTIFACT_EXEC_IR_MAX_BYTES ((TZrUInt32)67108864u)

/** @brief ExecIR 产物中允许的五类节；目录中的种类值不指向进程内对象。 */
typedef enum EZrArtifactExecIrSectionKind {
    ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR = 1,
    ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC = 2,
    ZR_ARTIFACT_EXEC_IR_SECTION_BINDINGS = 3,
    ZR_ARTIFACT_EXEC_IR_SECTION_STATE_MAPS = 4,
    ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS = 5
} EZrArtifactExecIrSectionKind;

/* 未知 flag 会被写端和读端拒绝；此位只表示当前节可选。 */
#define ZR_ARTIFACT_EXEC_IR_SECTION_OPTIONAL ((TZrUInt32)1u)

/** @brief 编码、读取和 relocation 解析共用的结果分类。 */
typedef enum EZrArtifactExecIrStatus {
    ZR_ARTIFACT_EXEC_IR_OK = 0,
    ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,
    ZR_ARTIFACT_EXEC_IR_BAD_MAGIC,
    ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,
    ZR_ARTIFACT_EXEC_IR_TRUNCATED,
    ZR_ARTIFACT_EXEC_IR_LIMIT,
    ZR_ARTIFACT_EXEC_IR_INVALID_DIRECTORY,
    ZR_ARTIFACT_EXEC_IR_OVERLAP,
    ZR_ARTIFACT_EXEC_IR_DUPLICATE_SECTION,
    ZR_ARTIFACT_EXEC_IR_UNKNOWN_SECTION,
    ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
    ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION,
    ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH,
    ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED
} EZrArtifactExecIrStatus;

/** @brief 可选的失败位置；status、节、行和字节偏移由各入口写入。
 * BUG: relocation 回调拒绝时，通用失败函数会覆盖刚记录的目标 token。 */
typedef struct SZrArtifactExecIrDiagnostic {
    EZrArtifactExecIrStatus status;
    TZrUInt32 byteOffset;
    TZrUInt32 sectionKind;
    TZrUInt32 rowIndex;
    TZrUInt32 token;
} SZrArtifactExecIrDiagnostic;

/** @brief 写端借用的节载荷；byteLength>0 须有 data，count>0 须有 size>0，size>0 时 count*size 须等于 byteLength。 */
typedef struct SZrArtifactExecIrSectionInput {
    EZrArtifactExecIrSectionKind kind;
    TZrUInt32 flags;
    TZrUInt32 elementCount;
    TZrUInt32 elementSize;
    const TZrByte *data;
    TZrUInt32 byteLength;
} SZrArtifactExecIrSectionInput;

/** @brief 写入前组装的线格式文档；sections 及其 data 由调用方持有。 */
typedef struct SZrArtifactExecIrDocument {
    TZrUInt16 abiVersion;
    TZrUInt16 flags;
    TZrUInt64 moduleHash;
    TZrUInt64 execIrHash;
    TZrUInt64 execBcHash;
    TZrUInt32 sectionCount;
    const SZrArtifactExecIrSectionInput *sections;
} SZrArtifactExecIrDocument;

/** @brief 从目录解码的节窗口；data 借用原始 buffer，byteOffset 相对产物起点。 */
typedef struct SZrArtifactExecIrSectionView {
    EZrArtifactExecIrSectionKind kind;
    TZrUInt32 flags;
    TZrUInt32 elementCount;
    TZrUInt32 elementSize;
    TZrUInt32 byteOffset;
    TZrUInt32 byteLength;
    const TZrByte *data;
} SZrArtifactExecIrSectionView;

/** @brief 读取成功后的完整视图；buffer 须在全部节访问期间保持有效。
 * BUG: 现行模块契约要求读取失败时清零视图，但后续目录或哈希失败会保留部分字段。 */
typedef struct SZrArtifactExecIrView {
    TZrUInt16 abiVersion;
    TZrUInt16 flags;
    TZrUInt64 moduleHash;
    TZrUInt64 execIrHash;
    TZrUInt64 execBcHash;
    TZrUInt32 sectionCount;
    const TZrByte *buffer;
    TZrUInt32 bufferLength;
    SZrArtifactExecIrSectionView sections[ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS];
} SZrArtifactExecIrView;

/** @brief relocation 的宿主侧字段描述；固定 32 字节线格式不能直接 memcpy 此 C 结构。 */
typedef struct SZrArtifactExecIrRelocation {
    TZrUInt32 targetToken;
    TZrUInt32 targetKind;
    TZrUInt32 codeOffset;
    TZrUInt32 targetIndex;
    TZrUInt64 expectedHash;
    TZrUInt64 expectedContractHash;
} SZrArtifactExecIrRelocation;

/** @brief 把稳定目标身份解析为进程内索引；返回真才发布 outTargetIndex。
 * @note userData 仅借用；ValidateRelocations 在全部回调成功后才提交结果数组。 */
typedef TZrBool (*FZrArtifactExecIrResolve)(TZrUInt32 targetToken,
                                             TZrUInt32 targetKind,
                                             TZrUInt64 expectedHash,
                                             TZrUInt64 expectedContractHash,
                                             TZrUInt32 *outTargetIndex,
                                             TZrPtr userData);

/** @brief 对借用的线格式字节计算稳定哈希；空指针配正长度返回零。 */
ZR_CORE_API TZrUInt64 ZrCore_ArtifactExecIr_HashBytes(const TZrByte *bytes,
                                                       TZrUInt32 length);
/** @brief 校验节目录并计算完整编码长度；成功才写 outSize。 */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_GetEncodedSize(
        const SZrArtifactExecIrDocument *document, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic);
/** @brief 预检容量与非零期望哈希，再把借用节编码到调用方缓冲区。
 * @note 失败保留 outWritten 原值；TODO: 核定源节 data 与目标 buffer 的重叠契约。 */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Write(
        const SZrArtifactExecIrDocument *document, TZrByte *buffer,
        TZrUInt32 capacity, TZrUInt32 *outWritten,
        SZrArtifactExecIrDiagnostic *diagnostic);
/** @brief 校验有界字节流并发布借用原始 buffer 的视图；只在 OK 后消费 outView。
 * BUG: 非零 count、零槽宽的节会绕过长度校验；后续失败也可能留下部分视图。 */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Read(
        const TZrByte *buffer, TZrUInt32 length, SZrArtifactExecIrView *outView,
        SZrArtifactExecIrDiagnostic *diagnostic);
/** @brief 从已验证视图查节；找到时返回随 view 和底层 buffer 失效的借用指针。
 * @note 有效入参下缺失节时把 outSection 置空并返回 INVALID_SECTION。 */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_FindSection(
        const SZrArtifactExecIrView *view, EZrArtifactExecIrSectionKind kind,
        const SZrArtifactExecIrSectionView **outSection,
        SZrArtifactExecIrDiagnostic *diagnostic);
/** @brief 调用 resolver 映射全部 relocation，成功后一次性写 resolvedTargetIndices。
 * BUG: codeOffset 只与整包长度比较，越过 ExecIR 节仍可通过；失败诊断丢失目标 token。 */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_ValidateRelocations(
        const SZrArtifactExecIrView *view, FZrArtifactExecIrResolve resolver,
        TZrUInt32 *resolvedTargetIndices, TZrUInt32 capacity, TZrPtr userData,
        SZrArtifactExecIrDiagnostic *diagnostic);
/** @brief 返回静态状态名；未单列的状态统一显示 invalid-artifact。 */
ZR_CORE_API const TZrChar *ZrCore_ArtifactExecIr_StatusName(
        EZrArtifactExecIrStatus status);

#endif
