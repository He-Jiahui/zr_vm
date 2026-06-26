#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "compiler/compiler.h"
#include "compiler/compiler_aot.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/generic_instantiation.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}

void tearDown(void) {}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL(strstr(text, needle));
}

static void assert_text_not_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL(strstr(text, needle));
}

static TZrInstruction create_instruction_2(EZrInstructionCode opcode,
                                           TZrUInt16 operandExtra,
                                           TZrUInt16 operandA,
                                           TZrUInt16 operandB) {
    TZrInstruction instruction;

    instruction.value = 0u;
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static void write_u32_le(TZrByte *buffer, TZrSize offset, TZrUInt32 value) {
    buffer[offset + 0u] = (TZrByte)(value & 0xFFu);
    buffer[offset + 1u] = (TZrByte)((value >> 8u) & 0xFFu);
    buffer[offset + 2u] = (TZrByte)((value >> 16u) & 0xFFu);
    buffer[offset + 3u] = (TZrByte)((value >> 24u) & 0xFFu);
}

static SZrFunction *create_preserve_method_fixture(SZrState *state,
                                                   const TZrChar *usedName,
                                                   const TZrChar *keptName) {
    SZrFunction *root;
    SZrFunctionTopLevelCallableBinding *bindings;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
    root->instructionsLength = 1u;
    root->stackSize = 1u;
    root->parameterCount = 0u;
    root->lineInSourceStart = 1u;
    root->lineInSourceEnd = 1u;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction) * 2u);
    root->childFunctionLength = 2u;

    root->childFunctionList[0].functionName = ZrCore_String_CreateFromNative(state, (TZrChar *)usedName);
    root->childFunctionList[0].parameterCount = 0u;
    root->childFunctionList[0].stackSize = 1u;
    root->childFunctionList[0].lineInSourceStart = 10u;
    root->childFunctionList[0].lineInSourceEnd = 10u;

    root->childFunctionList[1].functionName = ZrCore_String_CreateFromNative(state, (TZrChar *)keptName);
    root->childFunctionList[1].parameterCount = 0u;
    root->childFunctionList[1].stackSize = 1u;
    root->childFunctionList[1].lineInSourceStart = 20u;
    root->childFunctionList[1].lineInSourceEnd = 20u;

    bindings = (SZrFunctionTopLevelCallableBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTopLevelCallableBinding) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(bindings);
    memset(bindings, 0, sizeof(SZrFunctionTopLevelCallableBinding) * 2u);
    bindings[0].name = ZrCore_String_CreateFromNative(state, (TZrChar *)usedName);
    bindings[0].callableChildIndex = 0u;
    bindings[0].exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
    bindings[1].name = ZrCore_String_CreateFromNative(state, (TZrChar *)keptName);
    bindings[1].callableChildIndex = 1u;
    bindings[1].exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
    root->topLevelCallableBindings = bindings;
    root->topLevelCallableBindingLength = 2u;

    TEST_ASSERT_NOT_NULL(root->childFunctionList[0].functionName);
    TEST_ASSERT_NOT_NULL(root->childFunctionList[1].functionName);
    TEST_ASSERT_NOT_NULL(bindings[0].name);
    TEST_ASSERT_NOT_NULL(bindings[1].name);
    return root;
}

