#ifndef ZR_TEST_AOT_C_DIRECT_INLINE_RETURN_LAYOUT_PROJECTION_CASES_H
#define ZR_TEST_AOT_C_DIRECT_INLINE_RETURN_LAYOUT_PROJECTION_CASES_H

#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/type_layout.h"

#define ZR_AOT_TEST_TYPE_LAYOUT_CACHE_READY ((TZrUInt8)2u)

#if defined(ZR_PLATFORM_UNIX)
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.h"
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.h"
#endif

static void test_aot_exec_ir_direct_inline_return_layout_accessor_isolates_raw_metadata(void) {
    SZrFunction rawFunction;
    SZrAotExecIrFunction functionIr;

    memset(&rawFunction, 0, sizeof(rawFunction));
    memset(&functionIr, 0, sizeof(functionIr));
    functionIr.function = &rawFunction;

    rawFunction.hasCallableReturnType = ZR_FALSE;
    functionIr.directInlineReturnLayoutKnown = ZR_TRUE;
    functionIr.directInlineReturnTypeLayoutId = 17u;
    TEST_ASSERT_EQUAL_UINT32(
            17u, backend_aot_exec_ir_direct_inline_return_type_layout_id(&functionIr));

    rawFunction.hasCallableReturnType = ZR_TRUE;
    rawFunction.callableReturnType.staticCType = ZR_STATIC_C_TYPE_STRUCT;
    rawFunction.callableReturnType.staticCTypeId = 29u;
    functionIr.directInlineReturnLayoutKnown = ZR_FALSE;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(&functionIr));

    functionIr.directInlineReturnLayoutKnown = (TZrBool)2u;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(&functionIr));

    functionIr.directInlineReturnLayoutKnown = ZR_TRUE;
    functionIr.directInlineReturnTypeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(&functionIr));
}

#if defined(ZR_PLATFORM_UNIX)
static SZrFunction *find_direct_inline_return_function(SZrFunction *function) {
    TZrUInt32 instructionIndex;
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    for (instructionIndex = 0u;
         function->semIrInstructions != ZR_NULL &&
         instructionIndex < function->semIrInstructionLength;
         instructionIndex++) {
        if (function->semIrInstructions[instructionIndex].opcode ==
            (TZrUInt32)ZR_SEMIR_OPCODE_RETURN_TYPED) {
            return function;
        }
    }
    for (childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        SZrFunction *found = find_direct_inline_return_function(
                &function->childFunctionList[childIndex]);

        if (found != ZR_NULL) {
            return found;
        }
    }
    return ZR_NULL;
}

static const SZrAotExecIrFunction *find_direct_inline_return_function_ir(
        const SZrAotExecIrModule *module,
        const SZrFunction *function) {
    TZrUInt32 functionIndex;

    if (module == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }
    for (functionIndex = 0u; functionIndex < module->functionCount; functionIndex++) {
        if (module->functions[functionIndex].function == function) {
            return &module->functions[functionIndex];
        }
    }
    return ZR_NULL;
}

static const SZrAotExecIrFrameSlotLayout *find_direct_inline_return_slot_layout(
        const SZrAotExecIrFrameLayout *frameLayout,
        TZrUInt32 stackSlot) {
    TZrUInt32 layoutIndex;

    if (frameLayout == ZR_NULL) {
        return ZR_NULL;
    }
    for (layoutIndex = 0u;
         frameLayout->slotLayouts != ZR_NULL &&
         layoutIndex < frameLayout->slotLayoutCount;
         layoutIndex++) {
        if (frameLayout->slotLayouts[layoutIndex].stackSlot == stackSlot) {
            return &frameLayout->slotLayouts[layoutIndex];
        }
    }
    return ZR_NULL;
}

