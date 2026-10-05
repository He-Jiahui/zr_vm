#include "compiler_script_entry_metadata.h"
#include "compiler_script_callable_return.h"
#include "compiler_metadata_signature.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"

#include <stdint.h>
#include <string.h>

/* Narrow producer slice: ordinary, versionless, pure source SCRIPT only. */
static TZrBool script_entry_is_eligible(const SZrCompilerState *cs, const SZrFunction *function) {
    return (TZrBool)(cs != ZR_NULL && function != ZR_NULL &&
            cs->currentAst != ZR_NULL && cs->currentAst->type == ZR_AST_SCRIPT &&
            cs->currentAst == cs->scriptAst && cs->currentFunction == function &&
            cs->currentFunctionNode == ZR_NULL && cs->topLevelFunction == ZR_NULL &&
            cs->submissionContext == ZR_NULL && cs->submissionEntryFunction == ZR_NULL &&
            cs->currentModuleKey != ZR_NULL && ZrCore_String_GetByteLength(cs->currentModuleKey) != 0U &&
            function->moduleVersion == ZR_NULL && function->parameterCount == 0U &&
            !function->hasVariableArguments && function->staticImportLength == 0U &&
            function->moduleEntryEffectLength == 0U && function->childFunctionLength == 0U &&
            function->closureValueLength == 0U && function->typedClosureBindingLength == 0U &&
            cs->closureVars.length == 0U && function->exportedVariableLength == 0U &&
            function->typedExportedSymbolLength == 0U &&
            function->exportedCallableSummaryLength == 0U && function->topLevelCallableBindingLength == 0U &&
            cs->preSemanticIrInitialized && cs->preSemanticIrValidated && cs->preSemanticIrCfgActive &&
            cs->semanticContext != ZR_NULL);
}

TZrBool compiler_script_entry_metadata_prepare(SZrCompilerState *cs, SZrFunction *function) {
    if (!compiler_script_callable_return_publish(cs, function, cs != ZR_NULL ? cs->currentAst : ZR_NULL))
        return ZR_FALSE;
    if (!script_entry_is_eligible(cs, function) || !function->hasCallableReturnType ||
        function->callableReturnType.baseType != ZR_VALUE_TYPE_INT64)
        return ZR_TRUE;
    function->functionName = cs->currentModuleKey;
    ZrCore_RawObject_Barrier(cs->state, &function->super, &cs->currentModuleKey->super);
    return ZR_TRUE;
}