static void attach_list_foo_type_spec_metadata(SZrState *state, SZrFunction *function) {
    const TZrUInt32 listStringIndex = 10u;
    const TZrUInt32 fooStringIndex = 11u;
    const TZrUInt32 typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    const TZrUInt32 signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrUInt64 signatureHash = (TZrUInt64)0x123456789ABCDEF0ULL;
    const TZrSize signatureLength = 1u + 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) +
                                    sizeof(TZrUInt32) + 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    TZrSize cursor = 0u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    function->metadataStringHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->signatureBlobHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            signatureLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);

    memset(function->metadataStringHeap, 0, sizeof(SZrMetadataStringHeapEntry) * 2u);
    memset(function->signatureBlobHeap, 0, signatureLength);
    memset(function->metadataTokenRecords, 0, sizeof(SZrMetadataTokenRecord) * 2u);
    function->metadataStringHeapLength = 2u;
    function->signatureBlobHeapLength = (TZrUInt32)signatureLength;
    function->metadataTokenRecordLength = 2u;

    function->metadataStringHeap[0].stringIndex = listStringIndex;
    function->metadataStringHeap[0].value = ZrCore_String_CreateFromNative(state, "List");
    function->metadataStringHeap[1].stringIndex = fooStringIndex;
    function->metadataStringHeap[1].value = ZrCore_String_CreateFromNative(state, "Foo");
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[0].value);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[1].value);

    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, listStringIndex);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, 1u);
    cursor += sizeof(TZrUInt32);
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, fooStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(signatureLength, cursor);

    function->metadataTokenRecords[0].token = typeSpecToken;
    function->metadataTokenRecords[0].relatedToken = signatureToken;
    function->metadataTokenRecords[0].signatureBlobOffset = 0u;
    function->metadataTokenRecords[0].signatureBlobLength = (TZrUInt32)signatureLength;
    function->metadataTokenRecords[0].signatureHash = signatureHash;
    function->metadataTokenRecords[1].token = signatureToken;
    function->metadataTokenRecords[1].relatedToken = typeSpecToken;
    function->metadataTokenRecords[1].ownerToken = typeSpecToken;
    function->metadataTokenRecords[1].signatureBlobOffset = 0u;
    function->metadataTokenRecords[1].signatureBlobLength = (TZrUInt32)signatureLength;
    function->metadataTokenRecords[1].signatureHash = signatureHash;
}

static void attach_list_type_ref_metadata(SZrState *state, SZrFunction *function) {
    const TZrUInt32 listStringIndex = 10u;
    const TZrUInt32 fooStringIndex = 11u;
    const TZrUInt32 typeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    const TZrUInt32 typeRefSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrUInt64 typeRefSignatureHash = (TZrUInt64)0x0ABCDEF012345678ULL;
    const TZrSize typeRefSignatureLength = 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    TZrSize cursor = 0u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    function->metadataStringHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->signatureBlobHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            typeRefSignatureLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);

    memset(function->metadataStringHeap, 0, sizeof(SZrMetadataStringHeapEntry) * 2u);
    memset(function->signatureBlobHeap, 0, typeRefSignatureLength);
    memset(function->metadataTokenRecords, 0, sizeof(SZrMetadataTokenRecord) * 2u);
    function->metadataStringHeapLength = 2u;
    function->signatureBlobHeapLength = (TZrUInt32)typeRefSignatureLength;
    function->metadataTokenRecordLength = 2u;

    function->metadataStringHeap[0].stringIndex = listStringIndex;
    function->metadataStringHeap[0].value = ZrCore_String_CreateFromNative(state, "List");
    function->metadataStringHeap[1].stringIndex = fooStringIndex;
    function->metadataStringHeap[1].value = ZrCore_String_CreateFromNative(state, "Foo");
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[0].value);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[1].value);

    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, listStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(typeRefSignatureLength, cursor);

    function->metadataTokenRecords[0].token = typeRefToken;
    function->metadataTokenRecords[0].relatedToken = typeRefSignatureToken;
    function->metadataTokenRecords[0].signatureBlobOffset = 0u;
    function->metadataTokenRecords[0].signatureBlobLength = (TZrUInt32)typeRefSignatureLength;
    function->metadataTokenRecords[0].signatureHash = typeRefSignatureHash;
    function->metadataTokenRecords[1].token = typeRefSignatureToken;
    function->metadataTokenRecords[1].relatedToken = typeRefToken;
    function->metadataTokenRecords[1].ownerToken = typeRefToken;
    function->metadataTokenRecords[1].signatureBlobOffset = 0u;
    function->metadataTokenRecords[1].signatureBlobLength = (TZrUInt32)typeRefSignatureLength;
    function->metadataTokenRecords[1].signatureHash = typeRefSignatureHash;
}

