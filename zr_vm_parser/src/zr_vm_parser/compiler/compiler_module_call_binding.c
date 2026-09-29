#include "compiler_call_binding.h"

#include <string.h>

#include "compiler_internal.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/memory.h"

/** @brief 为当前函数的 metadata 表保留下一个局部 RID。
 *  @pre function 必须是正在完成 call-binding 发布的有效函数；调用方在持有该函数期间独占修改记录。
 *  @note RID 按 token table 分别递增，不能用记录总数代替最大 RID，因为既有记录可稀疏且跨表混排。
 */
static TZrUInt32 next_rid(const SZrFunction *function, TZrUInt32 table) {
    TZrUInt32 rid = 0u;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        TZrMetadataToken token = function->metadataTokenRecords[index].token;
        if (ZR_METADATA_TOKEN_TABLE(token) == table && ZR_METADATA_TOKEN_RID(token) > rid)
            rid = ZR_METADATA_TOKEN_RID(token);
    }
    return rid + 1u;
}

/** @brief 在本地调用契约定稿后，把跨模块消费者需要的 callable 身份补入函数 metadata。
 *  @pre compiler、compiler->state 与 function 有效；仅在函数的 call-site binding 完成后调用。
 *  @note compiler_finalize_call_bindings 在链接当前函数之前调用本入口；导入方随后以 token 和签名契约
 *        定位 provider 常量或导出函数，不能依赖 provider 恰好从自身代码调用过该目标。
 *  TODO: 当前发布路径按每个函数只运行一次设计；重复调用会再次追加 CALLABLE_CHILD 关联记录，
 *        若未来引入重试或增量重定稿，应先为 child 关联增加查重/替换策略。
 *  @return 元数据完整发布时返回 true；容量、布局、RID 或分配失败时返回 false，发布数组保持原状。
 */
TZrBool compiler_publish_module_call_bindings(SZrCompilerState *compiler, SZrFunction *function) {
    TZrUInt32 memberRid = next_rid(function, ZR_METADATA_TABLE_MEMBER_DEF);
    TZrUInt32 signatureRid = next_rid(function, ZR_METADATA_TABLE_SIGNATURE);
    TZrUInt32 typeRid = next_rid(function, ZR_METADATA_TABLE_TYPE_DEF);
    TZrSize capacity = (TZrSize)function->metadataTokenRecordLength +
            (TZrSize)function->constantValueLength * 3u + function->prototypeCount;
    TZrUInt32 count = function->metadataTokenRecordLength;
    SZrMetadataTokenRecord *records;
    TZrSize offset = sizeof(TZrUInt32);
    /* 为旧记录、每个 callable constant 的 member/signature/child 三类上限，以及每个 prototype 的 owner 记录一次预留。 */
    if (capacity == 0u) return ZR_TRUE;
    if (capacity > UINT32_MAX || capacity > SIZE_MAX / sizeof(*records)) return ZR_FALSE;
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global,
            capacity * sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    memset(records, 0, capacity * sizeof(*records));
    if (count != 0u) memcpy(records, function->metadataTokenRecords, count * sizeof(*records));
    /* 为可导入的非 native 常量函数发布稳定身份；导出消费者不能依赖 provider 自己是否调用过这些方法。 */
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
        /* 常量重绑定到本函数 child 时记录精确关联，使闭包和同一源码行上的函数无需名称/源码区间即可重定位。 */
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
    /* 为 prototype 建立 owner token，并把方法常量绑定到相同的布局版本/哈希，供导入链接建立 owner guard。 */
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
    /* 到此才替换正式 metadata；此前所有解析和扩容失败都只丢弃临时副本，不留下半发布状态。 */
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