TZrBool compiler_script_entry_metadata_plan(SZrCompilerState *cs, const SZrFunction *function,
                                           SZrScriptEntryMetadataPlan *plan) {
    if (plan == ZR_NULL) return ZR_FALSE;
    memset(plan, 0, sizeof(*plan));
    if (!script_entry_is_eligible(cs, function) || function->functionName != cs->currentModuleKey ||
        !function->hasCallableReturnType || function->callableReturnType.baseType != ZR_VALUE_TYPE_INT64 ||
        function->callableReturnType.isNullable || function->callableReturnType.isArray ||
        function->callableReturnType.typeName != ZR_NULL || function->callableReturnType.ownershipQualifier != 0U)
        return ZR_TRUE;
    if (function->typedExportedSymbolLength >= ZR_METADATA_TOKEN_RID_MASK)
        return ZR_FALSE;
    plan->signatureHeapLength = metadata_token_method_signature_size(
            cs, &function->callableReturnType, 0U, 0U, ZR_NULL);
    if (plan->signatureHeapLength == 0U || plan->signatureHeapLength > UINT32_MAX) return ZR_FALSE;
    plan->present = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool compiler_script_entry_metadata_emit(SZrCompilerState *cs, const SZrFunction *function,
        const SZrScriptEntryMetadataPlan *plan, SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount, TZrUInt32 *recordIndex, TZrByte *heap, TZrSize heapLength,
        TZrSize *heapOffset, TZrUInt32 memberRid, TZrUInt32 *signatureRid,
        const SZrMetadataStringHeapEntry *strings, TZrUInt32 stringCount) {
    TZrSize start;
    TZrUInt64 hash;
    TZrMetadataToken memberToken, signatureToken, moduleToken = 0U;
    if (!plan->present) return ZR_TRUE;
    if (records == ZR_NULL || heap == ZR_NULL || recordIndex == ZR_NULL || heapOffset == ZR_NULL ||
        signatureRid == ZR_NULL || *recordIndex > recordCount || recordCount - *recordIndex < 2U ||
        *heapOffset > heapLength || plan->signatureHeapLength > heapLength - *heapOffset ||
        memberRid == 0U || memberRid > ZR_METADATA_TOKEN_RID_MASK ||
        *signatureRid == 0U || *signatureRid > ZR_METADATA_TOKEN_RID_MASK)
        return ZR_FALSE;
    for (TZrUInt32 i = 0U; i < *recordIndex; ++i)
        if (ZR_METADATA_TOKEN_TABLE(records[i].token) == ZR_METADATA_TABLE_MODULE) {
            if (moduleToken != 0U) return ZR_FALSE;
            moduleToken = records[i].token;
        }
    if (moduleToken == 0U) return ZR_FALSE;
    start = *heapOffset;
    metadata_token_write_method_signature(heap, heapOffset, cs, &function->callableReturnType,
            0U, 0U, ZR_NULL, strings, stringCount);
    if (*heapOffset - start != plan->signatureHeapLength) return ZR_FALSE;
    hash = metadata_signature_hash_v1(heap + start, plan->signatureHeapLength);
    if (hash == 0U) return ZR_FALSE;
    memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid);
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*signatureRid)++);
    for (TZrUInt32 i = 0U; i < 2U; ++i) {
        SZrMetadataTokenRecord *record = &records[(*recordIndex)++];
        record->token = i == 0U ? memberToken : signatureToken;
        record->relatedToken = i == 0U ? signatureToken : memberToken;
        record->ownerToken = i == 0U ? moduleToken : memberToken;
        record->ownerIndex = ZR_CALL_BINDING_SLOT_NONE;
        record->signatureBlobOffset = (TZrUInt32)start;
        record->signatureBlobLength = (TZrUInt32)plan->signatureHeapLength;
        record->signatureHash = hash;
        record->reserved0 = ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY;
    }
    return ZR_TRUE;
}

/* Versioned entry ABI domain: key byte length + exact bytes, absent version,
 * METHOD_SIG byte length + complete bytes. No tokens or legacy hash guesses. */
TZrUInt64 compiler_script_entry_metadata_hash(SZrCompilerState *cs, const SZrFunction *function) {
    static const TZrByte prefix[] = "zr.md.script.entry.v1";
    const SZrMetadataTokenRecord *entry = ZR_NULL;
    TZrSize keyLength, length, offset = 0U;
    TZrByte *bytes;
    TZrUInt64 hash;
    for (TZrUInt32 i = 0U; i < function->metadataTokenRecordLength; ++i) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[i];
        if (record->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY &&
            ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MEMBER_DEF) {
            if (entry != ZR_NULL) return 0U;
            entry = record;
        }
    }
    if (entry == ZR_NULL || function->functionName == ZR_NULL || function->moduleVersion != ZR_NULL ||
        entry->signatureBlobOffset > function->signatureBlobHeapLength ||
        entry->signatureBlobLength > function->signatureBlobHeapLength - entry->signatureBlobOffset)
        return 0U;
    keyLength = ZrCore_String_GetByteLength(function->functionName);
    if (keyLength > UINT32_MAX || keyLength > SIZE_MAX - 9U ||
        entry->signatureBlobLength > SIZE_MAX - 9U - keyLength) return 0U;
    length = 9U + keyLength + entry->signatureBlobLength;
    bytes = (TZrByte *)ZrCore_Memory_RawMallocWithType(cs->state->global, length, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (bytes == ZR_NULL) return 0U;
    metadata_token_write_u32(bytes, &offset, (TZrUInt32)keyLength);
    memcpy(bytes + offset, ZrCore_String_GetNativeString(function->functionName), keyLength);
    offset += keyLength;
    metadata_token_write_u8(bytes, &offset, 0U);
    metadata_token_write_u32(bytes, &offset, entry->signatureBlobLength);
    memcpy(bytes + offset, function->signatureBlobHeap + entry->signatureBlobOffset, entry->signatureBlobLength);
    hash = ZrCore_Hash_CreateStable64WithPrefix(prefix, sizeof(prefix), bytes, length);
    ZrCore_Memory_RawFreeWithType(cs->state->global, bytes, length, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return hash;
}