static void attach_list_foo_type_ref_and_type_spec_metadata(SZrState *state, SZrFunction *function) {
    const TZrUInt32 listStringIndex = 10u;
    const TZrUInt32 fooStringIndex = 11u;
    const TZrUInt32 typeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    const TZrUInt32 typeRefSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrUInt64 typeRefSignatureHash = (TZrUInt64)0x0ABCDEF012345678ULL;
    const TZrUInt32 typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    const TZrUInt32 typeSpecSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u);
    const TZrUInt64 typeSpecSignatureHash = (TZrUInt64)0x123456789ABCDEF0ULL;
    const TZrSize typeRefSignatureLength = 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    const TZrSize typeSpecSignatureLength = 1u + 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) +
                                            sizeof(TZrUInt32) + 1u + sizeof(TZrUInt32) +
                                            sizeof(TZrUInt32);
    const TZrSize signatureLength = typeRefSignatureLength + typeSpecSignatureLength;
    TZrSize cursor = 0u;
    TZrSize typeSpecSignatureOffset;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    function->metadataStringHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->signatureBlobHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            signatureLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);

    memset(function->metadataStringHeap, 0, sizeof(SZrMetadataStringHeapEntry) * 2u);
    memset(function->signatureBlobHeap, 0, signatureLength);
    memset(function->metadataTokenRecords, 0, sizeof(SZrMetadataTokenRecord) * 4u);
    function->metadataStringHeapLength = 2u;
    function->signatureBlobHeapLength = (TZrUInt32)signatureLength;
    function->metadataTokenRecordLength = 4u;

    function->metadataStringHeap[0].stringIndex = listStringIndex;
    function->metadataStringHeap[0].value = ZrCore_String_CreateFromNative(state, "List");
    function->metadataStringHeap[1].stringIndex = fooStringIndex;
    function->metadataStringHeap[1].value = ZrCore_String_CreateFromNative(state, "Foo");
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[0].value);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[1].value);

    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, listStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(typeRefSignatureLength, cursor);

    typeSpecSignatureOffset = cursor;
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, listStringIndex);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, 1u);
    cursor += sizeof(TZrUInt32);
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, fooStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(signatureLength, cursor);

    function->metadataTokenRecords[0].token = typeRefToken;
    function->metadataTokenRecords[0].relatedToken = typeRefSignatureToken;
    function->metadataTokenRecords[0].signatureBlobOffset = 0u;
    function->metadataTokenRecords[0].signatureBlobLength = (TZrUInt32)typeRefSignatureLength;
    function->metadataTokenRecords[0].signatureHash = typeRefSignatureHash;
    function->metadataTokenRecords[1].token = typeRefSignatureToken;
    function->metadataTokenRecords[1].relatedToken = typeRefToken;
    function->metadataTokenRecords[1].ownerToken = typeRefToken;
    function->metadataTokenRecords[1].signatureBlobOffset = 0u;
    function->metadataTokenRecords[1].signatureBlobLength = (TZrUInt32)typeRefSignatureLength;
    function->metadataTokenRecords[1].signatureHash = typeRefSignatureHash;
    function->metadataTokenRecords[2].token = typeSpecToken;
    function->metadataTokenRecords[2].relatedToken = typeSpecSignatureToken;
    function->metadataTokenRecords[2].signatureBlobOffset = (TZrUInt32)typeSpecSignatureOffset;
    function->metadataTokenRecords[2].signatureBlobLength = (TZrUInt32)typeSpecSignatureLength;
    function->metadataTokenRecords[2].signatureHash = typeSpecSignatureHash;
    function->metadataTokenRecords[3].token = typeSpecSignatureToken;
    function->metadataTokenRecords[3].relatedToken = typeSpecToken;
    function->metadataTokenRecords[3].ownerToken = typeSpecToken;
    function->metadataTokenRecords[3].signatureBlobOffset = (TZrUInt32)typeSpecSignatureOffset;
    function->metadataTokenRecords[3].signatureBlobLength = (TZrUInt32)typeSpecSignatureLength;
    function->metadataTokenRecords[3].signatureHash = typeSpecSignatureHash;
}

static void attach_generic_method_spec_metadata(SZrState *state, SZrFunction *function) {
    const TZrUInt32 fooStringIndex = 11u;
    const TZrUInt32 methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrUInt32 methodSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrUInt32 methodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u);
    const TZrUInt64 methodSpecHash = (TZrUInt64)0x2233445566778899ULL;
    const TZrSize methodSpecLength = 1u + 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) +
                                     1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    TZrSize cursor = 0u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    function->metadataStringHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->signatureBlobHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            methodSpecLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_NOT_NULL(function->typedExportedSymbols);

    memset(function->metadataStringHeap, 0, sizeof(SZrMetadataStringHeapEntry));
    memset(function->signatureBlobHeap, 0, methodSpecLength);
    memset(function->metadataTokenRecords, 0, sizeof(SZrMetadataTokenRecord));
    memset(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->metadataStringHeapLength = 1u;
    function->signatureBlobHeapLength = (TZrUInt32)methodSpecLength;
    function->metadataTokenRecordLength = 1u;
    function->typedExportedSymbolLength = 1u;

    function->metadataStringHeap[0].stringIndex = fooStringIndex;
    function->metadataStringHeap[0].value = ZrCore_String_CreateFromNative(state, "Foo");
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[0].value);

    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(function->signatureBlobHeap, cursor, methodToken);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, 1u);
    cursor += sizeof(TZrUInt32);
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, fooStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(methodSpecLength, cursor);

    function->metadataTokenRecords[0].token = methodSpecToken;
    function->metadataTokenRecords[0].relatedToken = methodToken;
    function->metadataTokenRecords[0].ownerToken = methodToken;
    function->metadataTokenRecords[0].signatureBlobOffset = 0u;
    function->metadataTokenRecords[0].signatureBlobLength = (TZrUInt32)methodSpecLength;
    function->metadataTokenRecords[0].signatureHash = methodSpecHash;

    function->typedExportedSymbols[0].name = ZrCore_String_CreateFromNative(state, "Factory.make");
    function->typedExportedSymbols[0].symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    function->typedExportedSymbols[0].metadataToken = methodToken;
    function->typedExportedSymbols[0].signatureToken = methodSignatureToken;
    TEST_ASSERT_NOT_NULL(function->typedExportedSymbols[0].name);
}

