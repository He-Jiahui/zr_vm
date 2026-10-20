#include "compiler/compiler_native_call_binding.h"

void test_public_compile_native_callback_capture_escape_flags(void);

void test_public_compile_native_callback_capture_escape_flags(void) {
    static const TZrChar *kSource =
            "var ffiNative = import(\"zr.ffi\");\n"
            "pub fn makeNativeCallbackHandle() {\n"
            "    var callbackSeed = 2.0;\n"
            "    fn nativeCallback(value: float): float {\n"
            "        return value + callbackSeed;\n"
            "    }\n"
            "    var callbackHandle = ffiNative.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, nativeCallback);\n"
            "    return callbackHandle;\n"
            "}\n";
    static const TZrChar *kSourceName = "escape_native_member_capture_fixture.zr";
    SZrState *state = ZR_NULL;
    SZrFunction *root = ZR_NULL;
    SZrString *sourceName;
    const SZrFunction *factory;
    const SZrFunction *callback;
    const SZrFunctionLocalVariable *seed;
    const SZrFunctionLocalVariable *callable;
    const SZrFunctionLocalVariable *handle;
    const SZrFunctionClosureVariable *closure = ZR_NULL;
    const SZrFunctionEscapeBinding *capture;
    const SZrFunctionEscapeBinding *nativeBinding;
    const char *failure = ZR_NULL;
    TZrUInt32 closureIndex = 0u;
    TZrBool typedHandle = ZR_FALSE;
    TZrUInt32 expectedFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE |
                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE;

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        failure = "Failed to create native callback compile state";
        goto cleanup;
    }
    ZrParser_ToGlobalState_Register(state);
    if (!register_ffi_test_modules(state->global)) {
        failure = "Failed to register native callback compile providers";
        goto cleanup;
    }
    sourceName = ZrCore_String_Create(state, (TZrNativeString)kSourceName, strlen(kSourceName));
    if (sourceName == ZR_NULL) {
        failure = "Failed to create native callback source name";
        goto cleanup;
    }
    root = ZrParser_Source_Compile(state, kSource, strlen(kSource), sourceName);
    if (root == ZR_NULL) {
        failure = "Public native callback source compilation failed";
        goto cleanup;
    }
    factory = find_named_function_recursive(root, "makeNativeCallbackHandle");
    callback = find_named_function_recursive(root, "nativeCallback");
    if (factory == ZR_NULL || callback == ZR_NULL) {
        failure = "Public native callback compile omitted fixture functions";
        goto cleanup;
    }
    seed = find_local_variable_by_name(factory, "callbackSeed");
    callable = find_local_variable_by_name(factory, "nativeCallback");
    handle = find_local_variable_by_name(factory, "callbackHandle");

    for (TZrUInt32 index = 0u; factory->instructionsList != ZR_NULL && index < factory->instructionsLength; index++) {
        const TZrInstruction *instruction = &factory->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        printf("native callback instruction=%u opcode=%u resultSlot=%u A1=%u B1=%u\n",
               (unsigned)index, (unsigned)opcode, (unsigned)instruction->instruction.operandExtra,
               (unsigned)instruction->instruction.operand.operand1[0],
               (unsigned)instruction->instruction.operand.operand1[1]);
        for (TZrUInt32 cacheIndex = 0u; factory->callSiteCaches != ZR_NULL && cacheIndex < factory->callSiteCacheLength; cacheIndex++) {
            const SZrFunctionCallSiteCacheEntry *cache = &factory->callSiteCaches[cacheIndex];
            const SZrCallBindingContract *contract = &cache->binding.contract;
            if (cache->instructionIndex != index || contract->operation != ZR_CALL_BINDING_OPERATION_CALL) continue;
            printf("native callback callCache=%u kind=%u instruction=%u arguments=%u location=%u targetIndex=%u ownerDepth=%u flags=%u bindingKind=%u operation=%u\n",
                   (unsigned)cacheIndex, (unsigned)cache->kind, (unsigned)cache->instructionIndex,
                   (unsigned)cache->argumentCount, (unsigned)cache->bindingLocation.kind,
                   (unsigned)cache->bindingLocation.targetIndex, (unsigned)cache->bindingLocation.ownerDepth,
                   (unsigned)cache->bindingLocation.flags, (unsigned)contract->bindingKind, (unsigned)contract->operation);
            printf("native callback callCache=%u targetToken=0x%08x signatureToken=0x%08x ownerToken=0x%08x signatureHash=0x%016llx moduleHash=0x%016llx layoutHash=0x%016llx layoutVersion=%u dispatchSlot=%u checkContract=%u providerContract=%u\n",
                   (unsigned)cacheIndex, (unsigned)contract->targetMetadataToken,
                   (unsigned)contract->signatureToken, (unsigned)contract->ownerTypeToken,
                   (unsigned long long)contract->signatureHash, (unsigned long long)contract->moduleSignatureHash,
                   (unsigned long long)contract->layoutHash, (unsigned)contract->layoutVersion,
                   (unsigned)contract->dispatchSlot,
                   (unsigned)ZrCore_CallBinding_CheckContract(contract, ZR_NULL),
                   (unsigned)compiler_native_call_binding_is_provider_contract(contract));
        }
        if (opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8)) {
            TZrUInt32 cacheIndex = opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL)
                    ? instruction->instruction.operand.operand1[0]
                    : instruction->instruction.operand.operand0[0];
            if (factory->callSiteCaches != ZR_NULL && cacheIndex < factory->callSiteCacheLength) {
                const SZrFunctionCallSiteCacheEntry *cache = &factory->callSiteCaches[cacheIndex];
                printf("native callback memberCache=%u kind=%u memberEntry=%u arguments=%u bindingKind=%u\n",
                       (unsigned)cacheIndex, (unsigned)cache->kind, (unsigned)cache->memberEntryIndex,
                       (unsigned)cache->argumentCount, (unsigned)cache->binding.contract.bindingKind);
            } else {
                printf("native callback memberCache=%u unavailable\n", (unsigned)cacheIndex);
            }
        }
    }
    for (TZrUInt32 index = 0u; factory->typedLocalBindings != ZR_NULL && index < factory->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &factory->typedLocalBindings[index];
        const TZrChar *typeName = binding->type.typeName != ZR_NULL
                ? ZrCore_String_GetNativeString(binding->type.typeName) : ZR_NULL;
        printf("native callback typedLocal slot=%u type=%s\n", (unsigned)binding->stackSlot,
               typeName != ZR_NULL ? typeName : "<none>");
        if (handle != ZR_NULL && binding->stackSlot == handle->stackSlot &&
            ZrTests_Fixture_StringEqualsCString(binding->type.typeName, "CallbackHandle")) {
            typedHandle = ZR_TRUE;
        }
    }
    for (TZrUInt32 index = 0u; callback->closureValueList != ZR_NULL && index < callback->closureValueLength; index++) {
        if (ZrTests_Fixture_StringEqualsCString(callback->closureValueList[index].name, "callbackSeed")) {
            closure = &callback->closureValueList[index];
            closureIndex = index;
            break;
        }
    }
    capture = find_escape_binding(callback, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "callbackSeed");
    nativeBinding = callable != ZR_NULL
            ? find_escape_binding_by_slot(factory, ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING, callable->stackSlot)
            : ZR_NULL;
    printf("native callback typedCallbackHandle=%u seedFlags=%u closureFlags=%u captureFlags=%u nativeFlags=%u\n",
           (unsigned)typedHandle, seed != ZR_NULL ? (unsigned)seed->escapeFlags : 0u,
           closure != ZR_NULL ? (unsigned)closure->escapeFlags : 0u,
           capture != ZR_NULL ? (unsigned)capture->escapeFlags : 0u,
           nativeBinding != ZR_NULL ? (unsigned)nativeBinding->escapeFlags : 0u);

    if (seed == ZR_NULL || callable == ZR_NULL || handle == ZR_NULL) {
        failure = "Public native callback compile omitted fixture locals";
    } else if (closure == ZR_NULL || capture == ZR_NULL || !closure->inStack ||
               closure->index != seed->stackSlot || capture->slotOrIndex != closureIndex) {
        failure = "Public native callback capture does not reference its parent seed";
    } else if ((seed->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) == 0u) {
        failure = "Public native callback seed lacks CLOSURE_CAPTURE";
    } else if ((seed->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) == 0u) {
        failure = "Public native callback seed lacks NATIVE_HANDLE";
    } else if ((closure->escapeFlags & expectedFlags) != expectedFlags ||
               (capture->escapeFlags & expectedFlags) != expectedFlags) {
        failure = "Public native callback closure metadata lacks native capture flags";
    } else if (nativeBinding == ZR_NULL ||
               (nativeBinding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) == 0u) {
        failure = "Public native callback local lacks NATIVE_BINDING";
    } else if (!typedHandle) {
        failure = "Public native callback result lacks typed CallbackHandle binding";
    }

