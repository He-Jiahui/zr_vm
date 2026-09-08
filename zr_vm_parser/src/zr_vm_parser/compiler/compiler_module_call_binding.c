#include "compiler_call_binding.h"

#include <string.h>

#include "compiler_internal.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/memory.h"

static TZrUInt32 next_rid(const SZrFunction *function, TZrUInt32 table) {
    TZrUInt32 rid = 0u;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        TZrMetadataToken token = function->metadataTokenRecords[index].token;
        if (ZR_METADATA_TOKEN_TABLE(token) == table && ZR_METADATA_TOKEN_RID(token) > rid)
            rid = ZR_METADATA_TOKEN_RID(token);
    }
    return rid + 1u;
}

TZrBool compiler_publish_module_call_bindings(SZrCompilerState *compiler, SZrFunction *function) {
    TZrUInt32 memberRid = next_rid(function, ZR_METADATA_TABLE_MEMBER_DEF);
    TZrUInt32 signatureRid = next_rid(function, ZR_METADATA_TABLE_SIGNATURE);
    TZrUInt32 typeRid = next_rid(function, ZR_METADATA_TABLE_TYPE_DEF);
    TZrSize capacity = (TZrSize)function->metadataTokenRecordLength +
            (TZrSize)function->constantValueLength * 3u + function->prototypeCount;
    TZrUInt32 count = function->metadataTokenRecordLength;
    SZrMetadataTokenRecord *records;
    TZrSize offset = sizeof(TZrUInt32);
    if (capacity == 0u) return ZR_TRUE;
    if (capacity > UINT32_MAX || capacity > SIZE_MAX / sizeof(*records)) return ZR_FALSE;
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global,
            capacity * sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    memset(records, 0, capacity * sizeof(*records));
    if (count != 0u) memcpy(records, function->metadataTokenRecords, count * sizeof(*records));
    /* Export consumers must not depend on which methods the provider calls. */
    for (TZrUInt32 index = 0u; index < function->constantValueLength; ++index) {
        SZrFunction *target = ZrCore_Closure_GetMetadataFunctionFromValue(compiler->state,
                &function->constantValueList[index]);
        SZrMetadataTokenRecord *record = ZR_NULL;
        TZrUInt64 hash;
        if (target == ZR_NULL || target->super.isNative) continue;
        hash = ZrCore_CallBinding_FunctionSignatureHash(target);
        if (hash == 0u) continue;
        for (TZrUInt32 row = 0u; row < count; ++row) {
            if (ZR_METADATA_TOKEN_TABLE(records[row].token) == ZR_METADATA_TABLE_MEMBER_DEF &&
                records[row].reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT &&
                records[row].ownerIndex == index) { record = &records[row]; break; }
        }
        if (record != ZR_NULL) {
            if (record->signatureHash != hash) goto fail;
        } else {
            if (memberRid > ZR_METADATA_TOKEN_RID_MASK || signatureRid > ZR_METADATA_TOKEN_RID_MASK) goto fail;
            record = &records[count];
            record->token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid++);
            record->relatedToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRid++);
            record->reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT;
            record->ownerIndex = index;
            record->signatureHash = hash;
            record[1] = *record;
            record[1].token = record->relatedToken;
            record[1].relatedToken = record->token;
            count += 2u;
        }
        /* Persist the exact constant-to-child identity when the compiler has
         * already rebound this value.  This keeps closures and same-line
         * functions relocatable without relying on names or source spans. */
        for (TZrUInt32 child = 0u; child < function->childFunctionLength; ++child) {
            SZrFunction *childFunction = &function->childFunctionList[child];
            if (target != childFunction) continue;
            if (memberRid > ZR_METADATA_TOKEN_RID_MASK) goto fail;
            records[count].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid++);
            records[count].reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_CHILD;
            records[count].ownerIndex = child;
            records[count].signatureHash = hash;
            record->targetMetadataToken = records[count].token;
            ++count;
            break;
        }
    }
    for (TZrUInt32 index = 0u; index < function->prototypeCount; ++index) {
        SZrCompiledPrototypeInfo prototype;
        TZrUInt64 bytes;
        TZrUInt64 hash = ZrCore_CallBinding_PrototypeLayoutHash(function, index);
        TZrMetadataToken ownerToken = 0u;
        const SZrCompiledMemberInfo *members;
        if (hash == 0u || offset > function->prototypeDataLength ||
            function->prototypeDataLength - offset < sizeof(prototype)) goto fail;
        memcpy(&prototype, function->prototypeData + offset, sizeof(prototype));
        bytes = sizeof(prototype) + ((TZrUInt64)prototype.inheritsCount + prototype.decoratorsCount) * sizeof(TZrUInt32) +
                (TZrUInt64)prototype.membersCount * sizeof(*members);
        if (bytes > function->prototypeDataLength - offset) goto fail;
        members = (const SZrCompiledMemberInfo *)(function->prototypeData + offset + sizeof(prototype) +
                ((TZrSize)prototype.inheritsCount + prototype.decoratorsCount) * sizeof(TZrUInt32));
        for (TZrUInt32 row = 0u; row < count; ++row) {
            if (records[row].reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER &&
                records[row].ownerIndex == index) { ownerToken = records[row].token; break; }
        }
        if (ownerToken == 0u) {
            if (typeRid > ZR_METADATA_TOKEN_RID_MASK) goto fail;
            ownerToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, typeRid++);
            records[count].token = ownerToken;
            records[count].reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER;
            records[count].ownerIndex = index;
            records[count++].signatureHash = hash;
        }
        for (TZrUInt32 member = 0u; member < prototype.membersCount; ++member) {
            if (members[member].memberType != ZR_AST_CLASS_METHOD &&
                members[member].memberType != ZR_AST_STRUCT_METHOD &&
                members[member].memberType != ZR_AST_CLASS_META_FUNCTION &&
                members[member].memberType != ZR_AST_STRUCT_META_FUNCTION) continue;
            for (TZrUInt32 row = 0u; row < count; ++row) {
                if (records[row].reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT &&
                    records[row].ownerIndex == members[member].functionConstantIndex) {
                    records[row].ownerToken = ownerToken;
                    records[row].layoutVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
                    records[row].layoutHash = hash;
                }
            }
        }
        offset += (TZrSize)bytes;
    }
    if (count != function->metadataTokenRecordLength) {
        SZrMetadataTokenRecord *exact = ZrCore_Memory_RawMallocWithType(compiler->state->global,
                count * sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (exact == ZR_NULL) goto fail;
        memcpy(exact, records, count * sizeof(*records));
        if (function->metadataTokenRecords != ZR_NULL) ZrCore_Memory_RawFreeWithType(compiler->state->global,
                function->metadataTokenRecords, function->metadataTokenRecordLength * sizeof(*records),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        function->metadataTokenRecords = exact;
        function->metadataTokenRecordLength = count;
    } else if (count != 0u) {
        memcpy(function->metadataTokenRecords, records, count * sizeof(*records));
    }
    ZrCore_Memory_RawFreeWithType(compiler->state->global, records,
            capacity * sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_TRUE;
fail:
    ZrCore_Memory_RawFreeWithType(compiler->state->global, records,
            capacity * sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_FALSE;
}
