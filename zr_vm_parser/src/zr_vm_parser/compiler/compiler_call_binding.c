#include "compiler_call_binding.h"

#include "compiler_internal.h"
#include "compile_expression_internal.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "compiler_native_call_binding.h"
#include "compiler_typed_call_binding.h"

static TZrUInt32 binding_next_rid(const SZrFunction *function, TZrUInt32 table);

/** @brief 把已解析的 typed-call 签名投影为 core linker 复用的结构哈希。
 *  @pre compiler 的 state/global 可供临时分配；signature 的类型与传参方式数组必须同长且可读，参数数量受元数据 16 位计数限制。
 *  @note 参数与 typed-local 数组仅为哈希临时构造，函数在所有出口释放它们；0 表示当前无法形成有效哈希。
 */
TZrUInt64 compiler_typed_call_signature_hash(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature) {
    SZrFunction temporary;
    SZrFunctionMetadataParameter *parameters;
    SZrFunctionTypedLocalBinding *locals;
    TZrUInt64 hash;
    TZrSize count;
    if (compiler == ZR_NULL || signature == ZR_NULL ||
        signature->parameterTypes.length != signature->parameterPassingModes.length ||
        signature->parameterTypes.length > UINT16_MAX) return 0u;
    memset(&temporary, 0, sizeof(temporary));
    count = signature->parameterTypes.length;
    parameters = count == 0u ? ZR_NULL : ZrCore_Memory_RawMallocWithType(compiler->state->global,
            sizeof(*parameters) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    locals = count == 0u ? ZR_NULL : ZrCore_Memory_RawMallocWithType(compiler->state->global,
            sizeof(*locals) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (count != 0u && (parameters == ZR_NULL || locals == ZR_NULL)) {
        /* TODO: OOM 与结构无效目前都折叠为 0，调用方使用同一诊断；仓内未见区分契约，后续可在此两次申请处注入失败并核定错误分类。 */
        hash = 0u;
        goto cleanup;
    }
    if (count != 0u) {
        memset(parameters, 0, sizeof(*parameters) * count);
        memset(locals, 0, sizeof(*locals) * count);
    }
    temporary.parameterCount = (TZrUInt16)count;
    temporary.parameterMetadataCount = (TZrUInt32)count;
    temporary.parameterMetadata = parameters;
    temporary.hasCallableReturnType = ZR_TRUE;
    compiler_typed_type_ref_from_inferred(&temporary.callableReturnType, &signature->returnType);
    temporary.typedLocalBindings = locals;
    temporary.typedLocalBindingLength = (TZrUInt32)count;
    for (TZrSize index = 0u; index < count; ++index) {
        const SZrInferredType *type = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&signature->parameterTypes, index);
        const EZrParameterPassingMode *mode = (const EZrParameterPassingMode *)ZrCore_Array_Get(
                (SZrArray *)&signature->parameterPassingModes, index);
        if (type == ZR_NULL || mode == ZR_NULL) {
            hash = 0u;
            goto cleanup;
        }
        compiler_typed_type_ref_from_inferred(&parameters[index].type, type);
        parameters[index].name = ZR_NULL;
        locals[index].stackSlot = (TZrUInt32)index;
        locals[index].roleFlags = (*mode == ZR_PARAMETER_PASSING_MODE_IN)
                ? ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN
                : (*mode == ZR_PARAMETER_PASSING_MODE_REF)
                ? ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF
                : (*mode == ZR_PARAMETER_PASSING_MODE_OUT)
                ? ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT
                : ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    }
    hash = ZrCore_CallBinding_FunctionSignatureHash(&temporary);
cleanup:
    if (parameters != ZR_NULL) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            parameters, sizeof(*parameters) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (locals != ZR_NULL) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            locals, sizeof(*locals) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return hash;
}

/** @brief 为 typed-call cache 发布可由后续运行时校验的签名 token。
 *  @pre entry 已包含非零 signatureHash；function 的 token 记录由函数图持有。
 *  @note 先复制并扩展记录数组，再替换旧数组；分配或 RID 越界时返回 false，旧数组仍由 function 持有。
 */
static TZrBool binding_publish_typed_signature(SZrCompilerState *compiler,
                                               SZrFunction *function,
                                               SZrFunctionCallSiteCacheEntry *entry) {
    TZrUInt32 rid;
    SZrMetadataTokenRecord *records;
    TZrSize oldSize;
    if (entry->binding.contract.signatureToken != 0u) return ZR_TRUE;
    rid = binding_next_rid(function, ZR_METADATA_TABLE_SIGNATURE);
    if (rid > ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
    oldSize = sizeof(*records) * function->metadataTokenRecordLength;
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global,
            oldSize + sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    if (oldSize != 0u) memcpy(records, function->metadataTokenRecords, oldSize);
    memset(&records[function->metadataTokenRecordLength], 0, sizeof(*records));
    records[function->metadataTokenRecordLength].token = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_SIGNATURE, rid);
    records[function->metadataTokenRecordLength].signatureHash = entry->binding.contract.signatureHash;
    records[function->metadataTokenRecordLength].reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_SIGNATURE;
    records[function->metadataTokenRecordLength].ownerIndex = ZR_CALL_BINDING_SLOT_NONE;
    entry->binding.contract.signatureToken = records[function->metadataTokenRecordLength].token;
    if (oldSize != 0u) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            function->metadataTokenRecords, oldSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = records;
    ++function->metadataTokenRecordLength;
    return ZR_TRUE;
}

/** @brief 将已解析的 callable 值调用记录为待 finalize 的结构签名约束。
 *  @pre 当前编译上下文须有函数及完整解析签名，argumentCount 必须等于形参数。
 *  @note 此阶段不选择目标或发布 metadata token；失败会让表达式编译清理并中止。
 */
TZrBool compiler_record_typed_call_binding(SZrCompilerState *compiler,
                                           const SZrResolvedCallSignature *signature,
                                           TZrUInt32 argumentCount,
                                           SZrFileRange location) {
    SZrFunctionCallSiteCacheEntry *entry;
    TZrUInt16 cacheIndex;
    TZrUInt64 signatureHash;
    if (compiler == ZR_NULL || compiler->currentFunction == ZR_NULL || signature == ZR_NULL ||
        argumentCount != signature->parameterTypes.length) return ZR_FALSE;
    signatureHash = compiler_typed_call_signature_hash(compiler, signature);
    if (signatureHash == 0u) {
        ZrParser_Compiler_Error(compiler, "Typed callable has no complete structural signature", location);
        return ZR_FALSE;
    }
    if (!reserve_member_slot_get_cache(compiler, ZR_PARSER_MEMBER_ID_NONE, ZR_NULL,
                argumentCount, &cacheIndex, location)) return ZR_FALSE;
    entry = &compiler->currentFunction->callSiteCaches[cacheIndex];
    entry->kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL;
    entry->binding.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
    entry->binding.contract.targetMetadataToken = 0u;
    entry->binding.contract.signatureHash = signatureHash;
    entry->binding.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    entry->binding.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    entry->bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    entry->bindingLocation.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    return ZR_TRUE;
}

/** @brief 从成员解析结果构造或复制一个调用点的 dispatch fact。
 *  @pre fact 必须指向可写输出；member 可为空。
 *  @note VM-module/native provider 自带的身份优先原样保留；普通成员则按 interface、virtual 与静态直接调用规则补齐 operation。
 */
void compiler_get_member_call_binding_fact(SZrCompilerState *compiler,
        const SZrTypeMemberInfo *member, SZrCallBindingContract *fact) {
    ZR_UNUSED_PARAMETER(compiler);
    if (member != ZR_NULL &&
        (member->callBindingLocationKind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
         compiler_native_call_binding_is_provider_contract(&member->callBindingFact))) {
        /* Provider 的 token/hash 属于导入身份；保留它们供后续 registry 或 module linker 重定位。 */
        *fact = member->callBindingFact;
        return;
    }
    memset(fact, 0, sizeof(*fact));
    if (member == ZR_NULL) return;
    fact->bindingKind = ZR_CALL_BINDING_DIRECT;
    fact->dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    fact->targetMetadataToken = member->metadataToken;
    fact->signatureToken = member->signatureToken;
    fact->signatureHash = member->signatureHash;
    if (!member->isStatic && member->interfaceContractSlot != ZR_CALL_BINDING_SLOT_NONE &&
        member->compiledFunction == ZR_NULL) {
        fact->bindingKind = ZR_CALL_BINDING_INTERFACE;
        fact->dispatchSlot = member->interfaceContractSlot;
    } else if (!member->isStatic && member->virtualSlotIndex != ZR_CALL_BINDING_SLOT_NONE &&
        (member->modifierFlags & (ZR_DECLARATION_MODIFIER_VIRTUAL | ZR_DECLARATION_MODIFIER_OVERRIDE |
                                  ZR_DECLARATION_MODIFIER_ABSTRACT)) != 0u) {
        fact->bindingKind = ZR_CALL_BINDING_VIRTUAL;
        fact->dispatchSlot = member->virtualSlotIndex;
    }
    fact->operation = member->isMetaMethod ? ZR_CALL_BINDING_OPERATION_META : ZR_CALL_BINDING_OPERATION_CALL;
    if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) fact->operation = ZR_CALL_BINDING_OPERATION_GET;
    if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_SET || member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_INIT)
        fact->operation = ZR_CALL_BINDING_OPERATION_SET;
}