cleanup:
    fflush(stdout);
    if (root != ZR_NULL) {
        ZrCore_Function_Free(state, root);
    }
    if (state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
    }
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure != ZR_NULL ? failure : "Native callback escape metadata");
}

void test_public_compile_native_callback_nominal_metadata_respects_parent_shadowing(void);

void test_public_compile_native_callback_nominal_metadata_respects_parent_shadowing(void) {
    static const TZrChar *kSource =
            "var ffiNative = import(\"zr.ffi\");\n"
            "var callbackHandle = 3.14;\n"
            "pub fn makeNativeCallbackHandle() {\n"
            "    var callbackSeed = 2.0;\n"
            "    fn nativeCallback(value: float): float {\n"
            "        return value + callbackSeed;\n"
            "    }\n"
            "    var callbackHandle = ffiNative.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, nativeCallback);\n"
            "    return callbackHandle;\n"
            "}\n";
    SZrState *state = ZR_NULL;
    SZrFunction *root = ZR_NULL;
    SZrString *sourceName;
    const SZrFunction *factory;
    const SZrFunction *callback;
    const SZrFunctionLocalVariable *parentHandle;
    const SZrFunctionLocalVariable *childHandle;
    const SZrFunctionLocalVariable *seed;
    const SZrFunctionLocalVariable *callable;
    const SZrFunctionEscapeBinding *capture;
    const SZrFunctionEscapeBinding *nativeBinding;
    SZrFunctionTypedLocalBinding parentBinding = {0};
    SZrFunctionTypedLocalBinding childBinding = {0};
    const char *failure = ZR_NULL;
    TZrBool parentFound = ZR_FALSE;
    TZrBool childFound = ZR_FALSE;
    TZrBool parentNameMatches = ZR_FALSE;
    TZrBool childNameMatches = ZR_FALSE;
    TZrBool parentTypeNameMissing = ZR_FALSE;
    TZrBool childTypeNameMatches = ZR_FALSE;
    TZrBool providerContractValid = ZR_FALSE;
    TZrBool providerLocationModule = ZR_FALSE;
    TZrBool providerInstructionValid = ZR_FALSE;
    TZrUInt32 providerCount = 0u;
    TZrUInt32 providerInstruction = 0u;
    TZrUInt32 providerOpcode = 0u;
    TZrUInt32 providerArguments = 0u;
    TZrUInt32 providerTargetToken = 0u;
    TZrUInt32 seedFlags = 0u;
    TZrUInt32 captureFlags = 0u;
    TZrUInt32 nativeFlags = 0u;

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        failure = "Failed to create parent-shadow callback compile state";
        goto cleanup;
    }
    ZrParser_ToGlobalState_Register(state);
    if (!register_ffi_test_modules(state->global)) {
        failure = "Failed to register parent-shadow callback providers";
        goto cleanup;
    }
    sourceName = ZrCore_String_CreateFromNative(state, "escape_native_callback_parent_shadow.zr");
    if (sourceName == ZR_NULL) {
        failure = "Failed to create parent-shadow callback source name";
        goto cleanup;
    }
    root = ZrParser_Source_Compile(state, kSource, strlen(kSource), sourceName);
    if (root == ZR_NULL) {
        failure = "Public parent-shadow callback source compilation failed";
        goto cleanup;
    }
    factory = find_named_function_recursive(root, "makeNativeCallbackHandle");
    callback = find_named_function_recursive(root, "nativeCallback");
    if (factory == ZR_NULL || callback == ZR_NULL) {
        failure = "Parent-shadow callback compile omitted fixture functions";
        goto cleanup;
    }
    parentHandle = find_local_variable_by_name(root, "callbackHandle");
    childHandle = find_local_variable_by_name(factory, "callbackHandle");
    seed = find_local_variable_by_name(factory, "callbackSeed");
    callable = find_local_variable_by_name(factory, "nativeCallback");
    if (parentHandle == ZR_NULL || childHandle == ZR_NULL || seed == ZR_NULL || callable == ZR_NULL) {
        failure = "Parent-shadow callback compile omitted fixture locals";
        goto cleanup;
    }
    for (TZrUInt32 index = 0u; root->typedLocalBindings != ZR_NULL && index < root->typedLocalBindingLength; index++) {
        if (root->typedLocalBindings[index].stackSlot == parentHandle->stackSlot) {
            parentBinding = root->typedLocalBindings[index];
            parentFound = ZR_TRUE;
            break;
        }
    }
    for (TZrUInt32 index = 0u; factory->typedLocalBindings != ZR_NULL && index < factory->typedLocalBindingLength; index++) {
        if (factory->typedLocalBindings[index].stackSlot == childHandle->stackSlot) {
            childBinding = factory->typedLocalBindings[index];
            childFound = ZR_TRUE;
            break;
        }
    }
    parentNameMatches = ZrTests_Fixture_StringEqualsCString(parentBinding.name, "callbackHandle");
    childNameMatches = ZrTests_Fixture_StringEqualsCString(childBinding.name, "callbackHandle");
    parentTypeNameMissing = (TZrBool)(parentBinding.type.typeName == ZR_NULL);
    childTypeNameMatches = ZrTests_Fixture_StringEqualsCString(childBinding.type.typeName, "CallbackHandle");

    for (TZrUInt32 index = 0u; factory->callSiteCaches != ZR_NULL && index < factory->callSiteCacheLength; index++) {
        const SZrFunctionCallSiteCacheEntry *cache = &factory->callSiteCaches[index];
        const SZrCallBindingContract *contract = &cache->binding.contract;
        if (contract->operation != ZR_CALL_BINDING_OPERATION_CALL ||
            !compiler_native_call_binding_is_provider_contract(contract)) {
            continue;
        }
        providerCount++;
        if (providerCount == 1u) {
            providerContractValid = (TZrBool)(ZrCore_CallBinding_CheckContract(contract, ZR_NULL) == ZR_CALL_BINDING_OK);
            providerLocationModule = (TZrBool)(cache->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE);
            providerInstruction = cache->instructionIndex;
            providerInstructionValid = (TZrBool)(factory->instructionsList != ZR_NULL &&
                                                providerInstruction < factory->instructionsLength);
            if (providerInstructionValid) {
                providerOpcode = factory->instructionsList[providerInstruction].instruction.operationCode;
            }
            providerArguments = cache->argumentCount;
            providerTargetToken = contract->targetMetadataToken;
        }
    }
    capture = find_escape_binding(callback, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "callbackSeed");
    nativeBinding = find_escape_binding_by_slot(factory, ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING,
                                               callable->stackSlot);
    seedFlags = seed->escapeFlags;
    captureFlags = capture != ZR_NULL ? capture->escapeFlags : 0u;
    nativeFlags = nativeBinding != ZR_NULL ? nativeBinding->escapeFlags : 0u;
    printf("native callback shadow parent found=%u name=%u symbol=%u type=%u place=%u base=%u typeNameMissing=%u\n",
           (unsigned)parentFound, (unsigned)parentNameMatches, (unsigned)parentBinding.symbolId,
           (unsigned)parentBinding.typeId, (unsigned)parentBinding.placeId,
           (unsigned)parentBinding.type.baseType, (unsigned)parentTypeNameMissing);
    printf("native callback shadow child found=%u name=%u symbol=%u type=%u place=%u base=%u typeName=%s\n",
           (unsigned)childFound, (unsigned)childNameMatches, (unsigned)childBinding.symbolId,
           (unsigned)childBinding.typeId, (unsigned)childBinding.placeId,
           (unsigned)childBinding.type.baseType, childBinding.type.typeName != ZR_NULL
                   ? ZrCore_String_GetNativeString(childBinding.type.typeName) : "<none>");
    printf("native callback shadow providers=%u contract=%u module=%u instruction=%u valid=%u opcode=%u arguments=%u target=0x%08x seedFlags=%u captureFlags=%u nativeFlags=%u\n",
           (unsigned)providerCount, (unsigned)providerContractValid, (unsigned)providerLocationModule,
           (unsigned)providerInstruction, (unsigned)providerInstructionValid, (unsigned)providerOpcode,
           (unsigned)providerArguments, (unsigned)providerTargetToken,
           (unsigned)seedFlags, (unsigned)captureFlags, (unsigned)nativeFlags);

