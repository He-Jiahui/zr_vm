#include "zr_vm_core/gc.h"
#include "zr_vm_core/module_call_binding.h"

#define IMPORTED_INSTANCE_REQUIRE(condition, message) \
    do { if (!(condition)) { failure = (message); goto cleanup; } } while (0)

/* The provider selects VM/AOT targets; loading a method must retain its exact binding. */
static void test_source_module_instance_method_uses_provider_dispatch(void) {
    SZrGcNativeCallPin pins[6] = {0};
    ZrTestsFixtureSource savedProvider = provider;
    const char *failure = ZR_NULL;
    SZrFunction *consumer = ZR_NULL;
    SZrString *path = ZR_NULL;
    SZrObjectModule *module = ZR_NULL;
    SZrFunction *metadata = ZR_NULL;
    SZrFunction *candidate = ZR_NULL;
    SZrObjectPrototype *prototype = ZR_NULL;
    SZrFunctionCallSiteCacheEntry *entry = ZR_NULL;
    const SZrMetadataTokenRecord *definition = ZR_NULL;
    const SZrMetadataTokenRecord *owner = ZR_NULL;
    const SZrTypeValue *constant = ZR_NULL;
    const SZrMemberDescriptor *descriptor = ZR_NULL;
    SZrCallBindingDiagnostic diagnostic = {0};
    TZrInstruction observedInstruction = {0};
    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ENUM_MAX);
    const char *opcodeName = "other";
    TZrUInt32 cacheIndex = ZR_CALL_BINDING_SLOT_NONE;
    TZrUInt32 operandCacheIndex = ZR_CALL_BINDING_SLOT_NONE;
    TZrUInt32 descriptorIndex = ZR_CALL_BINDING_SLOT_NONE;
    TZrUInt32 matches = 0u;
    TZrUInt64 layoutHash = 0u;
    TZrInt64 result = 0;
    TZrBool linked = ZR_FALSE;
    TZrBool providerDispatch = ZR_FALSE;
    TZrUInt32 genericCalls = 0u;

    provider.source = "pub class Counter { pub fn add(value: int): int { return value + 1; } }";
    consumer = compile_consumer("var provider = import(\"call_binding_provider\"); "
            "var value = new provider.Counter(); var result = value.add(41); return result;");
    IMPORTED_INSTANCE_REQUIRE(consumer != ZR_NULL, "Imported instance consumer must compile");
    IMPORTED_INSTANCE_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer), &pins[0]),
            "Consumer must remain fixed during provider loading");
    for (TZrUInt32 row = 0u; row < consumer->callSiteCacheLength; ++row) {
        SZrFunctionCallSiteCacheEntry *item = &consumer->callSiteCaches[row];
        if (item->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            item->binding.contract.bindingKind != ZR_CALL_BINDING_DIRECT ||
            item->binding.contract.operation != ZR_CALL_BINDING_OPERATION_CALL ||
            item->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) continue;
        entry = item; cacheIndex = row; ++matches;
    }
    IMPORTED_INSTANCE_REQUIRE(matches == 1u && entry->binding.contract.dispatchSlot == ZR_CALL_BINDING_SLOT_NONE &&
            entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_NONE,
            "Consumer must contain one unresolved imported direct instance CALL");
    IMPORTED_INSTANCE_REQUIRE(consumer->instructionsList != ZR_NULL &&
            entry->instructionIndex < consumer->instructionsLength, "Callsite must name an actual instruction");
    observedInstruction = consumer->instructionsList[entry->instructionIndex];
    opcode = (EZrInstructionCode)ZR_INSTRUCTION_OPCODE(observedInstruction);
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            providerDispatch = ZR_TRUE; opcodeName = "GET_MEMBER_SLOT";
            operandCacheIndex = observedInstruction.instruction.operand.operand1[1]; break;
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
            opcodeName = "KNOWN_VM_MEMBER_CALL";
            operandCacheIndex = observedInstruction.instruction.operand.operand1[0]; break;
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
            opcodeName = "KNOWN_VM_MEMBER_CALL_LOAD1_U8";
            operandCacheIndex = observedInstruction.instruction.operand.operand0[0]; break;
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            opcodeName = "KNOWN_NATIVE_MEMBER_CALL";
            operandCacheIndex = observedInstruction.instruction.operand.operand1[0]; break;
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8):
            opcodeName = "KNOWN_NATIVE_MEMBER_CALL_RECV_U8";
            operandCacheIndex = observedInstruction.instruction.operand.operand0[0]; break;
        default: break;
    }
    IMPORTED_INSTANCE_REQUIRE(operandCacheIndex == cacheIndex, "Actual member instruction must consume this cache");
    IMPORTED_INSTANCE_REQUIRE(consumer->memberEntries != ZR_NULL &&
            entry->memberEntryIndex < consumer->memberEntryLength &&
            !(consumer->memberEntries[entry->memberEntryIndex].reserved0 & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) &&
            ZrTests_Fixture_StringEqualsCString(consumer->memberEntries[entry->memberEntryIndex].symbol, "add"),
            "Actual member coordinate must identify the nonstatic add method");
    IMPORTED_INSTANCE_REQUIRE(providerDispatch, "Imported method must retain its binding on GET_MEMBER_SLOT");
    for (TZrUInt32 index = entry->instructionIndex + 1u; index < consumer->instructionsLength; ++index) {
        EZrInstructionCode callOpcode = (EZrInstructionCode)ZR_INSTRUCTION_OPCODE(consumer->instructionsList[index]);
        if (callOpcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) ||
            callOpcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)) ++genericCalls;
        IMPORTED_INSTANCE_REQUIRE(callOpcode != ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL) &&
                callOpcode != ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8) &&
                callOpcode != ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL) &&
                callOpcode != ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8),
                "Imported method implementation must be selected by the provider at runtime");
    }
    IMPORTED_INSTANCE_REQUIRE(genericCalls == 1u, "Bound imported method must use generic call dispatch");
    path = ZrCore_String_CreateFromNative(state, (TZrNativeString)provider.path);
    IMPORTED_INSTANCE_REQUIRE(path != ZR_NULL &&
            ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(path), &pins[1]), "Provider path must stay fixed");
    module = ZrCore_Module_ImportByPath(state, path);
    IMPORTED_INSTANCE_REQUIRE(module != ZR_NULL && ZrCore_Module_GetFromCache(state, path) == module &&
            ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module), &pins[2]), "Real cached provider must stay fixed");
    metadata = ZrCore_CallBinding_GetModuleFunction(state, module);
    IMPORTED_INSTANCE_REQUIRE(metadata != ZR_NULL && metadata->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
            !metadata->super.isNative &&
            ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(metadata), &pins[3]), "Provider metadata must be VM");
    IMPORTED_INSTANCE_REQUIRE(metadata->moduleSignatureHash != 0u &&
            entry->binding.contract.moduleSignatureHash == metadata->moduleSignatureHash &&
            metadata->metadataTokenRecords != ZR_NULL, "Consumer must select the actual provider module signature");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < metadata->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &metadata->metadataTokenRecords[row];
        if (record->token == entry->binding.contract.targetMetadataToken) { definition = record; ++matches; }
    }
    IMPORTED_INSTANCE_REQUIRE(matches == 1u && ZR_METADATA_TOKEN_TABLE(definition->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            definition->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT &&
            metadata->constantValueList != ZR_NULL && definition->ownerIndex < metadata->constantValueLength,
            "Target must name one actual provider callable constant");
    constant = &metadata->constantValueList[definition->ownerIndex];
    IMPORTED_INSTANCE_REQUIRE(constant->type == ZR_VALUE_TYPE_FUNCTION && constant->isGarbageCollectable &&
            !constant->isNative && constant->value.object != ZR_NULL &&
            constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION && !constant->value.object->isNative,
            "Ordinary imported method must contain a VM function");
    candidate = (SZrFunction *)constant->value.object;
    IMPORTED_INSTANCE_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(candidate), &pins[4]),
            "Provider method must remain fixed");
    IMPORTED_INSTANCE_REQUIRE(definition->signatureHash != 0u &&
            definition->signatureHash == ZrCore_CallBinding_FunctionSignatureHash(candidate) &&
            entry->binding.contract.signatureHash == definition->signatureHash &&
            entry->binding.contract.signatureToken == definition->relatedToken &&
            definition->ownerToken != 0u && entry->binding.contract.ownerTypeToken == definition->ownerToken,
            "Real callable signature and owner must match the consumer contract");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < metadata->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &metadata->metadataTokenRecords[row];
        if (record->token == definition->ownerToken) { owner = record; ++matches; }
    }
    IMPORTED_INSTANCE_REQUIRE(matches == 1u && ZR_METADATA_TOKEN_TABLE(owner->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            owner->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER, "Callable must have one actual owner definition");
    layoutHash = ZrCore_CallBinding_PrototypeLayoutHash(metadata, owner->ownerIndex);
    IMPORTED_INSTANCE_REQUIRE(layoutHash != 0u && owner->signatureHash == layoutHash && definition->layoutHash == layoutHash &&
            entry->binding.contract.layoutHash == layoutHash && entry->binding.contract.layoutVersion == definition->layoutVersion &&
            metadata->prototypeInstances != ZR_NULL && owner->ownerIndex < metadata->prototypeInstancesLength,
            "Provider and consumer must agree on the actual owner layout");
    prototype = metadata->prototypeInstances[owner->ownerIndex];
    IMPORTED_INSTANCE_REQUIRE(prototype != ZR_NULL && prototype->super.super.type == ZR_RAW_OBJECT_TYPE_OBJECT &&
            !prototype->super.super.isNative && prototype->super.internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE &&
            ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype), &pins[5]), "Actual VM owner must stay fixed");
    IMPORTED_INSTANCE_REQUIRE(prototype->memberDescriptorCount <= prototype->memberDescriptorCapacity &&
            (prototype->memberDescriptorCount == 0u || prototype->memberDescriptors != ZR_NULL), "Owner descriptors must be valid");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < prototype->memberDescriptorCount; ++row) {
        if (prototype->memberDescriptors[row].methodFunction == candidate) {
            descriptor = &prototype->memberDescriptors[row]; descriptorIndex = row; ++matches;
        }
    }
    IMPORTED_INSTANCE_REQUIRE(matches == 1u && descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_METHOD &&
            !descriptor->isStatic && descriptor->accessorRole == 0u &&
            ZrTests_Fixture_StringEqualsCString(descriptor->name, "add"), "VM callable must be one ordinary instance descriptor");
    linked = ZrCore_CallBinding_LinkImportedModule(state, consumer, module, &diagnostic);
    fprintf(stderr, "[zr-test-imported-instance] linked=%u status=%u instruction=%u cache=%u operandCache=%u "
                    "opcode=%u name=%s targetKind=%u token=0x%08x moduleSig=%llu providerSig=%llu "
                    "signature=%llu owner=0x%08x layout=%llu descriptor=%u\n",
            (unsigned)linked, (unsigned)diagnostic.status, (unsigned)entry->instructionIndex,
            (unsigned)cacheIndex, (unsigned)operandCacheIndex, (unsigned)opcode, opcodeName,
            (unsigned)entry->binding.target.targetKind, (unsigned)definition->token,
            (unsigned long long)entry->binding.contract.moduleSignatureHash,
            (unsigned long long)metadata->moduleSignatureHash, (unsigned long long)definition->signatureHash,
            (unsigned)owner->token, (unsigned long long)layoutHash, (unsigned)descriptorIndex);
    IMPORTED_INSTANCE_REQUIRE(linked && entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM &&
            entry->binding.target.vm.function == candidate && entry->picSlotCount == 1u &&
            entry->picSlots[0].cachedOwnerPrototype == prototype && entry->picSlots[0].cachedFunction == candidate &&
            entry->picSlots[0].cachedDescriptorIndex == descriptorIndex && !entry->picSlots[0].cachedIsStatic &&
            entry->binding.target.ownerLayoutGeneration == prototype->layoutGeneration, "Public linking must bind the exact VM method and owner");
    IMPORTED_INSTANCE_REQUIRE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, consumer, &result),
            "Correctly lowered imported instance method must execute");
    IMPORTED_INSTANCE_REQUIRE(result == 42 && entry->runtimeHitCount > 0u &&
            entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM && entry->binding.target.vm.function == candidate &&
            entry->picSlots[0].cachedOwnerPrototype == prototype, "Instance call must return 42 through the exact bound VM target");
cleanup:
    for (TZrUInt32 pin = (TZrUInt32)(sizeof(pins) / sizeof(pins[0])); pin > 0u; --pin)
        ZrCore_Gc_NativeCallUnpin(state->global, &pins[pin - 1u]);
    provider = savedProvider;
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure);
}

#undef IMPORTED_INSTANCE_REQUIRE