/** @brief 在指定 metadata token 表中选择当前记录集之后的 RID。
 *  @pre function 及其 token 记录数组处于有效状态。
 *  @note 只取表内最大 RID 加一，不填补历史空洞；实际 token 上限由发布方检查。
 */
static TZrUInt32 binding_next_rid(const SZrFunction *function, TZrUInt32 table) {
    TZrUInt32 maximum = 0u;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        TZrMetadataToken token = function->metadataTokenRecords[index].token;
        if (ZR_METADATA_TOKEN_TABLE(token) == table && ZR_METADATA_TOKEN_RID(token) > maximum)
            maximum = ZR_METADATA_TOKEN_RID(token);
    }
    return maximum + 1u;
}

/** @brief 为静态常量或模块 dispatch 发布成对的成员定义与签名记录。
 *  @pre entry 已有非零签名哈希及目标 owner index；新数组成功后由 function 接管。
 *  @note 同一 marker/owner 已有记录时复用 token，并拒绝签名漂移；RID 或分配失败不替换旧记录。
 */
static TZrBool binding_publish_definition(SZrCompilerState *compiler, SZrFunction *function,
                                          SZrFunctionCallSiteCacheEntry *entry,
                                          TZrUInt32 marker) {
    TZrUInt32 memberRid = binding_next_rid(function, ZR_METADATA_TABLE_MEMBER_DEF);
    TZrUInt32 signatureRid = binding_next_rid(function, ZR_METADATA_TABLE_SIGNATURE);
    SZrMetadataTokenRecord *records;
    SZrMetadataTokenRecord *record;
    TZrSize oldSize = sizeof(*records) * function->metadataTokenRecordLength;
    if (memberRid > ZR_METADATA_TOKEN_RID_MASK || signatureRid > ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            record->reserved0 == marker &&
            record->ownerIndex == entry->bindingLocation.targetIndex) {
            entry->binding.contract.targetMetadataToken = record->token;
            entry->binding.contract.signatureToken = record->relatedToken;
            return record->signatureHash == entry->binding.contract.signatureHash;
        }
    }
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global,
            oldSize + sizeof(*records) * 2u, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    if (oldSize != 0u) memcpy(records, function->metadataTokenRecords, oldSize);
    record = &records[function->metadataTokenRecordLength];
    memset(record, 0, sizeof(*record) * 2u);
    record->token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid);
    record->relatedToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRid);
    record->ownerIndex = entry->bindingLocation.targetIndex;
    record->reserved0 = marker;
    record->signatureHash = entry->binding.contract.signatureHash;
    record[1] = *record;
    record[1].token = record->relatedToken;
    record[1].relatedToken = record->token;
    entry->binding.contract.targetMetadataToken = record->token;
    entry->binding.contract.signatureToken = record->relatedToken;
    if (oldSize != 0u) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            function->metadataTokenRecords, oldSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = records;
    function->metadataTokenRecordLength += 2u;
    return ZR_TRUE;
}