static const char *direct_inline_return_projection_source(void) {
    return "struct DirectInlineReturnProjectionPointWithArtifactNameLongerThanTheShortStringInterningBoundaryForContentStableCallableAndSemanticIrIdentityProof {\n"
           "    pub var x: int;\n"
           "    pub var y: int;\n"
           "}\n"
           "fn choosePoint(left: DirectInlineReturnProjectionPointWithArtifactNameLongerThanTheShortStringInterningBoundaryForContentStableCallableAndSemanticIrIdentityProof, right: DirectInlineReturnProjectionPointWithArtifactNameLongerThanTheShortStringInterningBoundaryForContentStableCallableAndSemanticIrIdentityProof, seed: int): DirectInlineReturnProjectionPointWithArtifactNameLongerThanTheShortStringInterningBoundaryForContentStableCallableAndSemanticIrIdentityProof {\n"
           "    if (seed > 0) { return left; }\n"
           "    return right;\n"
           "}\n"
           "return 0;";
}

static void test_aot_exec_ir_projects_and_validates_direct_inline_return_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *root;
    SZrFunction *callee;
    SZrFunction *originalChildFunctionList;
    SZrFunction *expandedChildFunctionList;
    SZrFunction *unreachable;
    SZrAotExecIrModule module;
    const SZrAotExecIrFunction *calleeIr;
    TZrUInt32 expectedLayoutId;
    TZrUInt32 returnInstructionIndex = UINT32_MAX;
    TZrUInt32 secondReturnInstructionIndex = UINT32_MAX;
    TZrUInt32 originalTypeTableIndex;
    TZrUInt32 secondOriginalTypeTableIndex;
    TZrUInt32 originalReturnSourceSlot;
    EZrValueType originalBaseType;
    EZrStaticCType originalStaticCType;
    TZrUInt32 originalStaticCTypeId;
    TZrBool originalHasCallableReturnType;
    SZrFunctionTypedTypeRef *originalTypeTable;
    SZrFunctionTypedTypeRef *expandedTypeTable;
    TZrUInt32 originalTypeTableLength;
    SZrFunctionFrameSlotLayout *sourceFrameLayout = ZR_NULL;
    SZrFunctionFrameSlotLayout *secondSourceFrameLayout = ZR_NULL;
    SZrFunctionFrameSlotLayout *unreachableFrameSlotLayouts;
    SZrFunctionFrameSlotLayout *unreachableReturnSourceLayout = ZR_NULL;
    SZrSemIrInstruction *unreachableSemIrInstructions;
    TZrUInt16 originalSourceFlags;
    TZrUInt8 originalSourceSlotKind;
    TZrUInt32 secondOriginalByteAlign;
    SZrSemIrInstruction *originalSemIrInstructions;
    TZrUInt32 secondOriginalTypeLayoutId;
    SZrTypeLayout *returnTypeLayout;
    SZrTypeLayout savedReturnTypeLayout;
    SZrTypeLayout *originalTypeLayouts;
    SZrTypeLayout *expandedTypeLayouts;
    TZrUInt8 *originalTypeLayoutStates;
    TZrUInt8 *expandedTypeLayoutStates;
    TZrUInt32 originalPrototypeCount;
    TZrUInt32 originalTypeLayoutLength;
    TZrUInt32 originalChildFunctionLength;
    TZrUInt32 alternateTypeLayoutId;
    SZrAotWriterOptions writerOptions;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    root = compile_source(
            state, direct_inline_return_projection_source(), "direct_inline_return_projection.zr");
    TEST_ASSERT_NOT_NULL(root);
    callee = find_direct_inline_return_function(root);
    TEST_ASSERT_NOT_NULL(callee);
    TEST_ASSERT_EQUAL(ZR_TRUE, callee->hasCallableReturnType);
    for (TZrUInt32 index = 0u; index < callee->semIrInstructionLength; index++) {
        if (callee->semIrInstructions[index].opcode ==
            (TZrUInt32)ZR_SEMIR_OPCODE_RETURN_TYPED) {
            if (returnInstructionIndex == UINT32_MAX) {
                returnInstructionIndex = index;
            } else if (secondReturnInstructionIndex == UINT32_MAX) {
                secondReturnInstructionIndex = index;
            }
        }
    }
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, returnInstructionIndex);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, secondReturnInstructionIndex);

    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    expectedLayoutId = backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr);
    TEST_ASSERT_NOT_EQUAL(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, expectedLayoutId);
    for (TZrUInt32 index = 0u; index < calleeIr->instructionCount; index++) {
        const SZrAotExecIrInstruction *instruction =
                &module.instructions[calleeIr->firstInstructionOffset + index];
        const SZrAotExecIrFrameSlotLayout *sourceLayout;

        if (instruction->semIrOpcode != (TZrUInt32)ZR_SEMIR_OPCODE_RETURN_TYPED) {
            continue;
        }
        sourceLayout = find_direct_inline_return_slot_layout(
                &calleeIr->frameLayout, instruction->operand0);
        TEST_ASSERT_NOT_NULL(sourceLayout);
        TEST_ASSERT_EQUAL_UINT32(expectedLayoutId, sourceLayout->typeLayoutId);
    }
    backend_aot_exec_ir_release_module(state, &module);

    originalStaticCType =
            callee->semIrTypeTable[callee->semIrInstructions[returnInstructionIndex]
                                           .typeTableIndex]
                    .staticCType;
    originalStaticCTypeId =
            callee->semIrTypeTable[callee->semIrInstructions[returnInstructionIndex]
                                           .typeTableIndex]
                    .staticCTypeId;
    callee->semIrTypeTable[callee->semIrInstructions[returnInstructionIndex]
                                   .typeTableIndex]
            .staticCType = ZR_STATIC_C_TYPE_DYNAMIC;
    callee->semIrTypeTable[callee->semIrInstructions[returnInstructionIndex]
                                   .typeTableIndex]
            .staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr));
    backend_aot_exec_ir_release_module(state, &module);
    callee->semIrTypeTable[callee->semIrInstructions[returnInstructionIndex]
                                   .typeTableIndex]
            .staticCType = originalStaticCType;
    callee->semIrTypeTable[callee->semIrInstructions[returnInstructionIndex]
                                   .typeTableIndex]
            .staticCTypeId = originalStaticCTypeId;

    originalHasCallableReturnType = callee->hasCallableReturnType;
    callee->hasCallableReturnType = ZR_FALSE;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr));
    backend_aot_exec_ir_release_module(state, &module);
    callee->hasCallableReturnType = originalHasCallableReturnType;

    for (TZrUInt32 index = 0u; index < callee->frameSlotLayoutLength; index++) {
        if (callee->frameSlotLayouts[index].stackSlot ==
            callee->semIrInstructions[returnInstructionIndex].operand0) {
            sourceFrameLayout = &callee->frameSlotLayouts[index];
        }
        if (callee->frameSlotLayouts[index].stackSlot ==
            callee->semIrInstructions[secondReturnInstructionIndex].operand0) {
            secondSourceFrameLayout = &callee->frameSlotLayouts[index];
        }
    }
    TEST_ASSERT_NOT_NULL(sourceFrameLayout);
    TEST_ASSERT_NOT_NULL(secondSourceFrameLayout);

    originalChildFunctionList = root->childFunctionList;
    originalChildFunctionLength = root->childFunctionLength;
    TEST_ASSERT_EQUAL_UINT32(1u, originalChildFunctionLength);
    expandedChildFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*expandedChildFunctionList) *
                    (originalChildFunctionLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    unreachableFrameSlotLayouts =
            (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(*unreachableFrameSlotLayouts) *
                    callee->frameSlotLayoutLength,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    unreachableSemIrInstructions =
            (SZrSemIrInstruction *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(*unreachableSemIrInstructions) *
                            callee->semIrInstructionLength,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(expandedChildFunctionList);
    TEST_ASSERT_NOT_NULL(unreachableFrameSlotLayouts);
    TEST_ASSERT_NOT_NULL(unreachableSemIrInstructions);
    memcpy(expandedChildFunctionList,
           originalChildFunctionList,
           sizeof(*expandedChildFunctionList) * originalChildFunctionLength);
    unreachable = &expandedChildFunctionList[originalChildFunctionLength];
    *unreachable = *callee;
    unreachable->ownerFunction = root;
    unreachable->lineInSourceStart = callee->lineInSourceStart + 100u;
    unreachable->lineInSourceEnd = callee->lineInSourceEnd + 100u;
    memcpy(unreachableFrameSlotLayouts,
           callee->frameSlotLayouts,
           sizeof(*unreachableFrameSlotLayouts) *
                   callee->frameSlotLayoutLength);
    memcpy(unreachableSemIrInstructions,
           callee->semIrInstructions,
           sizeof(*unreachableSemIrInstructions) *
                   callee->semIrInstructionLength);
    unreachable->frameSlotLayouts = unreachableFrameSlotLayouts;
    unreachable->semIrInstructions = unreachableSemIrInstructions;
    for (TZrUInt32 index = 0u;
         index < unreachable->frameSlotLayoutLength;
         index++) {
        if (unreachable->frameSlotLayouts[index].stackSlot ==
            unreachable->semIrInstructions[returnInstructionIndex].operand0) {
            unreachableReturnSourceLayout =
                    &unreachable->frameSlotLayouts[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(unreachableReturnSourceLayout);
    root->childFunctionList = expandedChildFunctionList;
    root->childFunctionLength = originalChildFunctionLength + 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_method_info_signature",
            "direct_inline_return_unreachable_prefilter",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));
    memset(&writerOptions, 0, sizeof(writerOptions));
    writerOptions.moduleName = "direct_inline_return_unreachable_prefilter";
    writerOptions.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    writerOptions.enableCodeStripping = ZR_TRUE;
    writerOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, root, generatedCPath, &writerOptions));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_text_contains(
            generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(
            generatedCText, "/* code_stripping.functionsRemoved = 1 */");
    free(generatedCText);

    originalSemIrInstructions = unreachable->semIrInstructions;
    unreachable->semIrInstructions = ZR_NULL;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    unreachable->semIrInstructions = originalSemIrInstructions;

    originalReturnSourceSlot =
            unreachable->semIrInstructions[returnInstructionIndex].operand0;
    unreachable->semIrInstructions[returnInstructionIndex].operand0 = UINT32_MAX;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    unreachable->semIrInstructions[returnInstructionIndex].operand0 =
            originalReturnSourceSlot;

    originalSourceSlotKind = unreachableReturnSourceLayout->slotKind;
    unreachableReturnSourceLayout->slotKind =
            (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, root, generatedCPath, &writerOptions));
    unreachableReturnSourceLayout->slotKind = originalSourceSlotKind;
    root->childFunctionList = originalChildFunctionList;
    root->childFunctionLength = originalChildFunctionLength;
    ZrCore_Memory_RawFreeWithType(
            state->global,
            unreachableSemIrInstructions,
            sizeof(*unreachableSemIrInstructions) *
                    callee->semIrInstructionLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    ZrCore_Memory_RawFreeWithType(
            state->global,
            unreachableFrameSlotLayouts,
            sizeof(*unreachableFrameSlotLayouts) *
                    callee->frameSlotLayoutLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    ZrCore_Memory_RawFreeWithType(
            state->global,
            expandedChildFunctionList,
            sizeof(*expandedChildFunctionList) *
                    (originalChildFunctionLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);

    returnTypeLayout = (SZrTypeLayout *)ZrCore_Function_ResolvePrototypeFrameTypeLayout(
            callee, expectedLayoutId, state);
    TEST_ASSERT_NOT_NULL(returnTypeLayout);
    TEST_ASSERT_EQUAL_UINT32(
            root->prototypeCount, root->prototypeFrameTypeLayoutLength);
    originalPrototypeCount = root->prototypeCount;
    originalTypeLayoutLength = root->prototypeFrameTypeLayoutLength;
    originalTypeLayouts = root->prototypeFrameTypeLayouts;
    originalTypeLayoutStates = root->prototypeFrameTypeLayoutStates;
    alternateTypeLayoutId = originalPrototypeCount;
    expandedTypeLayouts = (SZrTypeLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*expandedTypeLayouts) * (originalTypeLayoutLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    expandedTypeLayoutStates = (TZrUInt8 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*expandedTypeLayoutStates) * (originalTypeLayoutLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(expandedTypeLayouts);
    TEST_ASSERT_NOT_NULL(expandedTypeLayoutStates);
    memcpy(expandedTypeLayouts,
           originalTypeLayouts,
           sizeof(*expandedTypeLayouts) * originalTypeLayoutLength);
    memcpy(expandedTypeLayoutStates,
           originalTypeLayoutStates,
           sizeof(*expandedTypeLayoutStates) * originalTypeLayoutLength);
    expandedTypeLayouts[alternateTypeLayoutId] = *returnTypeLayout;
    expandedTypeLayouts[alternateTypeLayoutId].cTypeId = alternateTypeLayoutId;
    expandedTypeLayouts[alternateTypeLayoutId].layoutHash =
            ZrCore_TypeLayout_ComputeHash(
                    &expandedTypeLayouts[alternateTypeLayoutId]);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(
            &expandedTypeLayouts[alternateTypeLayoutId]));
    expandedTypeLayoutStates[alternateTypeLayoutId] =
            ZR_AOT_TEST_TYPE_LAYOUT_CACHE_READY;

    originalTypeTable = callee->semIrTypeTable;
    originalTypeTableLength = callee->semIrTypeTableLength;
    secondOriginalTypeTableIndex =
            callee->semIrInstructions[secondReturnInstructionIndex].typeTableIndex;
    expandedTypeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*expandedTypeTable) * (originalTypeTableLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(expandedTypeTable);
    memcpy(expandedTypeTable,
           originalTypeTable,
           sizeof(*expandedTypeTable) * originalTypeTableLength);
    expandedTypeTable[originalTypeTableLength] =
            originalTypeTable[secondOriginalTypeTableIndex];
    expandedTypeTable[originalTypeTableLength].staticCTypeId =
            alternateTypeLayoutId;

    secondOriginalTypeLayoutId = secondSourceFrameLayout->typeLayoutId;
    root->prototypeCount = originalPrototypeCount + 1u;
    root->prototypeFrameTypeLayoutLength = originalTypeLayoutLength + 1u;
    root->prototypeFrameTypeLayouts = expandedTypeLayouts;
    root->prototypeFrameTypeLayoutStates = expandedTypeLayoutStates;
    callee->semIrTypeTable = expandedTypeTable;
    callee->semIrTypeTableLength = originalTypeTableLength + 1u;
    callee->semIrInstructions[secondReturnInstructionIndex].typeTableIndex =
            originalTypeTableLength;
    secondSourceFrameLayout->typeLayoutId = alternateTypeLayoutId;

    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr));
    backend_aot_exec_ir_release_module(state, &module);

    secondOriginalByteAlign =
            expandedTypeLayouts[alternateTypeLayoutId].byteAlign;
    TEST_ASSERT_GREATER_THAN_UINT32(
            1u, expandedTypeLayouts[alternateTypeLayoutId].byteAlign);
    expandedTypeLayouts[alternateTypeLayoutId].byteAlign /= 2u;
    expandedTypeLayouts[alternateTypeLayoutId].layoutHash =
            ZrCore_TypeLayout_ComputeHash(
                    &expandedTypeLayouts[alternateTypeLayoutId]);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(
            &expandedTypeLayouts[alternateTypeLayoutId]));
    secondSourceFrameLayout->byteAlign =
            expandedTypeLayouts[alternateTypeLayoutId].byteAlign;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    expandedTypeLayouts[alternateTypeLayoutId].byteAlign =
            secondOriginalByteAlign;
    expandedTypeLayouts[alternateTypeLayoutId].layoutHash =
            ZrCore_TypeLayout_ComputeHash(
                    &expandedTypeLayouts[alternateTypeLayoutId]);
    secondSourceFrameLayout->byteAlign = secondOriginalByteAlign;

    secondSourceFrameLayout->typeLayoutId = secondOriginalTypeLayoutId;
    callee->semIrInstructions[secondReturnInstructionIndex].typeTableIndex =
            secondOriginalTypeTableIndex;
    callee->semIrTypeTable = originalTypeTable;
    callee->semIrTypeTableLength = originalTypeTableLength;
    root->prototypeCount = originalPrototypeCount;
    root->prototypeFrameTypeLayoutLength = originalTypeLayoutLength;
    root->prototypeFrameTypeLayouts = originalTypeLayouts;
    root->prototypeFrameTypeLayoutStates = originalTypeLayoutStates;
    ZrCore_Memory_RawFreeWithType(
            state->global,
            expandedTypeTable,
            sizeof(*expandedTypeTable) * (originalTypeTableLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    ZrCore_Memory_RawFreeWithType(
            state->global,
            expandedTypeLayoutStates,
            sizeof(*expandedTypeLayoutStates) * (originalTypeLayoutLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    ZrCore_Memory_RawFreeWithType(
            state->global,
            expandedTypeLayouts,
            sizeof(*expandedTypeLayouts) * (originalTypeLayoutLength + 1u),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);

    originalSourceFlags = sourceFrameLayout->reserved0;
    sourceFrameLayout->reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            expectedLayoutId,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr));
    backend_aot_exec_ir_release_module(state, &module);
    sourceFrameLayout->reserved0 =
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr));
    backend_aot_exec_ir_release_module(state, &module);
    sourceFrameLayout->reserved0 = originalSourceFlags;

    savedReturnTypeLayout = *returnTypeLayout;
    ZrCore_TypeLayout_InitUnion(
            returnTypeLayout,
            savedReturnTypeLayout.byteSize,
            savedReturnTypeLayout.byteAlign,
            0u,
            1u,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    returnTypeLayout->cTypeId = expectedLayoutId;
    returnTypeLayout->layoutHash = ZrCore_TypeLayout_ComputeHash(returnTypeLayout);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(returnTypeLayout));
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, root, &module));
    calleeIr = find_direct_inline_return_function_ir(&module, callee);
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE,
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeIr));
    backend_aot_exec_ir_release_module(state, &module);
    *returnTypeLayout = savedReturnTypeLayout;

    originalTypeTableIndex =
            callee->semIrInstructions[returnInstructionIndex].typeTableIndex;
    callee->semIrInstructions[returnInstructionIndex].typeTableIndex =
            callee->semIrTypeTableLength;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    callee->semIrInstructions[returnInstructionIndex].typeTableIndex =
            originalTypeTableIndex;

    originalTypeTable = callee->semIrTypeTable;
    callee->semIrTypeTable = ZR_NULL;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    callee->semIrTypeTable = originalTypeTable;

    originalStaticCTypeId =
            callee->semIrTypeTable[originalTypeTableIndex].staticCTypeId;
    callee->semIrTypeTable[originalTypeTableIndex].staticCTypeId =
            originalStaticCTypeId + 1u;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    callee->semIrTypeTable[originalTypeTableIndex].staticCTypeId =
            originalStaticCTypeId;

    originalBaseType = callee->semIrTypeTable[originalTypeTableIndex].baseType;
    callee->semIrTypeTable[originalTypeTableIndex].baseType = ZR_VALUE_TYPE_BOOL;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_build_module(state, root, &module));
    callee->semIrTypeTable[originalTypeTableIndex].baseType = originalBaseType;

    ZrCore_Function_Free(state, root);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_value_semir_consumes_direct_inline_return_layout_sidecar(void) {
    SZrFunction rawFunction;
    SZrAotExecIrFunction functionIr;
    SZrAotExecIrFrameSlotLayout slots[2];
    SZrAotExecIrFrameLayout callerFrame;
    SZrAotExecIrInstruction callInstruction;
    SZrAotExecIrInstruction returnInstruction;
    SZrAotExecIrInstruction moduleInstructions[1];
    SZrAotExecIrModule execModule;
    FILE *file;

    memset(&rawFunction, 0, sizeof(rawFunction));
    memset(&functionIr, 0, sizeof(functionIr));
    memset(slots, 0, sizeof(slots));
    memset(&callerFrame, 0, sizeof(callerFrame));
    memset(&callInstruction, 0, sizeof(callInstruction));
    memset(&returnInstruction, 0, sizeof(returnInstruction));
    memset(moduleInstructions, 0, sizeof(moduleInstructions));
    memset(&execModule, 0, sizeof(execModule));

    rawFunction.hasCallableReturnType = ZR_TRUE;
    rawFunction.callableReturnType.staticCType = ZR_STATIC_C_TYPE_STRUCT;
    rawFunction.callableReturnType.staticCTypeId = 91u;
    functionIr.function = &rawFunction;
    functionIr.directInlineReturnLayoutKnown = ZR_TRUE;
    functionIr.directInlineReturnTypeLayoutId = 41u;

    slots[0].stackSlot = 0u;
    slots[0].byteOffset = 0u;
    slots[0].byteSize = 16u;
    slots[0].byteAlign = 8u;
    slots[0].typeLayoutId = 41u;
    slots[0].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    slots[1].stackSlot = 1u;
    slots[1].byteOffset = 16u;
    slots[1].byteSize = 16u;
    slots[1].byteAlign = 8u;
    slots[1].typeLayoutId = 41u;
    slots[1].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerFrame.generatedFrameSlotCount = 2u;
    callerFrame.slotLayoutCount = 2u;
    callerFrame.slotLayouts = slots;
    functionIr.frameLayout = callerFrame;

    callInstruction.destinationSlot = 0u;
    callInstruction.operand0 = 1u;
    callInstruction.operand1 = 0u;
    returnInstruction.operand0 = 1u;
    moduleInstructions[0].semIrOpcode =
            (TZrUInt32)ZR_SEMIR_OPCODE_RETURN_TYPED;
    moduleInstructions[0].operand0 = 1u;
    execModule.instructionCount = 1u;
    execModule.instructions = moduleInstructions;
    functionIr.firstInstructionOffset = 0u;
    functionIr.instructionCount = 1u;

    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(backend_aot_try_write_c_value_semir_call_typed_exec(
            file, &callerFrame, &callInstruction, &functionIr, 0u, 0u, 3u, ZR_TRUE));
    TEST_ASSERT_TRUE(backend_aot_try_write_c_value_semir_return_typed_exec(
            file, &functionIr, &returnInstruction, ZR_TRUE));
    TEST_ASSERT_TRUE(backend_aot_c_value_semir_needs_skip_drop_slot(
            &execModule, &functionIr));
    fclose(file);

    slots[0].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS;
    slots[1].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS;
    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(backend_aot_try_write_c_value_semir_call_typed_exec(
            file, &callerFrame, &callInstruction, &functionIr, 0u, 0u, 3u, ZR_TRUE));
    TEST_ASSERT_TRUE(backend_aot_try_write_c_value_semir_return_typed_exec(
            file, &functionIr, &returnInstruction, ZR_TRUE));
    TEST_ASSERT_TRUE(backend_aot_c_value_semir_needs_skip_drop_slot(
            &execModule, &functionIr));
    fclose(file);

    slots[0].reserved0 |= ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS;
    slots[1].reserved0 |= ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS;
    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_FALSE(backend_aot_try_write_c_value_semir_call_typed_exec(
            file, &callerFrame, &callInstruction, &functionIr, 0u, 0u, 3u, ZR_TRUE));
    TEST_ASSERT_FALSE(backend_aot_try_write_c_value_semir_return_typed_exec(
            file, &functionIr, &returnInstruction, ZR_TRUE));
    TEST_ASSERT_FALSE(backend_aot_c_value_semir_needs_skip_drop_slot(
            &execModule, &functionIr));
    fclose(file);
    slots[0].reserved0 = 0u;
    slots[1].reserved0 = 0u;

    functionIr.directInlineReturnLayoutKnown = ZR_FALSE;
    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_FALSE(backend_aot_try_write_c_value_semir_call_typed_exec(
            file, &callerFrame, &callInstruction, &functionIr, 0u, 0u, 3u, ZR_TRUE));
    TEST_ASSERT_FALSE(backend_aot_try_write_c_value_semir_return_typed_exec(
            file, &functionIr, &returnInstruction, ZR_TRUE));
    TEST_ASSERT_FALSE(backend_aot_c_value_semir_needs_skip_drop_slot(
            &execModule, &functionIr));
    fclose(file);

    functionIr.directInlineReturnLayoutKnown = ZR_TRUE;
    functionIr.directInlineReturnTypeLayoutId = 52u;
    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_FALSE(backend_aot_try_write_c_value_semir_call_typed_exec(
            file, &callerFrame, &callInstruction, &functionIr, 0u, 0u, 3u, ZR_TRUE));
    TEST_ASSERT_FALSE(backend_aot_try_write_c_value_semir_return_typed_exec(
            file, &functionIr, &returnInstruction, ZR_TRUE));
    TEST_ASSERT_FALSE(backend_aot_c_value_semir_needs_skip_drop_slot(
            &execModule, &functionIr));
    fclose(file);
}
#endif

#endif
