#include "compiler_native_call_binding.h"

#include <string.h>
#include "zr_vm_parser/ast.h"

/** @brief 按 provider 的持久化 token 与模块哈希识别需跨模块重定位的 native 调用身份。 */
TZrBool compiler_native_call_binding_is_provider_contract(
        const SZrCallBindingContract *contract) {
    if (contract == ZR_NULL || contract->bindingKind != ZR_CALL_BINDING_DIRECT ||
        contract->targetMetadataToken == 0u || contract->signatureToken == 0u ||
        contract->signatureHash == 0u || contract->moduleSignatureHash == 0u) {
        return ZR_FALSE;
    }
    return ZR_METADATA_TOKEN_TABLE(contract->targetMetadataToken) == ZR_METADATA_TABLE_MEMBER_DEF &&
           ZR_METADATA_TOKEN_TABLE(contract->signatureToken) == ZR_METADATA_TABLE_SIGNATURE;
}

/* Read persistent callsite identity before link installs runtime targets. */
TZrBool compiler_native_call_binding_is_provider_known_call(
        const SZrFunction *function, TZrUInt32 instructionIndex) {
    const SZrFunctionCallSiteCacheEntry *match = ZR_NULL;
    const TZrInstruction *instruction;
    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }
    instruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_CALL)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->instructionIndex != instructionIndex) continue;
        if (match != ZR_NULL) return ZR_FALSE;
        match = entry;
    }
    return match != ZR_NULL && match->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL &&
           match->argumentCount == (TZrUInt32)instruction->instruction.operand.operand1[1] &&
           match->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
           match->bindingLocation.ownerDepth == 0u && match->bindingLocation.flags == 0u &&
           match->binding.contract.operation == ZR_CALL_BINDING_OPERATION_CALL &&
           compiler_native_call_binding_is_provider_contract(&match->binding.contract) &&
           ZrCore_CallBinding_CheckContract(&match->binding.contract, ZR_NULL) == ZR_CALL_BINDING_OK;
}

/** @brief 为已识别的 provider 成员建立模块重定位缓存，保留原有签名及来源哈希。
 *  @note targetIndex 记录调用点的成员定位值；无成员槽的静态调用使用 provider token。
 *  native registry 按契约 token 与哈希选择 provider，不读取该索引。 */
TZrBool compiler_native_call_binding_prepare_cache(
        SZrFunctionCallSiteCacheEntry *entry,
        const SZrCallBindingContract *contract,
        TZrUInt32 memberEntryIndex) {
    if (entry == ZR_NULL || contract == ZR_NULL ||
        !compiler_native_call_binding_is_provider_contract(contract)) {
        return ZR_FALSE;
    }
    entry->binding.contract = *contract;
    entry->bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    entry->bindingLocation.targetIndex = memberEntryIndex != ZR_PARSER_MEMBER_ID_NONE
            ? memberEntryIndex : contract->targetMetadataToken;
    entry->bindingLocation.ownerDepth = 0u;
    entry->bindingLocation.flags = 0u;
    return ZR_TRUE;
}

/** @brief 预留完整契约校验入口，失败时把当前 AST 位置作为编译诊断位置。
 *  TODO: 当前无生产调用方；链接阶段已执行契约校验，需确认是否还需要接入此编译期诊断入口。 */
TZrBool compiler_finalize_native_call_binding(
        SZrCompilerState *compiler, SZrFunctionCallSiteCacheEntry *entry) {
    SZrCallBindingDiagnostic diagnostic;
    if (compiler == ZR_NULL || entry == ZR_NULL) return ZR_FALSE;
    if (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_MODULE ||
        !compiler_native_call_binding_is_provider_contract(&entry->binding.contract) ||
        ZrCore_CallBinding_CheckContract(&entry->binding.contract, &diagnostic) != ZR_CALL_BINDING_OK) {
        ZrParser_Compiler_Error(compiler, "Native static call binding has no complete provider contract",
                compiler->currentAst != ZR_NULL ? compiler->currentAst->location : (SZrFileRange){0});
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