/** @brief 将绑定成员定位到函数图 prototype descriptor 并附加布局 owner guard。
 *  @pre entry 的 memberEntryIndex 指向函数拥有的成员缓存；prototype blob 必须可供 core 解析。
 *  @note interface contract 可先保留其已知 descriptor index；具体 target 则必须唯一匹配 descriptor，发布数组由 function 接管。
 */
static TZrBool binding_publish_owner(SZrCompilerState *compiler, SZrFunction *function,
                                     SZrFunctionCallSiteCacheEntry *entry, SZrFunction *target) {
    SZrFunctionMemberEntry *member;
    SZrFunction *owner = ZrCore_CallBinding_PrototypeOwner(function);
    SZrObjectPrototype *prototype;
    SZrMetadataTokenRecord *records;
    TZrSize oldSize;
    TZrUInt32 rid;
    if (entry->memberEntryIndex >= function->memberEntryLength) {
        return ZR_TRUE;
    }
    member = &function->memberEntries[entry->memberEntryIndex];
    if (member->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR) {
        return ZR_TRUE;
    }
    if (owner == ZR_NULL) {
        return ZR_FALSE;
    }
    if (owner->prototypeInstances == ZR_NULL) ZrCore_Module_CreatePrototypesFromData(compiler->state, ZR_NULL, owner);
    if (owner->prototypeInstances == ZR_NULL || member->prototypeIndex >= owner->prototypeInstancesLength ||
        (prototype = owner->prototypeInstances[member->prototypeIndex]) == ZR_NULL) return ZR_FALSE;
    if (target == ZR_NULL && entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE) {
        /* Interface contract 只知道 descriptor 槽，不要求编译期存在具体实现函数。 */
        if (member->descriptorIndex >= prototype->memberDescriptorCount) return ZR_FALSE;
    } else {
        member->descriptorIndex = (TZrUInt32)-1;
        for (TZrUInt32 index = 0u; index < prototype->memberDescriptorCount; ++index) {
            if (prototype->memberDescriptors[index].methodFunction == target) {
                if (member->descriptorIndex != (TZrUInt32)-1) return ZR_FALSE;
                member->descriptorIndex = index;
            }
        }
    }
    if (member->descriptorIndex == (TZrUInt32)-1) return ZR_FALSE;
    entry->binding.contract.layoutHash = ZrCore_CallBinding_PrototypeLayoutHash(owner, member->prototypeIndex);
    entry->binding.contract.layoutVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    if (entry->binding.contract.layoutHash == 0u) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            record->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER &&
            record->ownerIndex == member->prototypeIndex) {
            entry->binding.contract.ownerTypeToken = record->token;
            return record->signatureHash == entry->binding.contract.layoutHash;
        }
    }
    rid = binding_next_rid(function, ZR_METADATA_TABLE_TYPE_DEF);
    if (rid > ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
    oldSize = sizeof(*records) * function->metadataTokenRecordLength;
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global, oldSize + sizeof(*records),
                                            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    if (oldSize != 0u) memcpy(records, function->metadataTokenRecords, oldSize);
    memset(&records[function->metadataTokenRecordLength], 0, sizeof(*records));
    records[function->metadataTokenRecordLength].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, rid);
    records[function->metadataTokenRecordLength].ownerIndex = member->prototypeIndex;
    records[function->metadataTokenRecordLength].reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER;
    records[function->metadataTokenRecordLength].signatureHash = entry->binding.contract.layoutHash;
    entry->binding.contract.ownerTypeToken = records[function->metadataTokenRecordLength].token;
    if (oldSize != 0u) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            function->metadataTokenRecords, oldSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = records;
    ++function->metadataTokenRecordLength;
    return ZR_TRUE;
}

