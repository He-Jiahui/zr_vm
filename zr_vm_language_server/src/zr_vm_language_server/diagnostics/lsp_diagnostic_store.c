#include "zr_vm_language_server/lsp_diagnostic_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/lsp_semantic_snapshot.h"

#define ZR_LSP_DIAGNOSTIC_HASH_OFFSET ((TZrUInt64)14695981039346656037ULL)
#define ZR_LSP_DIAGNOSTIC_HASH_PRIME ((TZrUInt64)1099511628211ULL)

/** 将固定宽度标量并入诊断指纹，供两个传输入口比较 pull 结果身份。 */
static void diagnostic_hash_word(TZrUInt64 *hash, TZrUInt64 value) {
    for (TZrUInt32 shift = 0U; shift < 64U; shift += 8U) {
        *hash ^= (value >> shift) & 0xffU;
        *hash *= ZR_LSP_DIAGNOSTIC_HASH_PRIME;
    }
}

/** 可空字符串的长度与字节内容共同参与身份，避免文本变化仍复用旧结果。 */
static void diagnostic_hash_string(TZrUInt64 *hash, const SZrString *value) {
    const TZrChar *text = value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
    TZrSize length = value != ZR_NULL ? ZrCore_String_GetByteLength(value) : 0U;

    diagnostic_hash_word(hash, (TZrUInt64)length);
    for (TZrSize index = 0U; text != ZR_NULL && index < length; index++) {
        *hash ^= (TZrUInt8)text[index];
        *hash *= ZR_LSP_DIAGNOSTIC_HASH_PRIME;
    }
}

/** 使用 LSP 范围坐标计算身份；位置单位由上游诊断转换契约决定。 */
static void diagnostic_hash_range(TZrUInt64 *hash, SZrLspRange range) {
    diagnostic_hash_word(hash, (TZrUInt64)(TZrUInt32)range.start.line);
    diagnostic_hash_word(hash, (TZrUInt64)(TZrUInt32)range.start.character);
    diagnostic_hash_word(hash, (TZrUInt64)(TZrUInt32)range.end.line);
    diagnostic_hash_word(hash, (TZrUInt64)(TZrUInt32)range.end.character);
}

/** 子项只在哈希计算期间借用，不转移相关信息或修复建议的所有权。 */
typedef TZrUInt64 (*FZrLspDiagnosticChildHash)(const void *value);

/** 相关信息的 URI、范围和消息变化均会改变诊断结果身份。 */
static TZrUInt64 diagnostic_related_information_hash(const void *value) {
    const SZrLspDiagnosticRelatedInformation *relatedInformation =
            (const SZrLspDiagnosticRelatedInformation *)value;
    TZrUInt64 hash = ZR_LSP_DIAGNOSTIC_HASH_OFFSET;

    if (relatedInformation == ZR_NULL) {
        return 0U;
    }
    diagnostic_hash_string(&hash, relatedInformation->location.uri);
    diagnostic_hash_range(&hash, relatedInformation->location.range);
    diagnostic_hash_string(&hash, relatedInformation->message);
    return hash;
}

/** 修复建议变化也应失效 pull 结果，避免客户端沿用旧编辑。 */
static TZrUInt64 diagnostic_fix_hash(const void *value) {
    const SZrLspDiagnosticFix *fix = (const SZrLspDiagnosticFix *)value;
    TZrUInt64 hash = ZR_LSP_DIAGNOSTIC_HASH_OFFSET;

    if (fix == ZR_NULL) {
        return 0U;
    }
    diagnostic_hash_string(&hash, fix->title);
    diagnostic_hash_range(&hash, fix->editRange);
    diagnostic_hash_string(&hash, fix->editText);
    diagnostic_hash_word(&hash, (TZrUInt64)(TZrUInt32)fix->applicability);
    return hash;
}

/** 对临时指纹排序，使列表排列顺序不影响内容身份。 */
static int diagnostic_hash_compare(const void *left, const void *right) {
    TZrUInt64 leftHash = *(const TZrUInt64 *)left;
    TZrUInt64 rightHash = *(const TZrUInt64 *)right;
    return leftHash < rightHash ? -1 : (leftHash > rightHash ? 1 : 0);
}

/** 子项按顺序无关的列表计入父诊断，重复项仍保留；临时数组由本函数释放，分配失败向上报告。 */
static TZrBool diagnostic_hash_sorted_children(SZrState *state,
                                               const SZrArray *values,
                                               TZrSize expectedElementSize,
                                               FZrLspDiagnosticChildHash childHash,
                                               TZrUInt64 *hash) {
    TZrUInt64 *hashes = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL || hash == ZR_NULL || childHash == ZR_NULL ||
        values == ZR_NULL || (!values->isValid && values->length != 0U) ||
        (values->isValid && values->elementSize != expectedElementSize)) {
        return ZR_FALSE;
    }
    diagnostic_hash_word(hash, (TZrUInt64)values->length);
    if (values->length == 0U) {
        return ZR_TRUE;
    }

    hashes = (TZrUInt64 *)ZrCore_Memory_RawMalloc(
            state->global, values->length * sizeof(TZrUInt64));
    if (hashes == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < values->length; index++) {
        hashes[index] = childHash(ZrCore_Array_Get((SZrArray *)values, index));
    }
    qsort(hashes, (size_t)values->length, sizeof(*hashes), diagnostic_hash_compare);
    for (TZrSize index = 0U; index < values->length; index++) {
        diagnostic_hash_word(hash, hashes[index]);
    }
    ZrCore_Memory_RawFree(state->global, hashes, values->length * sizeof(TZrUInt64));
    return ZR_TRUE;
}