static void attach_list_foo_type_def_and_type_spec_metadata(SZrState *state, SZrFunction *function) {
    const TZrUInt32 listStringIndex = 10u;
    const TZrUInt32 fooStringIndex = 11u;
    const TZrUInt32 typeDefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrUInt32 typeDefSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrUInt64 typeDefSignatureHash = (TZrUInt64)0x0ABCDEF012345678ULL;
    const TZrUInt32 typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    const TZrUInt32 typeSpecSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u);
    const TZrUInt64 typeSpecSignatureHash = (TZrUInt64)0x123456789ABCDEF0ULL;
    const TZrSize typeDefSignatureLength = 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    const TZrSize typeSpecSignatureLength = 1u + 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) +
                                            sizeof(TZrUInt32) + 1u + sizeof(TZrUInt32) +
                                            sizeof(TZrUInt32);
    const TZrSize signatureLength = typeDefSignatureLength + typeSpecSignatureLength;
    TZrSize cursor = 0u;
    TZrSize typeSpecSignatureOffset;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    function->metadataStringHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->signatureBlobHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            signatureLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);

    memset(function->metadataStringHeap, 0, sizeof(SZrMetadataStringHeapEntry) * 2u);
    memset(function->signatureBlobHeap, 0, signatureLength);
    memset(function->metadataTokenRecords, 0, sizeof(SZrMetadataTokenRecord) * 4u);
    function->metadataStringHeapLength = 2u;
    function->signatureBlobHeapLength = (TZrUInt32)signatureLength;
    function->metadataTokenRecordLength = 4u;

    function->metadataStringHeap[0].stringIndex = listStringIndex;
    function->metadataStringHeap[0].value = ZrCore_String_CreateFromNative(state, "List");
    function->metadataStringHeap[1].stringIndex = fooStringIndex;
    function->metadataStringHeap[1].value = ZrCore_String_CreateFromNative(state, "Foo");
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[0].value);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[1].value);

    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_DEF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, listStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(typeDefSignatureLength, cursor);

    typeSpecSignatureOffset = cursor;
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_DEF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, listStringIndex);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, 1u);
    cursor += sizeof(TZrUInt32);
    function->signatureBlobHeap[cursor++] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, cursor, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    cursor += sizeof(TZrUInt32);
    write_u32_le(function->signatureBlobHeap, cursor, fooStringIndex);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT(signatureLength, cursor);

    function->metadataTokenRecords[0].token = typeDefToken;
    function->metadataTokenRecords[0].relatedToken = typeDefSignatureToken;
    function->metadataTokenRecords[0].signatureBlobOffset = 0u;
    function->metadataTokenRecords[0].signatureBlobLength = (TZrUInt32)typeDefSignatureLength;
    function->metadataTokenRecords[0].signatureHash = typeDefSignatureHash;
    function->metadataTokenRecords[1].token = typeDefSignatureToken;
    function->metadataTokenRecords[1].relatedToken = typeDefToken;
    function->metadataTokenRecords[1].ownerToken = typeDefToken;
    function->metadataTokenRecords[1].signatureBlobOffset = 0u;
    function->metadataTokenRecords[1].signatureBlobLength = (TZrUInt32)typeDefSignatureLength;
    function->metadataTokenRecords[1].signatureHash = typeDefSignatureHash;
    function->metadataTokenRecords[2].token = typeSpecToken;
    function->metadataTokenRecords[2].relatedToken = typeSpecSignatureToken;
    function->metadataTokenRecords[2].signatureBlobOffset = (TZrUInt32)typeSpecSignatureOffset;
    function->metadataTokenRecords[2].signatureBlobLength = (TZrUInt32)typeSpecSignatureLength;
    function->metadataTokenRecords[2].signatureHash = typeSpecSignatureHash;
    function->metadataTokenRecords[3].token = typeSpecSignatureToken;
    function->metadataTokenRecords[3].relatedToken = typeSpecToken;
    function->metadataTokenRecords[3].ownerToken = typeSpecToken;
    function->metadataTokenRecords[3].signatureBlobOffset = (TZrUInt32)typeSpecSignatureOffset;
    function->metadataTokenRecords[3].signatureBlobLength = (TZrUInt32)typeSpecSignatureLength;
    function->metadataTokenRecords[3].signatureHash = typeSpecSignatureHash;
}