/** @brief VisitFunctions 回调共享的编译期状态与当前模块签名身份。 */
typedef struct SZrCompilerCallBindingContext {
    SZrCompilerState *compiler;
    TZrUInt64 moduleSignatureHash;
} SZrCompilerCallBindingContext;

/** @brief 为函数图中的一个函数补全所有静态调用契约及其本地 metadata。
 *  @pre 由 VisitFunctions 对图中每个函数调用；context 指向本次 finalize 的栈上上下文。
 *  @note provider 身份不在此解析目标；typed/static 两类本地契约失败即向外返回 false，由编译入口释放整张函数图。
 */
static TZrBool binding_finalize_function(SZrFunction *function, void *data) {
    SZrCompilerCallBindingContext *context = data;
    SZrCompilerState *compiler = context->compiler;
    if (function->moduleSignatureHash == 0u)
        function->moduleSignatureHash = context->moduleSignatureHash;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        SZrFunction *target = ZR_NULL;
        TZrUInt32 marker;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            /* 运行时按结构签名动态验证 typed callable，不能固化当前编译期目标。 */
            compiler_typed_call_use_generic_dispatch(function, entry->instructionIndex);
            if (function->moduleSignatureHash == 0u) {
                function->moduleSignatureHash = ZrCore_Hash_CreateStable64(
                        function->prototypeData, function->prototypeDataLength);
            }
            entry->binding.contract.moduleSignatureHash = function->moduleSignatureHash;
            if (entry->binding.contract.signatureHash == 0u ||
                !binding_publish_typed_signature(compiler, function, entry)) {
                ZrParser_Compiler_Error(compiler,
                        "Typed function call has no complete callable signature contract",
                        compiler->currentAst != ZR_NULL ? compiler->currentAst->location : (SZrFileRange){0});
                return ZR_FALSE;
            }
            entry->binding.generation = function->callBindingGeneration;
            continue;
        }
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
             compiler_native_call_binding_is_provider_contract(&entry->binding.contract))) {
            /* Provider identities are already complete.  Keep their token,
             * signature and module hash intact; the native registry performs
             * relocation during linking. */
            entry->bindingLocation.ownerDepth = 0u;
            entry->bindingLocation.flags = 0u;
            entry->binding.generation = function->callBindingGeneration;
            continue;
        }
        /* 本地常量目标与本地 interface/virtual 模块槽需要 token、签名及 owner 布局记录。 */
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT) {
            if (entry->bindingLocation.targetIndex >= function->constantValueLength) return ZR_FALSE;
            target = ZrCore_Closure_GetMetadataFunctionFromValue(compiler->state,
                    &function->constantValueList[entry->bindingLocation.targetIndex]);
            entry->binding.contract.signatureHash = ZrCore_CallBinding_FunctionSignatureHash(target);
            marker = ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT;
        } else if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
                   (entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE ||
                    entry->binding.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL)) {
            marker = ZR_METADATA_TOKEN_RECORD_CALLABLE_MODULE;
        } else {
            return ZR_FALSE;
        }
        if (function->moduleSignatureHash == 0u && function->prototypeData != ZR_NULL &&
            function->prototypeDataLength != 0u) {
            function->moduleSignatureHash = ZrCore_Hash_CreateStable64(
                    function->prototypeData, function->prototypeDataLength);
        }
        entry->binding.contract.moduleSignatureHash = function->moduleSignatureHash;
        if (entry->binding.contract.signatureHash == 0u ||
            !binding_publish_definition(compiler, function, entry, marker) ||
            !binding_publish_owner(compiler, function, entry, target)) {
            ZrParser_Compiler_Error(compiler, "Static call binding has no complete callable contract",
                    compiler->currentAst != ZR_NULL ? compiler->currentAst->location : (SZrFileRange){0});
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/** @brief 按图遍历、provider 发布、core 链接的顺序完成本地调用绑定。
 *  @pre quickening 与当前 source module 定稿已完成；function 是本轮编译拥有的完整函数图。
 *  @note 编译入口只调用一次；任一步失败都会丢弃整张函数图，因此部分修改状态不会作为可重试结果返回。
 */
TZrBool compiler_finalize_call_bindings(SZrCompilerState *compiler, SZrFunction *function) {
    SZrCompilerCallBindingContext context;
    if (compiler == ZR_NULL || function == ZR_NULL) return ZR_FALSE;
    context.compiler = compiler;
    context.moduleSignatureHash = function->moduleSignatureHash != 0u ? function->moduleSignatureHash :
            ZrCore_Hash_CreateStable64(function->prototypeData, function->prototypeDataLength);
    /* 先定稿图内调用及本地记录，再扩充跨模块身份，最后交 core 建立可执行的链接目标。 */
    return ZrCore_CallBinding_VisitFunctions(function, binding_finalize_function, &context) &&
            compiler_publish_module_call_bindings(compiler, function) &&
            ZrCore_CallBinding_LinkFunction(compiler->state, function, &compiler->state->lastCallBindingError);
}