/** 汇总单条诊断的可见字段及顺序无关的子项，供 stdio/WASM 使用同一身份算法。 */
static TZrBool diagnostic_payload_hash(SZrState *state,
                                       const SZrLspDiagnostic *diagnostic,
                                       TZrUInt64 *outHash) {
    TZrUInt64 hash = ZR_LSP_DIAGNOSTIC_HASH_OFFSET;

    if (outHash == ZR_NULL) {
        return ZR_FALSE;
    }
    if (diagnostic == ZR_NULL) {
        *outHash = 0U;
        return ZR_TRUE;
    }
    diagnostic_hash_range(&hash, diagnostic->range);
    diagnostic_hash_word(&hash, (TZrUInt64)(TZrUInt32)diagnostic->severity);
    diagnostic_hash_word(&hash, (TZrUInt64)diagnostic->descriptorId);
    diagnostic_hash_word(&hash, (TZrUInt64)(TZrUInt32)diagnostic->noFixReason);
    diagnostic_hash_string(&hash, diagnostic->code);
    diagnostic_hash_string(&hash, diagnostic->codeDescriptionHref);
    diagnostic_hash_string(&hash, diagnostic->message);
    if (!diagnostic_hash_sorted_children(state,
                                         &diagnostic->relatedInformation,
                                         sizeof(SZrLspDiagnosticRelatedInformation),
                                         diagnostic_related_information_hash,
                                         &hash) ||
        !diagnostic_hash_sorted_children(state,
                                         &diagnostic->fixes,
                                         sizeof(SZrLspDiagnosticFix),
                                         diagnostic_fix_hash,
                                         &hash)) {
        return ZR_FALSE;
    }
    *outHash = hash;
    return ZR_TRUE;
}

/** @brief 为 stdio 与 WASM pull diagnostics 构造内容及语义快照绑定的结果 ID。
 *  @pre state、context、uri、有效诊断指针数组及输出缓冲区在调用期间有效。
 *  @return 成功时写入完整 NUL 结尾 ID；分配、身份或缓冲区失败时返回假。
 *  @note 只借用诊断和快照；排序只作用于临时指纹数组，不重排原诊断。 */
TZrBool ZrLanguageServer_LspDiagnosticStore_BuildResultId(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrArray *diagnostics,
        TZrChar *buffer,
        TZrSize bufferLength) {
    SZrLspSemanticSnapshot *snapshot;
    const SZrLspSemanticSnapshotIdentity *identity;
    SZrFileVersion *fileVersion;
    SZrLspSemanticSnapshotIdentity documentIdentity = {0};
    TZrUInt64 *hashes = ZR_NULL;
    TZrUInt64 payloadHash = ZR_LSP_DIAGNOSTIC_HASH_OFFSET;
    int written;

    if (buffer == ZR_NULL || bufferLength == 0U || state == ZR_NULL ||
        state->global == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        diagnostics == ZR_NULL || !diagnostics->isValid ||
        diagnostics->elementSize != sizeof(SZrLspDiagnostic *)) {
        return ZR_FALSE;
    }
    if (diagnostics->length > 0U) {
        hashes = (TZrUInt64 *)ZrCore_Memory_RawMalloc(
                state->global, diagnostics->length * sizeof(TZrUInt64));
        if (hashes == ZR_NULL) {
            return ZR_FALSE;
        }
        for (TZrSize index = 0U; index < diagnostics->length; index++) {
            SZrLspDiagnostic *const *diagnostic = (SZrLspDiagnostic *const *)ZrCore_Array_Get(
                    (SZrArray *)diagnostics, index);
            if (!diagnostic_payload_hash(state,
                                         diagnostic != ZR_NULL ? *diagnostic : ZR_NULL,
                                         &hashes[index])) {
                ZrCore_Memory_RawFree(state->global,
                                       hashes,
                                       diagnostics->length * sizeof(TZrUInt64));
                return ZR_FALSE;
            }
        }
        qsort(hashes, (size_t)diagnostics->length, sizeof(*hashes), diagnostic_hash_compare);
    }
    diagnostic_hash_word(&payloadHash, (TZrUInt64)diagnostics->length);
    for (TZrSize index = 0U; index < diagnostics->length; index++) {
        diagnostic_hash_word(&payloadHash, hashes[index]);
    }

    // 快照身份优先于文档代际；最终释放快照前仍需保留身份的有效期。
    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, uri);
    identity = ZrLanguageServer_LspSemanticSnapshot_GetIdentity(snapshot);
    if (identity == ZR_NULL) {
        fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
        if (fileVersion == ZR_NULL || fileVersion->textBlock == ZR_NULL) {
            if (hashes != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, hashes, diagnostics->length * sizeof(TZrUInt64));
            }
            ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
            return ZR_FALSE;
        }
        // 无快照时仍把当前文档和 provider 代际写入 ID，防止旧结果被继续复用。
        documentIdentity.documentGeneration = (TZrUInt64)fileVersion->textBlock->contentGeneration;
        documentIdentity.providerGeneration = context->semanticSnapshotProviderGeneration;
        identity = &documentIdentity;
    }
    written = snprintf(buffer, (size_t)bufferLength,
                       "zr-diagnostic:%llx:%llx:%llx:%llx:%llx:%llx",
                       (unsigned long long)identity->documentGeneration,
                       (unsigned long long)identity->projectGeneration,
                       (unsigned long long)identity->providerGeneration,
                       (unsigned long long)identity->semanticGeneration,
                       (unsigned long long)identity->dependencyFingerprint,
                       (unsigned long long)payloadHash);
    if (hashes != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, hashes, diagnostics->length * sizeof(TZrUInt64));
    }
    ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
    return written >= 0 && (TZrSize)written < bufferLength;
}