cleanup:
    fflush(stdout);
    if (root != ZR_NULL) {
        ZrCore_Function_Free(state, root);
    }
    if (state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
    }
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure != ZR_NULL ? failure : "Parent-shadow callback metadata");
    TEST_ASSERT_TRUE_MESSAGE(parentFound && childFound, "Both callbackHandle typed rows must exist");
    TEST_ASSERT_TRUE_MESSAGE(parentNameMatches && childNameMatches, "Both typed rows must name callbackHandle");
    TEST_ASSERT_GREATER_THAN_UINT32(0u, parentBinding.symbolId);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, parentBinding.typeId);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, parentBinding.placeId);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, childBinding.symbolId);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, childBinding.typeId);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, childBinding.placeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(parentBinding.symbolId, childBinding.symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(parentBinding.typeId, childBinding.typeId);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_VALUE_TYPE_DOUBLE, parentBinding.type.baseType,
                                  "The parent callbackHandle must retain DOUBLE metadata");
    TEST_ASSERT_TRUE_MESSAGE(parentTypeNameMissing, "The parent DOUBLE row must retain its primitive type ref");
    TEST_ASSERT_GREATER_THAN_UINT32(0u, providerCount);
    TEST_ASSERT_TRUE_MESSAGE(providerContractValid && providerLocationModule && providerInstructionValid,
                             "The callback must retain its real valid native provider call record");
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE, seedFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE);
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE, captureFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_VALUE_TYPE_OBJECT, childBinding.type.baseType,
                                  "The child callbackHandle must retain its own nominal OBJECT type");
    TEST_ASSERT_TRUE_MESSAGE(childTypeNameMatches, "The child callbackHandle must retain CallbackHandle metadata");
}