static void assert_generated_preserves_all_fixture_functions(SZrState *state,
                                                             SZrFunction *function,
                                                             SZrAotWriterOptions *options,
                                                             const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");
    free(generatedCText);
}

static void assert_generated_trims_kept_fixture_function(SZrState *state,
                                                         SZrFunction *function,
                                                         SZrAotWriterOptions *options,
                                                         const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_not_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 2 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 1 */");
    free(generatedCText);
}

static void assert_generated_reports_manifest_generic_roots(SZrState *state,
                                                            SZrFunction *function,
                                                            SZrAotWriterOptions *options,
                                                            const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* manifest.genericRoots = 1 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0] target=List argumentCount=2 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].argument[0] = Foo */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].argument[1] = Bar.Baz */");
    free(generatedCText);
}

static void assert_generated_reports_manifest_generic_type_spec_binding(SZrState *state,
                                                                        SZrFunction *function,
                                                                        SZrAotWriterOptions *options,
                                                                        const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].typeSpecToken = 0x07000001 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].signatureToken = 0x08000001 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].signatureHash = 0x123456789abcdef0 */");
    free(generatedCText);
}

static void assert_generated_reports_manifest_generic_instantiation_binding(SZrState *state,
                                                                            SZrFunction *function,
                                                                            SZrAotWriterOptions *options,
                                                                            const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.baseToken = 0x07000001 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.id = 1 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.shareKind = 1 */");
    free(generatedCText);
}

static void assert_generated_reports_manifest_generic_instantiation_type_ref_binding(
        SZrState *state,
        SZrFunction *function,
        SZrAotWriterOptions *options,
        const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.baseToken = 0x05000001 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.id = 1 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.shareKind = 1 */");
    free(generatedCText);
}

static void assert_generated_reports_manifest_generic_instantiation_type_def_binding(
        SZrState *state,
        SZrFunction *function,
        SZrAotWriterOptions *options,
        const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.baseToken = 0x02000001 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.id = 1 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].genericInstance.shareKind = 1 */");
    free(generatedCText);
}

static void assert_generated_reports_manifest_generic_method_spec_binding(
        SZrState *state,
        SZrFunction *function,
        SZrAotWriterOptions *options,
        const TZrChar *artifactName) {
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].methodSpecToken = 0x08000002 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].methodSpec.methodToken = 0x03000001 */");
    assert_text_contains(generatedCText, "/* manifest.genericRoot[0].methodSpec.signatureHash = 0x2233445566778899 */");
    free(generatedCText);
}

static void test_cli_aot_writer_options_bind_preserve_method_to_manifest_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;
    TZrUInt32 directFlatIndex = 0u;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "main.kept");
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-preserve-method";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-preserve-method";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrParser_Writer_ResolveTopLevelCallableFlatIndex(state, function, "kept", &directFlatIndex));
    TEST_ASSERT_EQUAL_UINT32(2u, directFlatIndex);
    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotWriterOptions(&projectContext, &options));
    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, preserveRoots.count);
    TEST_ASSERT_EQUAL_UINT32(2u, preserveRoots.indices[0]);
    TEST_ASSERT_EQUAL_PTR(preserveRoots.indices, options.manifestPreserveFunctionFlatIndices);
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveFunctionFlatIndexCount);

    assert_generated_preserves_all_fixture_functions(state, function, &options, "preserve_method_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_bind_dotted_method_target_to_exact_callable(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "Widget.used", "Widget.kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "Widget.kept");
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-dotted-method";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-dotted-method";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, preserveRoots.count);
    TEST_ASSERT_EQUAL_UINT32(2u, preserveRoots.indices[0]);

    assert_generated_preserves_all_fixture_functions(state, function, &options, "dotted_method_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_bind_type_preserve_methods_to_callable_prefix(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "Widget.used", "Widget.kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE;
    preserveRule.members = ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_METHODS;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "Widget");
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-type-methods";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-type-methods";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(2u, preserveRoots.count);
    TEST_ASSERT_EQUAL_UINT32(1u, preserveRoots.indices[0]);
    TEST_ASSERT_EQUAL_UINT32(2u, preserveRoots.indices[1]);

    assert_generated_preserves_all_fixture_functions(state, function, &options, "type_methods_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_applies_matching_feature_conditioned_preserve_rule(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectFeatureSwitch featureSwitch;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "Widget.used", "Widget.kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&featureSwitch, 0, sizeof(featureSwitch));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    featureSwitch.name = ZrCore_String_CreateFromNative(state, "EnableFastAot");
    featureSwitch.value = ZR_TRUE;
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "Widget.kept");
    preserveRule.feature = ZrCore_String_CreateFromNative(state, "EnableFastAot");
    preserveRule.hasFeatureValue = ZR_TRUE;
    preserveRule.featureValue = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(featureSwitch.name);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    TEST_ASSERT_NOT_NULL(preserveRule.feature);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.featureSwitches = &featureSwitch;
    libraryProject.featureSwitchCount = 1u;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-feature-match";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-feature-match";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, preserveRoots.count);
    TEST_ASSERT_EQUAL_UINT32(2u, preserveRoots.indices[0]);

    assert_generated_preserves_all_fixture_functions(state, function, &options, "feature_match_method_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_skips_mismatched_feature_conditioned_preserve_rule(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectFeatureSwitch featureSwitch;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "Widget.used", "Widget.kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&featureSwitch, 0, sizeof(featureSwitch));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    featureSwitch.name = ZrCore_String_CreateFromNative(state, "EnableFastAot");
    featureSwitch.value = ZR_FALSE;
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "Widget.kept");
    preserveRule.feature = ZrCore_String_CreateFromNative(state, "EnableFastAot");
    preserveRule.hasFeatureValue = ZR_TRUE;
    preserveRule.featureValue = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(featureSwitch.name);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    TEST_ASSERT_NOT_NULL(preserveRule.feature);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.featureSwitches = &featureSwitch;
    libraryProject.featureSwitchCount = 1u;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-feature-mismatch";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-feature-mismatch";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(0u, preserveRoots.count);
    TEST_ASSERT_NULL(options.manifestPreserveFunctionFlatIndices);
    TEST_ASSERT_EQUAL_UINT32(0u, options.manifestPreserveFunctionFlatIndexCount);

    assert_generated_trims_kept_fixture_function(state, function, &options, "feature_mismatch_method_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_bind_generic_preserve_arguments_to_writer_options(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[2];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    genericArguments[1] = ZrCore_String_CreateFromNative(state, "Bar.Baz");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 2u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(genericArguments[1]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(0u, preserveRoots.count);
    TEST_ASSERT_NULL(options.manifestPreserveFunctionFlatIndices);
    TEST_ASSERT_EQUAL_UINT32(0u, options.manifestPreserveFunctionFlatIndexCount);
    TEST_ASSERT_EQUAL_UINT32(1u, preserveRoots.genericRootCount);
    TEST_ASSERT_EQUAL_PTR(preserveRoots.genericRoots, options.manifestPreserveGenericRoots);
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_EQUAL_STRING("List", options.manifestPreserveGenericRoots[0].target);
    TEST_ASSERT_EQUAL_UINT32(2u, options.manifestPreserveGenericRoots[0].argumentCount);
    TEST_ASSERT_EQUAL_STRING("Foo", options.manifestPreserveGenericRoots[0].arguments[0]);
    TEST_ASSERT_EQUAL_STRING("Bar.Baz", options.manifestPreserveGenericRoots[0].arguments[1]);

    assert_generated_reports_manifest_generic_roots(state, function, &options, "generic_preserve_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_binds_generic_preserve_to_type_spec_token(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");
    attach_list_foo_type_spec_metadata(state, function);

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-type-spec";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-type-spec";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u),
                             options.manifestPreserveGenericRoots[0].typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u),
                             options.manifestPreserveGenericRoots[0].signatureToken);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)0x123456789ABCDEF0ULL,
                             options.manifestPreserveGenericRoots[0].signatureHash);

    assert_generated_reports_manifest_generic_type_spec_binding(state,
                                                                function,
                                                                &options,
                                                                "generic_preserve_type_spec_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_materializes_bound_generic_preserve_instantiation_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");
    attach_list_foo_type_spec_metadata(state, function);

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-instantiation";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-instantiation";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasGenericInstantiationBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u),
                             options.manifestPreserveGenericRoots[0].genericInstantiationBaseToken);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             options.manifestPreserveGenericRoots[0].genericInstantiationInstanceId);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE,
                             options.manifestPreserveGenericRoots[0].genericInstantiationShareKind);

    assert_generated_reports_manifest_generic_instantiation_binding(state,
                                                                    function,
                                                                    &options,
                                                                    "generic_preserve_instantiation_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_materializes_generic_preserve_instantiation_open_base_token(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");
    attach_list_foo_type_ref_and_type_spec_metadata(state, function);

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-open-base-token";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-open-base-token";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasGenericInstantiationBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u),
                             options.manifestPreserveGenericRoots[0].genericInstantiationBaseToken);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             options.manifestPreserveGenericRoots[0].genericInstantiationInstanceId);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE,
                             options.manifestPreserveGenericRoots[0].genericInstantiationShareKind);

    assert_generated_reports_manifest_generic_instantiation_type_ref_binding(
            state,
            function,
            &options,
            "generic_preserve_open_base_token_instantiation_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_materializes_generic_preserve_instantiation_type_def_base_token(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");
    attach_list_foo_type_def_and_type_spec_metadata(state, function);

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-type-def-base-token";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-type-def-base-token";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasGenericInstantiationBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u),
                             options.manifestPreserveGenericRoots[0].genericInstantiationBaseToken);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             options.manifestPreserveGenericRoots[0].genericInstantiationInstanceId);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE,
                             options.manifestPreserveGenericRoots[0].genericInstantiationShareKind);

    assert_generated_reports_manifest_generic_instantiation_type_def_binding(
            state,
            function,
            &options,
            "generic_preserve_type_def_base_token_instantiation_root");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_synthesizes_missing_generic_preserve_type_spec_from_open_type_ref(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");
    attach_list_type_ref_metadata(state, function);

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-synthesized-typespec";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-synthesized-typespec";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotWriterOptions(&projectContext, &options));
    TEST_ASSERT_TRUE(options.requireFullAot);
    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u),
                             options.manifestPreserveGenericRoots[0].typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u),
                             options.manifestPreserveGenericRoots[0].signatureToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, options.manifestPreserveGenericRoots[0].signatureHash);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasGenericInstantiationBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u),
                             options.manifestPreserveGenericRoots[0].genericInstantiationBaseToken);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             options.manifestPreserveGenericRoots[0].genericInstantiationInstanceId);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE,
                             options.manifestPreserveGenericRoots[0].genericInstantiationShareKind);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       "generic_preserve_synthesized_typespec_full_aot",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    remove(generatedCPath);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    assert_generated_reports_manifest_generic_instantiation_type_ref_binding(
            state,
            function,
            &options,
            "generic_preserve_synthesized_typespec_full_aot");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_binds_generic_method_preserve_to_method_spec(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");
    attach_generic_method_spec_metadata(state, function);

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "Factory.make");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-method-preserve-methodspec";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-method-preserve-methodspec";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotWriterOptions(&projectContext, &options));
    TEST_ASSERT_TRUE(options.requireFullAot);
    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_FALSE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);
    TEST_ASSERT_FALSE(options.manifestPreserveGenericRoots[0].hasGenericInstantiationBinding);
    TEST_ASSERT_TRUE(options.manifestPreserveGenericRoots[0].hasMethodSpecBinding);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u),
                             options.manifestPreserveGenericRoots[0].methodSpecToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u),
                             options.manifestPreserveGenericRoots[0].methodSpecMethodToken);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)0x2233445566778899ULL,
                             options.manifestPreserveGenericRoots[0].methodSpecSignatureHash);

    assert_generated_reports_manifest_generic_method_spec_binding(
            state,
            function,
            &options,
            "generic_method_preserve_methodspec_full_aot");

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_rejects_unbound_generic_preserve_root_in_full_aot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrLibrary_ProjectPreserveRule preserveRule;
    SZrString *genericArguments[1];
    SZrCliProjectContext projectContext;
    SZrCliAotPreserveRoots preserveRoots;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&preserveRule, 0, sizeof(preserveRule));
    memset(&projectContext, 0, sizeof(projectContext));
    memset(&options, 0, sizeof(options));
    ZrCli_Compiler_AotPreserveRoots_Init(&preserveRoots);

    genericArguments[0] = ZrCore_String_CreateFromNative(state, "Foo");
    preserveRule.kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
    preserveRule.target = ZrCore_String_CreateFromNative(state, "List");
    preserveRule.genericArguments = genericArguments;
    preserveRule.genericArgumentCount = 1u;
    TEST_ASSERT_NOT_NULL(genericArguments[0]);
    TEST_ASSERT_NOT_NULL(preserveRule.target);
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT;
    libraryProject.preserveRules = &preserveRule;
    libraryProject.preserveRuleCount = 1u;
    projectContext.libraryProject = &libraryProject;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-full-aot-unbound";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-full-aot-unbound";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotWriterOptions(&projectContext, &options));
    TEST_ASSERT_TRUE(options.requireFullAot);
    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotPreserveRules(&projectContext,
                                                                 state,
                                                                 function,
                                                                 "main",
                                                                 &options,
                                                                 &preserveRoots));
    TEST_ASSERT_EQUAL_UINT32(1u, options.manifestPreserveGenericRootCount);
    TEST_ASSERT_FALSE(options.manifestPreserveGenericRoots[0].hasTypeSpecBinding);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       "generic_preserve_unbound_full_aot",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    ZrCli_Compiler_AotPreserveRoots_Free(&preserveRoots);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_options_rejects_typespec_only_generic_preserve_root_in_full_aot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    const TZrChar *genericArguments[1] = {"Foo"};
    SZrAotManifestGenericRoot genericRoot;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_preserve_method_fixture(state, "used", "kept");

    memset(&genericRoot, 0, sizeof(genericRoot));
    memset(&options, 0, sizeof(options));

    genericRoot.target = "List";
    genericRoot.arguments = genericArguments;
    genericRoot.argumentCount = 1u;
    genericRoot.hasTypeSpecBinding = ZR_TRUE;
    genericRoot.typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    genericRoot.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    genericRoot.signatureHash = (TZrUInt64)0x123456789ABCDEF0ULL;

    options.moduleName = "main";
    options.sourceHash = "cli-aot-generic-preserve-full-aot-typespec-only";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "cli-aot-generic-preserve-full-aot-typespec-only";
    options.requireExecutableLowering = ZR_TRUE;
    options.requireFullAot = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.manifestPreserveGenericRoots = &genericRoot;
    options.manifestPreserveGenericRootCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_writer_options",
                                                       "generated",
                                                       "generic_preserve_typespec_only_full_aot",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cli_aot_writer_options_bind_preserve_method_to_manifest_root);
    RUN_TEST(test_cli_aot_writer_options_bind_dotted_method_target_to_exact_callable);
    RUN_TEST(test_cli_aot_writer_options_bind_type_preserve_methods_to_callable_prefix);
    RUN_TEST(test_cli_aot_writer_options_applies_matching_feature_conditioned_preserve_rule);
    RUN_TEST(test_cli_aot_writer_options_skips_mismatched_feature_conditioned_preserve_rule);
    RUN_TEST(test_cli_aot_writer_options_bind_generic_preserve_arguments_to_writer_options);
    RUN_TEST(test_cli_aot_writer_options_binds_generic_preserve_to_type_spec_token);
    RUN_TEST(test_cli_aot_writer_options_materializes_bound_generic_preserve_instantiation_root);
    RUN_TEST(test_cli_aot_writer_options_materializes_generic_preserve_instantiation_open_base_token);
    RUN_TEST(test_cli_aot_writer_options_materializes_generic_preserve_instantiation_type_def_base_token);
    RUN_TEST(test_cli_aot_writer_options_synthesizes_missing_generic_preserve_type_spec_from_open_type_ref);
    RUN_TEST(test_cli_aot_writer_options_binds_generic_method_preserve_to_method_spec);
    RUN_TEST(test_cli_aot_writer_options_rejects_unbound_generic_preserve_root_in_full_aot);
    RUN_TEST(test_cli_aot_writer_options_rejects_typespec_only_generic_preserve_root_in_full_aot);
    return UNITY_END();
}
