#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_ffi_contract.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/zrp_metadata.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/writer.h"

#define ZR_AOT_TEST_TYPE_LAYOUT_CACHE_READY ((TZrUInt8)2u)

void setUp(void) {}

void tearDown(void) {}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL(strstr(text, needle));
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL(strstr(text, needle));
}

static void assert_file_does_not_exist(const char *path) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    file = fopen(path, "rb");
    if (file != ZR_NULL) {
        fclose(file);
    }
    TEST_ASSERT_NULL(file);
}

static void assert_aot_c_write_rejected_without_output(
        SZrState *state,
        SZrFunction *function,
        const char *path,
        const SZrAotWriterOptions *options) {
    (void)remove(path);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, path, options));
    assert_file_does_not_exist(path);
}

static SZrObject *get_or_create_function_metadata_object(SZrState *state, SZrFunction *function) {
    SZrObject *metadataObject;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    if (function->hasDecoratorMetadata &&
        function->decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
        function->decoratorMetadataValue.value.object != ZR_NULL) {
        metadataObject = ZR_CAST_OBJECT(state, function->decoratorMetadataValue.value.object);
        if (metadataObject != ZR_NULL) {
            return metadataObject;
        }
    }

    metadataObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(metadataObject);
    ZrCore_Value_InitAsRawObject(state,
                                 &function->decoratorMetadataValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    function->hasDecoratorMetadata = ZR_TRUE;
    return metadataObject;
}

static void mark_function_metadata_uint(SZrState *state,
                                        SZrFunction *function,
                                        const TZrChar *fieldName,
                                        TZrUInt64 fieldValue) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(fieldName);

    metadataObject = get_or_create_function_metadata_object(state, function);
    TEST_ASSERT_NOT_NULL(metadataObject);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state,
                                 &key,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    ZrCore_Value_InitAsUInt(state, &value, fieldValue);
    ZrCore_Object_SetValue(state, metadataObject, &key, &value);
}

static void mark_function_dynamic_dependency_type_layout(SZrState *state,
                                                         SZrFunction *function,
                                                         TZrUInt32 typeLayoutId) {
    mark_function_metadata_uint(state,
                                function,
                                "dynamicDependencyTypeLayoutId",
                                (TZrUInt64)typeLayoutId);
}

static void mark_function_dynamic_dependency_type_token(SZrState *state,
                                                        SZrFunction *function,
                                                        TZrMetadataToken typeToken) {
    mark_function_metadata_uint(state,
                                function,
                                "dynamicDependencyTypeToken",
                                (TZrUInt64)typeToken);
}

static void mark_function_dynamic_dependency_field_token(SZrState *state,
                                                         SZrFunction *function,
                                                         TZrMetadataToken fieldToken) {
    mark_function_metadata_uint(state,
                                function,
                                "dynamicDependencyFieldToken",
                                (TZrUInt64)fieldToken);
}

static void assert_code_stripping_stats(const char *text,
                                        TZrUInt32 functionsBefore,
                                        TZrUInt32 functionsAfter,
                                        TZrUInt32 functionsRemoved) {
    char needle[128];

    assert_text_contains(text, "/* code_stripping.enabled = 1 */");
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.functionsBefore = %u */",
             (unsigned)functionsBefore);
    assert_text_contains(text, needle);
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.functionsAfter = %u */",
             (unsigned)functionsAfter);
    assert_text_contains(text, needle);
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.functionsRemoved = %u */",
             (unsigned)functionsRemoved);
    assert_text_contains(text, needle);
}

static void assert_code_stripping_type_layout_stats(const char *text,
                                                    TZrUInt32 typeLayoutsBefore,
                                                    TZrUInt32 typeLayoutsAfter,
                                                    TZrUInt32 typeLayoutsRemoved) {
    char needle[128];

    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.typeLayoutsBefore = %u */",
             (unsigned)typeLayoutsBefore);
    assert_text_contains(text, needle);
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.typeLayoutsAfter = %u */",
             (unsigned)typeLayoutsAfter);
    assert_text_contains(text, needle);
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.typeLayoutsRemoved = %u */",
             (unsigned)typeLayoutsRemoved);
    assert_text_contains(text, needle);
}

static void assert_code_stripping_type_layout_byte_stats(const char *text,
                                                         unsigned long long bytesBefore,
                                                         unsigned long long bytesAfter,
                                                         unsigned long long bytesRemoved) {
    char needle[160];

    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.typeLayoutPayloadBytesBefore = %llu */",
             bytesBefore);
    assert_text_contains(text, needle);
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.typeLayoutPayloadBytesAfter = %llu */",
             bytesAfter);
    assert_text_contains(text, needle);
    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.typeLayoutPayloadBytesRemoved = %llu */",
             bytesRemoved);
    assert_text_contains(text, needle);
}

static unsigned long long read_u64_marker(const char *text, const char *name) {
    char marker[160];
    const char *valueStart;
    char *valueEnd;
    unsigned long long value;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(name);
    snprintf(marker, sizeof(marker), "/* %s = ", name);
    valueStart = strstr(text, marker);
    TEST_ASSERT_NOT_NULL(valueStart);
    valueStart += strlen(marker);
    value = strtoull(valueStart, &valueEnd, 10);
    TEST_ASSERT_TRUE(valueEnd != valueStart);
    TEST_ASSERT_EQUAL_CHAR(' ', *valueEnd);
    return value;
}

static void assert_code_stripping_type_layout_generated_byte_stats(const char *text,
                                                                   TZrBool expectRemovedBytes) {
    unsigned long long bytesBefore =
            read_u64_marker(text, "code_stripping.typeLayoutGeneratedBytesBefore");
    unsigned long long bytesAfter =
            read_u64_marker(text, "code_stripping.typeLayoutGeneratedBytesAfter");
    unsigned long long bytesRemoved =
            read_u64_marker(text, "code_stripping.typeLayoutGeneratedBytesRemoved");
    unsigned long long emittedTotal = read_u64_marker(text, "aot_size.typeLayoutBytesTotal");

    TEST_ASSERT_TRUE(bytesBefore >= bytesAfter);
    TEST_ASSERT_EQUAL_UINT64(bytesBefore - bytesAfter, bytesRemoved);
    TEST_ASSERT_EQUAL_UINT64(bytesAfter, emittedTotal);
    if (expectRemovedBytes) {
        TEST_ASSERT_TRUE(bytesRemoved > 0u);
    } else {
        TEST_ASSERT_EQUAL_UINT64(0u, bytesRemoved);
    }
}

static void assert_code_stripping_method_metadata_generated_byte_stats(const char *text,
                                                                       TZrBool expectRemovedBytes) {
    unsigned long long bytesBefore =
            read_u64_marker(text, "code_stripping.methodMetadataGeneratedBytesBefore");
    unsigned long long bytesAfter =
            read_u64_marker(text, "code_stripping.methodMetadataGeneratedBytesAfter");
    unsigned long long bytesRemoved =
            read_u64_marker(text, "code_stripping.methodMetadataGeneratedBytesRemoved");
    unsigned long long emittedTotal = read_u64_marker(text, "aot_size.methodMetadataBytesTotal");

    TEST_ASSERT_TRUE(bytesBefore >= bytesAfter);
    TEST_ASSERT_EQUAL_UINT64(bytesBefore - bytesAfter, bytesRemoved);
    TEST_ASSERT_EQUAL_UINT64(bytesAfter, emittedTotal);
    if (expectRemovedBytes) {
        TEST_ASSERT_TRUE(bytesRemoved > 0u);
    } else {
        TEST_ASSERT_EQUAL_UINT64(0u, bytesRemoved);
    }
}

static void assert_code_stripping_reflection_metadata_policy_minimal(const char *text) {
    assert_text_contains(text, "/* metadata_policy.reflectionLevel = 0 */");
    assert_text_contains(text, ".reflectionMetadataLevel = ZR_AOT_REFLECTION_METADATA_NONE,");
    assert_text_does_not_contain(text, ".reflectionMetadataLevel = ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING,");
}

static void assert_code_stripping_function_body_bytes_contains(const char *text,
                                                               TZrUInt32 functionIndex) {
    char needle[128];

    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.functionBodyBytes[%u] = ",
             (unsigned)functionIndex);
    assert_text_contains(text, needle);
}

static void assert_code_stripping_function_body_bytes_missing(const char *text,
                                                              TZrUInt32 functionIndex) {
    char needle[128];

    snprintf(needle,
             sizeof(needle),
             "/* code_stripping.functionBodyBytes[%u] = ",
             (unsigned)functionIndex);
    assert_text_does_not_contain(text, needle);
}

static void assert_code_stripping_function_body_bytes_total_present(const char *text) {
    assert_text_contains(text, "/* code_stripping.functionBodyBytesTotal = ");
}

static void assert_zrp_metadata_size_marker(const char *text,
                                            const char *name,
                                            unsigned long long bytes) {
    char needle[160];

    snprintf(needle, sizeof(needle), "/* aot_size.%s = %llu */", name, bytes);
    assert_text_contains(text, needle);
}

static void assert_code_stripping_zrp_metadata_size_marker(const char *text,
                                                           const char *name,
                                                           unsigned long long bytes) {
    char needle[180];

    snprintf(needle, sizeof(needle), "/* code_stripping.%s = %llu */", name, bytes);
    assert_text_contains(text, needle);
}

static void assert_descriptor_embedded_module_length_marker(const char *text, unsigned long long bytes) {
    char needle[180];

    snprintf(needle, sizeof(needle), "/* descriptor.embeddedModuleBlobLength = %llu */", bytes);
    assert_text_contains(text, needle);
}

static void assert_zrp_metadata_size_stats(const char *text,
                                           TZrSize metadataBytes,
                                           TZrSize tokenRecordBytes,
                                           TZrSize definitionTableBytes,
                                           TZrSize poolBytes,
                                           TZrSize typeDefBytes,
                                           TZrSize stringPoolBytes,
                                           TZrSize signatureBlobPoolBytes,
                                           TZrSize constantPoolBytes) {
    assert_zrp_metadata_size_marker(text, "zrpMetadataBytes", (unsigned long long)metadataBytes);
    assert_zrp_metadata_size_marker(text, "zrpMetadataTokenRecordBytes", (unsigned long long)tokenRecordBytes);
    assert_zrp_metadata_size_marker(text, "zrpMetadataDefinitionTableBytes", (unsigned long long)definitionTableBytes);
    assert_zrp_metadata_size_marker(text, "zrpMetadataPoolBytes", (unsigned long long)poolBytes);
    assert_zrp_metadata_size_marker(text,
                                    "zrpMetadataSectionBytes.tokenRecords",
                                    (unsigned long long)tokenRecordBytes);
    assert_zrp_metadata_size_marker(text, "zrpMetadataSectionBytes.typeDefs", (unsigned long long)typeDefBytes);
    assert_zrp_metadata_size_marker(text,
                                    "zrpMetadataSectionBytes.stringPool",
                                    (unsigned long long)stringPoolBytes);
    assert_zrp_metadata_size_marker(text,
                                    "zrpMetadataSectionBytes.signatureBlobPool",
                                    (unsigned long long)signatureBlobPoolBytes);
    assert_zrp_metadata_size_marker(text,
                                    "zrpMetadataSectionBytes.constantPool",
                                    (unsigned long long)constantPoolBytes);
    assert_zrp_metadata_size_marker(text, "zrpMetadataSectionBytes.manifestExports", 0u);
    assert_zrp_metadata_size_marker(text, "zrpMetadataSectionCounts.manifestExports", 0u);
}

static void assert_zrp_metadata_code_stripping_delta_stats(const char *text,
                                                           TZrSize metadataBytes,
                                                           TZrSize tokenRecordBytes,
                                                           TZrSize definitionTableBytes,
                                                           TZrSize poolBytes) {
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataBytesBefore",
                                                   (unsigned long long)metadataBytes);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataBytesAfter",
                                                   (unsigned long long)metadataBytes);
    assert_code_stripping_zrp_metadata_size_marker(text, "zrpMetadataBytesRemoved", 0u);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataTokenRecordBytesBefore",
                                                   (unsigned long long)tokenRecordBytes);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataTokenRecordBytesAfter",
                                                   (unsigned long long)tokenRecordBytes);
    assert_code_stripping_zrp_metadata_size_marker(text, "zrpMetadataTokenRecordBytesRemoved", 0u);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataDefinitionTableBytesBefore",
                                                   (unsigned long long)definitionTableBytes);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataDefinitionTableBytesAfter",
                                                   (unsigned long long)definitionTableBytes);
    assert_code_stripping_zrp_metadata_size_marker(text, "zrpMetadataDefinitionTableBytesRemoved", 0u);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataPoolBytesBefore",
                                                   (unsigned long long)poolBytes);
    assert_code_stripping_zrp_metadata_size_marker(text,
                                                   "zrpMetadataPoolBytesAfter",
                                                   (unsigned long long)poolBytes);
    assert_code_stripping_zrp_metadata_size_marker(text, "zrpMetadataPoolBytesRemoved", 0u);
}

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
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

static void attach_inline_struct_layout_slot(SZrState *state,
                                             SZrFunction *function,
                                             TZrUInt32 typeLayoutId) {
    SZrFunctionFrameSlotLayout *slotLayout;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    slotLayout = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(slotLayout);
    memset(slotLayout, 0, sizeof(*slotLayout));
    slotLayout->stackSlot = 0u;
    slotLayout->byteOffset = 0u;
    slotLayout->byteSize = 8u;
    slotLayout->byteAlign = 8u;
    slotLayout->typeLayoutId = typeLayoutId;
    slotLayout->slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    function->frameByteSize = 8u;
    function->frameByteAlign = 8u;
    function->frameSlotLayoutLength = 1u;
    function->frameSlotLayouts = slotLayout;
}

static void attach_debug_sidecar_rows(SZrState *state,
                                      SZrFunction *function,
                                      TZrUInt32 rowCount,
                                      TZrUInt32 instructionCount,
                                      TZrUInt32 firstLine) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, rowCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, instructionCount);

    if (function->instructionsList == ZR_NULL) {
        function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(TZrInstruction) * instructionCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->instructionsList);
        memset(function->instructionsList, 0, sizeof(TZrInstruction) * instructionCount);
        function->instructionsLength = instructionCount;
        for (TZrUInt32 instructionIndex = 0u;
             instructionIndex < instructionCount;
             instructionIndex++) {
            function->instructionsList[instructionIndex] =
                    test_create_instruction_2(ZR_INSTRUCTION_ENUM(NOP), 0u, 0u, 0u);
        }
    } else {
        TEST_ASSERT_EQUAL_UINT32(instructionCount, function->instructionsLength);
    }
    function->executionLocationInfoList =
            (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrFunctionExecutionLocationInfo) * rowCount,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->executionLocationInfoList);
    memset(function->executionLocationInfoList,
           0,
           sizeof(SZrFunctionExecutionLocationInfo) * rowCount);
    function->executionLocationInfoLength = rowCount;
    function->lineInSourceStart = firstLine;
    function->lineInSourceEnd = firstLine + rowCount - 1u;

    for (TZrUInt32 rowIndex = 0u; rowIndex < rowCount; rowIndex++) {
        SZrFunctionExecutionLocationInfo *row = &function->executionLocationInfoList[rowIndex];

        row->currentInstructionOffset =
                (TZrMemoryOffset)(rowIndex < instructionCount ? rowIndex : instructionCount - 1u);
        row->lineInSource = firstLine + rowIndex;
        row->columnInSourceStart = 3u + rowIndex;
        row->lineInSourceEnd = firstLine + rowIndex;
        row->columnInSourceEnd = 8u + rowIndex;
    }
}

static void attach_value_layout_slot(SZrState *state, SZrFunction *function) {
    SZrFunctionFrameSlotLayout *slotLayout;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    slotLayout = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(slotLayout);
    memset(slotLayout, 0, sizeof(*slotLayout));
    slotLayout->stackSlot = 0u;
    slotLayout->byteOffset = 0u;
    slotLayout->byteSize = 8u;
    slotLayout->byteAlign = 8u;
    slotLayout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    slotLayout->slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function->frameByteSize = 8u;
    function->frameByteAlign = 8u;
    function->frameSlotLayoutLength = 1u;
    function->frameSlotLayouts = slotLayout;
}

static void initialize_test_union_layout(SZrTypeLayout *layout,
                                         TZrUInt32 byteSize,
                                         TZrUInt32 byteAlign,
                                         TZrUInt32 cTypeId) {
    TEST_ASSERT_NOT_NULL(layout);
    ZrCore_TypeLayout_InitUnion(layout,
                                byteSize,
                                byteAlign,
                                0u,
                                1u,
                                ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                ZR_NULL,
                                0u);
    layout->cTypeId = cTypeId;
    layout->layoutHash = ZrCore_TypeLayout_ComputeHash(layout);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(layout));
}

static void initialize_test_struct_layout(SZrTypeLayout *layout,
                                          TZrUInt32 byteSize,
                                          TZrUInt32 byteAlign,
                                          const SZrTypeLayoutField *fields,
                                          TZrUInt32 fieldCount,
                                          TZrUInt32 cTypeId) {
    TEST_ASSERT_NOT_NULL(layout);
    ZrCore_TypeLayout_InitStruct(layout,
                                 byteSize,
                                 byteAlign,
                                 ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 fields,
                                 fieldCount);
    layout->cTypeId = cTypeId;
    layout->layoutHash = ZrCore_TypeLayout_ComputeHash(layout);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(layout));
}

static void install_static_callable_trim_type_layout_cache(SZrState *state, SZrFunction *root) {
    const TZrUInt32 prototypeCount = 3u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(root);

    root->prototypeFrameTypeLayouts = (SZrTypeLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeLayout) * prototypeCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    root->prototypeFrameTypeLayoutStates = (TZrUInt8 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrUInt8) * prototypeCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->prototypeFrameTypeLayouts);
    TEST_ASSERT_NOT_NULL(root->prototypeFrameTypeLayoutStates);
    memset(root->prototypeFrameTypeLayouts, 0, sizeof(SZrTypeLayout) * prototypeCount);
    memset(root->prototypeFrameTypeLayoutStates, 0, sizeof(TZrUInt8) * prototypeCount);
    root->prototypeCount = prototypeCount;
    root->prototypeFrameTypeLayoutLength = prototypeCount;

    for (TZrUInt32 typeLayoutId = 1u; typeLayoutId <= 2u; typeLayoutId++) {
        initialize_test_union_layout(
                &root->prototypeFrameTypeLayouts[typeLayoutId], 8u, 8u, typeLayoutId);
        root->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_AOT_TEST_TYPE_LAYOUT_CACHE_READY;
    }
}

static void enable_static_callable_trim_type_layout_zero(SZrFunction *root) {
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(root->prototypeFrameTypeLayouts);
    TEST_ASSERT_NOT_NULL(root->prototypeFrameTypeLayoutStates);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, root->prototypeFrameTypeLayoutLength);

    initialize_test_union_layout(&root->prototypeFrameTypeLayouts[0], 8u, 8u, 0u);
    root->prototypeFrameTypeLayoutStates[0] = ZR_AOT_TEST_TYPE_LAYOUT_CACHE_READY;
}

static void set_zrp_metadata_section(SZrZrpMetadataSection *section,
                                     TZrUInt32 *offset,
                                     TZrUInt32 byteLength,
                                     TZrUInt32 count,
                                     TZrUInt32 elementSize) {
    TEST_ASSERT_NOT_NULL(section);
    TEST_ASSERT_NOT_NULL(offset);

    if (byteLength == 0u) {
        memset(section, 0, sizeof(*section));
        return;
    }

    section->offset = *offset;
    section->byteLength = byteLength;
    section->count = count;
    section->elementSize = elementSize;
    *offset += byteLength;
}

static TZrSize build_zrp_metadata_size_fixture(TZrByte *buffer,
                                               TZrSize bufferLength,
                                               TZrSize *outTokenRecordBytes,
                                               TZrSize *outDefinitionTableBytes,
                                               TZrSize *outPoolBytes,
                                               TZrSize *outTypeDefBytes,
                                               TZrSize *outStringPoolBytes,
                                               TZrSize *outSignatureBlobPoolBytes,
                                               TZrSize *outConstantPoolBytes) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 stringPoolBytes = 6u;
    const TZrUInt32 signatureBlobPoolBytes = 7u;
    const TZrUInt32 constantPoolBytes = 5u;
    SZrZrpMetadataHeader header;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           tokenRecordBytes +
                                           typeDefBytes +
                                           stringPoolBytes +
                                           signatureBlobPoolBytes +
                                           constantPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_zrp_metadata_section(&header.tokenRecords,
                             &offset,
                             tokenRecordBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_zrp_metadata_section(&header.typeDefs,
                             &offset,
                             typeDefBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_zrp_metadata_section(&header.methodDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_zrp_metadata_section(&header.signatureBlobPool,
                             &offset,
                             signatureBlobPoolBytes,
                             signatureBlobPoolBytes,
                             1u);
    set_zrp_metadata_section(&header.constantPool,
                             &offset,
                             constantPoolBytes,
                             constantPoolBytes,
                             1u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    *outTokenRecordBytes = tokenRecordBytes;
    *outDefinitionTableBytes = typeDefBytes;
    *outPoolBytes = (TZrSize)stringPoolBytes + signatureBlobPoolBytes + constantPoolBytes;
    *outTypeDefBytes = typeDefBytes;
    *outStringPoolBytes = stringPoolBytes;
    *outSignatureBlobPoolBytes = signatureBlobPoolBytes;
    *outConstantPoolBytes = constantPoolBytes;
    return offset;
}

static TZrSize build_zrp_metadata_method_def_trim_fixture(TZrByte *buffer,
                                                          TZrSize bufferLength,
                                                          TZrSize *outMetadataBytesAfterTrim,
                                                          TZrSize *outTokenRecordBytes,
                                                          TZrSize *outDefinitionTableBytesBeforeTrim,
                                                          TZrSize *outDefinitionTableBytesAfterTrim,
                                                          TZrSize *outPoolBytesBeforeTrim,
                                                          TZrSize *outPoolBytesAfterTrim,
                                                          TZrSize *outTypeDefBytes,
                                                          TZrSize *outMethodDefBytesBeforeTrim,
                                                          TZrSize *outMethodDefBytesAfterTrim,
                                                          TZrSize *outStringPoolBytesBeforeTrim,
                                                          TZrSize *outStringPoolBytesAfterTrim,
                                                          TZrSize *outSignatureBlobPoolBytesBeforeTrim,
                                                          TZrSize *outSignatureBlobPoolBytesAfterTrim,
                                                          TZrSize *outConstantPoolBytesBeforeTrim,
                                                          TZrSize *outConstantPoolBytesAfterTrim) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefRowBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodDefBytesBeforeTrim = methodDefRowBytes * 2u;
    const TZrUInt32 methodDefBytesAfterTrim = methodDefRowBytes;
    const TZrUInt32 stringPoolBytesBeforeTrim = 6u;
    const TZrUInt32 stringPoolBytesAfterTrim = 1u;
    const TZrUInt32 signatureBlobPoolBytesBeforeTrim = 7u;
    const TZrUInt32 signatureBlobPoolBytesAfterTrim = 0u;
    const TZrUInt32 constantPoolBytesBeforeTrim = 5u;
    const TZrUInt32 constantPoolBytesAfterTrim = 0u;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           tokenRecordBytes +
                                           typeDefBytes +
                                           methodDefBytesBeforeTrim +
                                           stringPoolBytesBeforeTrim +
                                           signatureBlobPoolBytesBeforeTrim +
                                           constantPoolBytesBeforeTrim);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_zrp_metadata_section(&header.tokenRecords,
                             &offset,
                             tokenRecordBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_zrp_metadata_section(&header.typeDefs,
                             &offset,
                             typeDefBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_zrp_metadata_section(&header.methodDefs,
                             &offset,
                             methodDefBytesBeforeTrim,
                             2u,
                             methodDefRowBytes);
    set_zrp_metadata_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.stringPool,
                             &offset,
                             stringPoolBytesBeforeTrim,
                             stringPoolBytesBeforeTrim,
                             1u);
    set_zrp_metadata_section(&header.signatureBlobPool,
                             &offset,
                             signatureBlobPoolBytesBeforeTrim,
                             signatureBlobPoolBytesBeforeTrim,
                             1u);
    set_zrp_metadata_section(&header.constantPool,
                             &offset,
                             constantPoolBytesBeforeTrim,
                             constantPoolBytesBeforeTrim,
                             1u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    methodDefs[0].ownerTypeToken = typeDefs[0].token;
    methodDefs[0].functionIndex = 2u;
    methodDefs[1].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    methodDefs[1].ownerTypeToken = typeDefs[0].token;
    methodDefs[1].functionIndex = 1u;

    *outMetadataBytesAfterTrim =
            (TZrSize)(offset -
                      methodDefRowBytes -
                      (stringPoolBytesBeforeTrim - stringPoolBytesAfterTrim) -
                      signatureBlobPoolBytesBeforeTrim +
                      signatureBlobPoolBytesAfterTrim -
                      constantPoolBytesBeforeTrim +
                      constantPoolBytesAfterTrim);
    *outTokenRecordBytes = tokenRecordBytes;
    *outDefinitionTableBytesBeforeTrim = (TZrSize)typeDefBytes + methodDefBytesBeforeTrim;
    *outDefinitionTableBytesAfterTrim = (TZrSize)typeDefBytes + methodDefBytesAfterTrim;
    *outPoolBytesBeforeTrim =
            (TZrSize)stringPoolBytesBeforeTrim + signatureBlobPoolBytesBeforeTrim + constantPoolBytesBeforeTrim;
    *outPoolBytesAfterTrim =
            (TZrSize)stringPoolBytesAfterTrim + signatureBlobPoolBytesAfterTrim + constantPoolBytesAfterTrim;
    *outTypeDefBytes = typeDefBytes;
    *outMethodDefBytesBeforeTrim = methodDefBytesBeforeTrim;
    *outMethodDefBytesAfterTrim = methodDefBytesAfterTrim;
    *outStringPoolBytesBeforeTrim = stringPoolBytesBeforeTrim;
    *outStringPoolBytesAfterTrim = stringPoolBytesAfterTrim;
    *outSignatureBlobPoolBytesBeforeTrim = signatureBlobPoolBytesBeforeTrim;
    *outSignatureBlobPoolBytesAfterTrim = signatureBlobPoolBytesAfterTrim;
    *outConstantPoolBytesBeforeTrim = constantPoolBytesBeforeTrim;
    *outConstantPoolBytesAfterTrim = constantPoolBytesAfterTrim;
    return offset;
}

static TZrSize build_zrp_metadata_type_token_layout_fixture(TZrByte *buffer,
                                                            TZrSize bufferLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE + tokenRecordBytes + typeDefBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_zrp_metadata_section(&header.tokenRecords,
                             &offset,
                             tokenRecordBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_zrp_metadata_section(&header.typeDefs,
                             &offset,
                             typeDefBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_zrp_metadata_section(&header.methodDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = tokenRecords[0].token;
    typeDefs[0].typeLayoutId = 2u;

    return offset;
}

static TZrSize build_zrp_metadata_type_ref_token_layout_fixture(TZrByte *buffer,
                                                                TZrSize bufferLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)(sizeof(SZrMetadataTokenRecord) * 2u);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE + tokenRecordBytes + typeDefBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_zrp_metadata_section(&header.tokenRecords,
                             &offset,
                             tokenRecordBytes,
                             2u,
                             (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_zrp_metadata_section(&header.typeDefs,
                             &offset,
                             typeDefBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_zrp_metadata_section(&header.methodDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    tokenRecords[0].targetMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    tokenRecords[1].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = tokenRecords[1].token;
    typeDefs[0].typeLayoutId = 2u;

    return offset;
}

static TZrSize build_zrp_metadata_type_spec_token_layout_fixture(TZrByte *buffer,
                                                                 TZrSize bufferLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE + tokenRecordBytes + typeSpecBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_zrp_metadata_section(&header.tokenRecords,
                             &offset,
                             tokenRecordBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_zrp_metadata_section(&header.typeDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.methodDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.typeSpecs,
                             &offset,
                             typeSpecBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_zrp_metadata_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);

    typeSpecs = (SZrZrpMetadataTypeSpecRow *)(void *)(buffer + header.typeSpecs.offset);
    typeSpecs[0].token = tokenRecords[0].token;
    typeSpecs[0].typeLayoutId = 2u;

    return offset;
}

static TZrSize build_zrp_metadata_field_token_layout_fixture(TZrByte *buffer,
                                                             TZrSize bufferLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)(sizeof(SZrMetadataTokenRecord) * 2u);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           tokenRecordBytes +
                                           typeDefBytes +
                                           fieldDefBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_zrp_metadata_section(&header.tokenRecords,
                             &offset,
                             tokenRecordBytes,
                             2u,
                             (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_zrp_metadata_section(&header.typeDefs,
                             &offset,
                             typeDefBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_zrp_metadata_section(&header.methodDefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.fieldDefs,
                             &offset,
                             fieldDefBytes,
                             1u,
                             (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    set_zrp_metadata_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_zrp_metadata_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    tokenRecords[1].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = tokenRecords[0].token;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;
    typeDefs[0].typeLayoutId = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = tokenRecords[1].token;
    fieldDefs[0].ownerTypeToken = typeDefs[0].token;
    fieldDefs[0].byteOffset = 0u;
    fieldDefs[0].typeLayoutId = 2u;

    return offset;
}

static SZrFunction *create_static_callable_trim_fixture(SZrState *state) {
    SZrFunction *root;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
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

    root->childFunctionList[0].parameterCount = 0u;
    root->childFunctionList[0].stackSize = 1u;
    root->childFunctionList[0].ownerFunction = root;
    root->childFunctionList[0].lineInSourceStart = 10u;
    root->childFunctionList[0].lineInSourceEnd = 10u;

    root->childFunctionList[1].parameterCount = 0u;
    root->childFunctionList[1].stackSize = 1u;
    root->childFunctionList[1].ownerFunction = root;
    root->childFunctionList[1].lineInSourceStart = 20u;
    root->childFunctionList[1].lineInSourceEnd = 20u;
    attach_inline_struct_layout_slot(state, &root->childFunctionList[0], 1u);
    attach_inline_struct_layout_slot(state, &root->childFunctionList[1], 2u);
    install_static_callable_trim_type_layout_cache(state, root);
    return root;
}

static SZrFunction *create_single_compiled_member_trim_fixture(
        SZrState *state,
        TZrUInt32 prototypeType,
        TZrUInt32 prototypeModifierFlags,
        const SZrCompiledMemberInfo *member) {
    const TZrUInt32 prototypeCount = 1u;
    const TZrUInt32 prototypeDataLength = (TZrUInt32)(sizeof(prototypeCount) +
                                                      sizeof(SZrCompiledPrototypeInfo) +
                                                      sizeof(SZrCompiledMemberInfo));
    SZrCompiledPrototypeInfo prototype;
    SZrFunction *root;
    SZrFunction *target;
    TZrSize offset = 0u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(member);
    root = ZrCore_Function_New(state);
    target = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(target);

    root->stackSize = 1u;
    root->lineInSourceStart = 1u;
    root->lineInSourceEnd = 1u;
    target->stackSize = 1u;
    target->lineInSourceStart = 10u;
    target->lineInSourceEnd = 10u;

    root->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->constantValueList);
    root->constantValueLength = 1u;
    ZrCore_Value_InitAsRawObject(state,
                                 &root->constantValueList[0],
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(target));
    root->constantValueList[0].type = ZR_VALUE_TYPE_FUNCTION;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction));
    root->childFunctionLength = 1u;
    root->childFunctionList[0].ownerFunction = root;
    root->childFunctionList[0].stackSize = 1u;
    root->childFunctionList[0].lineInSourceStart = 20u;
    root->childFunctionList[0].lineInSourceEnd = 20u;

    root->prototypeData = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            prototypeDataLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->prototypeData);
    root->prototypeDataLength = prototypeDataLength;
    root->prototypeCount = prototypeCount;

    memset(&prototype, 0, sizeof(prototype));
    prototype.type = prototypeType;
    prototype.membersCount = 1u;
    prototype.modifierFlags = prototypeModifierFlags;

    memcpy(root->prototypeData + offset, &prototypeCount, sizeof(prototypeCount));
    offset += sizeof(prototypeCount);
    memcpy(root->prototypeData + offset, &prototype, sizeof(prototype));
    offset += sizeof(prototype);
    memcpy(root->prototypeData + offset, member, sizeof(*member));
    return root;
}

static SZrFunction *create_property_accessor_trim_fixture(SZrState *state,
                                                          TZrUInt32 accessorRole,
                                                          TZrUInt32 functionConstantIndex) {
    SZrCompiledMemberInfo member;

    memset(&member, 0, sizeof(member));
    member.functionConstantIndex = functionConstantIndex;
    member.propertyIdentity = 0u;
    member.accessorRole = accessorRole;
    return create_single_compiled_member_trim_fixture(state,
                                                      ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                                      0u,
                                                      &member);
}

static SZrFunction *create_resource_drop_trim_fixture(SZrState *state,
                                                      TZrUInt32 functionConstantIndex) {
    SZrCompiledMemberInfo member;

    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CLASS_META_FUNCTION;
    member.isMetaMethod = ZR_TRUE;
    member.metaType = (TZrUInt32)ZR_META_DESTRUCTOR;
    member.functionConstantIndex = functionConstantIndex;
    member.propertyIdentity = UINT32_MAX;
    return create_single_compiled_member_trim_fixture(state,
                                                      ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                                      ZR_DECLARATION_MODIFIER_RESOURCE,
                                                      &member);
}

static SZrFunction *create_reflection_constructor_trim_fixture(SZrState *state,
                                                               TZrUInt32 functionConstantIndex) {
    SZrCompiledMemberInfo member;

    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CLASS_META_FUNCTION;
    member.accessModifier = ZR_ACCESS_CONSTANT_PUBLIC;
    member.isMetaMethod = ZR_TRUE;
    member.metaType = (TZrUInt32)ZR_META_CONSTRUCTOR;
    member.functionConstantIndex = functionConstantIndex;
    member.propertyIdentity = UINT32_MAX;
    return create_single_compiled_member_trim_fixture(state,
                                                      ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                                      0u,
                                                      &member);
}

static void add_exported_second_child_callable_binding(SZrState *state, SZrFunction *root) {
    SZrFunctionTopLevelCallableBinding *binding;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(root);

    binding = (SZrFunctionTopLevelCallableBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTopLevelCallableBinding),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(binding);
    memset(binding, 0, sizeof(*binding));
    binding->callableChildIndex = 1u;
    binding->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    root->topLevelCallableBindings = binding;
    root->topLevelCallableBindingLength = 1u;
}

static void add_typed_exported_first_child_method_token(SZrState *state,
                                                         SZrFunction *root,
                                                         TZrMetadataToken metadataToken) {
    SZrFunctionTypedExportSymbol *symbol;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(root);

    symbol = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(symbol);
    memset(symbol, 0, sizeof(*symbol));
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->callableChildIndex = 0u;
    symbol->metadataToken = metadataToken;
    root->typedExportedSymbols = symbol;
    root->typedExportedSymbolLength = 1u;
}

static void add_typed_second_child_method_token(SZrState *state,
                                                 SZrFunction *root,
                                                 TZrMetadataToken metadataToken) {
    SZrFunctionTypedExportSymbol *symbol;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(root);

    symbol = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(symbol);
    memset(symbol, 0, sizeof(*symbol));
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
    symbol->callableChildIndex = 1u;
    symbol->metadataToken = metadataToken;
    root->typedExportedSymbols = symbol;
    root->typedExportedSymbolLength = 1u;
}

static void add_native_callback_escape_binding(SZrState *state,
                                               SZrFunction *root,
                                               TZrUInt32 stackSlot) {
    SZrFunctionEscapeBinding *binding;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(root);

    binding = (SZrFunctionEscapeBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionEscapeBinding),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(binding);
    memset(binding, 0, sizeof(*binding));
    binding->slotOrIndex = stackSlot;
    binding->escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE;
    binding->bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING;
    root->escapeBindings = binding;
    root->escapeBindingLength = 1u;
}

static void add_native_import_contract(SZrState *state,
                                       SZrFunction *function,
                                       TZrUInt64 symbolId,
                                       const TZrChar *entryPoint) {
    SZrNativeImportContract *previousContracts;
    SZrNativeImportContract *contracts;
    SZrNativeImportContract *contract;
    TZrUInt32 previousLength;
    TZrUInt32 newLength;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(entryPoint);
    previousContracts = function->nativeImportContracts;
    previousLength = function->nativeImportContractLength;
    TEST_ASSERT_LESS_THAN_UINT32(
            ZR_FFI_CONTRACT_MAX_IMPORTS_PER_FUNCTION, previousLength);
    newLength = previousLength + 1u;
    contracts = (SZrNativeImportContract *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrNativeImportContract) * newLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(contracts);
    if (previousLength > 0u) {
        TEST_ASSERT_NOT_NULL(previousContracts);
        memcpy(contracts,
               previousContracts,
               sizeof(SZrNativeImportContract) * previousLength);
    }
    contract = &contracts[previousLength];
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_FFI_CONTRACT_SCHEMA_VERSION;
    strcpy(contract->libraryLocator, "fixture");
    strcpy(contract->entryPoint, entryPoint);
    strcpy(contract->sourceMapping.document, "native_import_reachability.zr");
    contract->symbolId = symbolId;
    contract->declaringModuleId = 0xabcdu;
    contract->availability = ZR_FFI_CONTRACT_AVAILABILITY_ALL;
    contract->requiredCapabilities = ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME;
    contract->sourceMapping.startLine = 1;
    contract->sourceMapping.startColumn = 1;
    contract->sourceMapping.endLine = 1;
    contract->sourceMapping.endColumn = 1;
    contract->signature.abi = ZR_FFI_CONTRACT_ABI_C;
    contract->signature.targetPointerSize = (TZrUInt32)sizeof(TZrPtr);
    contract->signature.targetEndianness = ZR_FFI_CONTRACT_ENDIAN_LITTLE;
    strcpy(contract->signature.targetTriple, ZrCommon_FfiContract_GetHostTargetTriple());
    contract->signature.targetAbiHash = ZrCommon_FfiContract_ComputeTargetAbiHash(
            contract->signature.abi,
            contract->signature.targetPointerSize,
            contract->signature.targetEndianness,
            contract->signature.targetTriple);
    contract->signature.charset = ZR_FFI_CONTRACT_CHARSET_NONE;
    contract->signature.errorPolicy = ZR_FFI_CONTRACT_ERROR_NONE;
    contract->signature.cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_NONE;
    contract->signature.callbackLifetime = ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE;
    contract->signature.callbackThreadPolicy = ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE;
    contract->signature.callbackExceptionPolicy = ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE;
    contract->signature.returnType.typeKind = ZR_FFI_CONTRACT_TYPE_VOID;
    contract->signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract->signature);
    contract->callableContractHash = contract->signature.signatureHash;
    TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(contract));
    if (previousContracts != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                previousContracts,
                sizeof(SZrNativeImportContract) * previousLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    function->nativeImportContracts = contracts;
    function->nativeImportContractLength = newLength;
}

static void test_aot_c_code_stripping_option_filters_unreachable_static_callable(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    attach_value_layout_slot(state, function);
    attach_debug_sidecar_rows(state, function, 1u, 1u, 1u);
    attach_debug_sidecar_rows(state, &function->childFunctionList[0], 2u, 1u, 10u);
    attach_debug_sidecar_rows(state, &function->childFunctionList[1], 1u, 1u, 20u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping";
    options.sourceHash = "aot-c-code-stripping";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "static_callable_trim",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsAfter = 2 */");
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsRemoved = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.debugLocationsBefore = 4 */");
    assert_text_contains(generatedCText, "/* code_stripping.debugLocationsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.debugLocationsRemoved = 1 */");
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 1u, 1u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 8u, 8u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_reflection_metadata_policy_minimal(generatedCText);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 0u);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 1u);
    assert_code_stripping_function_body_bytes_missing(generatedCText, 2u);
    assert_code_stripping_function_body_bytes_total_present(generatedCText);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 2 */");
    assert_text_contains(generatedCText,
                         "/* reachability.functionManifest.node[0] = reason=root.entry predecessor=none */");
    assert_text_contains(generatedCText,
                         "/* reachability.functionManifest.node[1] = reason=edge.direct_call predecessor=0 */");
    assert_text_does_not_contain(generatedCText, "reachability.functionManifest.node[2]");
    assert_text_contains(generatedCText, "/* reachability.frameLayoutManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.frameLayoutManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.frameLayoutManifest.node[0] = reason=edge.frame_layout "
            "predecessorFunction=0 ownerFunction=0 slotLayout=0 stackSlot=0 byteOffset=0 "
            "byteSize=8 byteAlign=8 typeLayoutId=4294967295 slotKind=value isParameter=0 "
            "flags=0x0000 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.frameLayoutManifest.node[1] = reason=edge.frame_layout "
            "predecessorFunction=1 ownerFunction=1 slotLayout=0 stackSlot=0 byteOffset=0 "
            "byteSize=8 byteAlign=8 typeLayoutId=1 slotKind=inline_struct isParameter=0 "
            "flags=0x0000 */");
    assert_text_does_not_contain(generatedCText, "reachability.frameLayoutManifest.node[2]");
    assert_text_contains(generatedCText, "/* reachability.debugSidecarManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.debugSidecarManifest.count = 3 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.debugSidecarManifest.node[0] = reason=edge.debug_sidecar "
            "predecessorFunction=0 ownerFunction=0 locationIndex=0 instructionOffset=0 "
            "lineStart=1 columnStart=3 lineEnd=1 columnEnd=8 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.debugSidecarManifest.node[1] = reason=edge.debug_sidecar "
            "predecessorFunction=1 ownerFunction=1 locationIndex=0 instructionOffset=0 "
            "lineStart=10 columnStart=3 lineEnd=10 columnEnd=8 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.debugSidecarManifest.node[2] = reason=edge.debug_sidecar "
            "predecessorFunction=1 ownerFunction=1 locationIndex=1 instructionOffset=0 "
            "lineStart=11 columnStart=4 lineEnd=11 columnEnd=9 */");
    assert_text_does_not_contain(generatedCText, "reachability.debugSidecarManifest.node[3]");
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.count = 1 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.typeLayoutManifest.node[1] = reason=edge.frame_layout predecessorFunction=1 */");
    assert_text_does_not_contain(generatedCText, "reachability.typeLayoutManifest.node[2]");
    assert_text_contains(generatedCText,
                         "static const FZrAotEntryThunk zr_aot_function_thunks[] = {\n"
                         "    zr_aot_fn_0,\n"
                         "    zr_aot_fn_1,\n"
                         "    ZR_NULL,\n"
                         "};");
    assert_text_contains(generatedCText,
                         "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {\n"
                         "    &zr_aot_method_info_0,\n"
                         "    &zr_aot_method_info_1,\n"
                         "    ZR_NULL,\n"
                         "};");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_malformed_unreachable_debug_sidecar(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;
    SZrFunctionExecutionLocationInfo *locations;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].executionLocationInfoLength = 1u;

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_malformed_unreachable_debug_sidecar";
    options.sourceHash = "aot-c-code-stripping-malformed-unreachable-debug-sidecar";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-malformed-unreachable-debug-sidecar";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "malformed_unreachable_debug_sidecar",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    attach_debug_sidecar_rows(state, &function->childFunctionList[1], 2u, 2u, 20u);
    locations = function->childFunctionList[1].executionLocationInfoList;

    locations[0].currentInstructionOffset = -1;
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    assert_file_does_not_exist(generatedCPath);

    locations[0].currentInstructionOffset = 0;
    locations[1].currentInstructionOffset = 2;
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    assert_file_does_not_exist(generatedCPath);

    locations[0].currentInstructionOffset = 1;
    locations[1].currentInstructionOffset = 0;
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    assert_file_does_not_exist(generatedCPath);

    locations[0].currentInstructionOffset = 0;
    locations[1].currentInstructionOffset = 1;
    locations[0].lineInSourceEnd = locations[0].lineInSource - 1u;
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    assert_file_does_not_exist(generatedCPath);

    locations[0].lineInSourceEnd = locations[0].lineInSource;
    locations[0].columnInSourceEnd = locations[0].columnInSourceStart - 1u;
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    assert_file_does_not_exist(generatedCPath);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unresolved_retained_frame_type_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[0].frameSlotLayouts[0].typeLayoutId = 3u;

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_unresolved_retained_frame_type_layout";
    options.sourceHash = "aot-c-code-stripping-unresolved-retained-frame-type-layout";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-unresolved-retained-frame-type-layout";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "unresolved_retained_frame_type_layout",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_malformed_unreachable_frame_type_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionFrameSlotLayout *unreachableLayout;
    SZrTypeLayout *typeLayout;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    unreachableLayout = &function->childFunctionList[1].frameSlotLayouts[0];
    typeLayout = &function->prototypeFrameTypeLayouts[2];

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_malformed_unreachable_frame_type_layout";
    options.sourceHash = "aot-c-code-stripping-malformed-unreachable-frame-type-layout";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-malformed-unreachable-frame-type-layout";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "malformed_unreachable_frame_type_layout",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    unreachableLayout->typeLayoutId = 3u;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    unreachableLayout->typeLayoutId = 2u;
    typeLayout->layoutHash ^= 1u;
    TEST_ASSERT_NOT_EQUAL_UINT64(
            ZrCore_TypeLayout_ComputeHash(typeLayout), typeLayout->layoutHash);
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    initialize_test_union_layout(typeLayout, 8u, 8u, 9u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(typeLayout));
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    ZrCore_TypeLayout_InitValue(typeLayout);
    typeLayout->cTypeId = 2u;
    typeLayout->layoutHash = ZrCore_TypeLayout_ComputeHash(typeLayout);
    unreachableLayout->byteSize = typeLayout->byteSize;
    unreachableLayout->byteAlign = typeLayout->byteAlign;
    function->childFunctionList[1].frameByteSize = typeLayout->byteSize;
    function->childFunctionList[1].frameByteAlign = typeLayout->byteAlign;
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(typeLayout));
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    initialize_test_union_layout(typeLayout, 8u, 8u, 2u);
    unreachableLayout->byteAlign = 8u;
    unreachableLayout->byteSize = 16u;
    function->childFunctionList[1].frameByteSize = 16u;
    function->childFunctionList[1].frameByteAlign = 8u;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    unreachableLayout->byteSize = 8u;
    unreachableLayout->byteAlign = 16u;
    function->childFunctionList[1].frameByteSize = 8u;
    function->childFunctionList[1].frameByteAlign = 16u;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unreachable_materialized_parameter_undercount(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *unreachable;
    SZrFunctionFrameSlotLayout *layouts;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_materialized_parameter_cardinality";
    options.sourceHash = "aot-c-code-stripping-materialized-parameter-cardinality";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-materialized-parameter-cardinality";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "materialized_parameter_cardinality",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    function->parameterCount = 1u;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    function->parameterCount = 0u;
    function->childFunctionList[0].parameterCount = 1u;
    function->childFunctionList[0].stackSize = 2u;
    function->childFunctionList[0].frameSlotLayouts[0].stackSlot = 1u;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    unreachable = &function->childFunctionList[1];
    unreachable->parameterCount = 1u;
    TEST_ASSERT_EQUAL_UINT32(1u, unreachable->frameSlotLayoutLength);
    TEST_ASSERT_EQUAL_UINT8(0u, unreachable->frameSlotLayouts[0].isParameter);
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    ZrCore_Memory_RawFreeWithType(state->global,
                                  unreachable->frameSlotLayouts,
                                  sizeof(SZrFunctionFrameSlotLayout),
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);
    memset(layouts, 0, sizeof(SZrFunctionFrameSlotLayout) * 2u);
    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset = index * 8u;
        layouts[index].byteSize = 8u;
        layouts[index].byteAlign = 8u;
        layouts[index].typeLayoutId = 2u;
        layouts[index].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    }
    layouts[1].isParameter = 1u;
    unreachable->stackSize = 2u;
    unreachable->frameByteSize = 16u;
    unreachable->frameByteAlign = 8u;
    unreachable->frameSlotLayoutLength = 2u;
    unreachable->frameSlotLayouts = layouts;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_malformed_unreachable_constructor_bitmap(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *unreachable;
    SZrFunctionFrameSlotLayout *layouts;
    SZrTypeLayoutField field;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    unreachable = &function->childFunctionList[1];
    unreachable->functionName = ZrCore_String_CreateFromNative(state, "constructor");
    TEST_ASSERT_NOT_NULL(unreachable->functionName);
    unreachable->parameterCount = 1u;
    unreachable->frameSlotLayouts[0].isParameter = 1u;
    unreachable->frameSlotLayouts[0].reserved0 =
            ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP;

    memset(&field, 0, sizeof(field));
    field.byteSize = 8u;
    initialize_test_struct_layout(
            &function->prototypeFrameTypeLayouts[2], 8u, 8u, &field, 1u, 2u);
    unreachable->frameByteSize = 16u;
    unreachable->frameByteAlign = 8u;

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_constructor_bitmap_layout";
    options.sourceHash = "aot-c-code-stripping-constructor-bitmap-layout";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-constructor-bitmap-layout";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "constructor_bitmap_layout",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    unreachable->frameByteSize = 8u;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    unreachable->frameByteSize = 16u;
    unreachable->functionName = ZrCore_String_CreateFromNative(state, "not_constructor");
    TEST_ASSERT_NOT_NULL(unreachable->functionName);
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    unreachable->functionName = ZrCore_String_CreateFromNative(state, "constructor");
    TEST_ASSERT_NOT_NULL(unreachable->functionName);
    initialize_test_struct_layout(
            &function->prototypeFrameTypeLayouts[2], 8u, 8u, ZR_NULL, 0u, 2u);
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    field.byteSize = 4u;
    initialize_test_struct_layout(
            &function->prototypeFrameTypeLayouts[2], 4u, 4u, &field, 1u, 2u);
    unreachable->frameSlotLayouts[0].byteSize = 4u;
    unreachable->frameSlotLayouts[0].byteAlign = 4u;
    unreachable->frameByteAlign = 4u;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    field.byteSize = 8u;
    initialize_test_struct_layout(
            &function->prototypeFrameTypeLayouts[2], 8u, 8u, &field, 1u, 2u);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  unreachable->frameSlotLayouts,
                                  sizeof(SZrFunctionFrameSlotLayout),
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);
    memset(layouts, 0, sizeof(SZrFunctionFrameSlotLayout) * 2u);
    layouts[0].stackSlot = 0u;
    layouts[0].byteSize = 8u;
    layouts[0].byteAlign = 8u;
    layouts[0].typeLayoutId = 2u;
    layouts[0].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[0].isParameter = 1u;
    layouts[0].reserved0 =
            ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP;
    layouts[1].stackSlot = 1u;
    layouts[1].byteOffset = 8u;
    layouts[1].byteSize = 8u;
    layouts[1].byteAlign = 8u;
    layouts[1].typeLayoutId = 1u;
    layouts[1].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    unreachable->stackSize = 2u;
    unreachable->frameSlotLayouts = layouts;
    unreachable->frameSlotLayoutLength = 2u;
    unreachable->frameByteSize = 24u;
    unreachable->frameByteAlign = 8u;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    unreachable->frameByteSize = 16u;
    assert_aot_c_write_rejected_without_output(
            state, function, generatedCPath, &options);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_malformed_unreachable_frame_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionFrameSlotLayout *unreachableLayout;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    unreachableLayout = &function->childFunctionList[1].frameSlotLayouts[0];

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_malformed_unreachable_frame_layout";
    options.sourceHash = "aot-c-code-stripping-malformed-unreachable-frame-layout";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-malformed-unreachable-frame-layout";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    unreachableLayout->byteAlign = 3u;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "malformed_unreachable_frame_alignment",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    unreachableLayout->byteAlign = 8u;
    unreachableLayout->byteOffset = 8u;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "malformed_unreachable_frame_span",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_legal_frame_alias_layouts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *retained;
    SZrFunctionFrameSlotLayout *layouts;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_legal_frame_alias_layouts";
    options.sourceHash = "aot-c-code-stripping-legal-frame-alias-layouts";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-legal-frame-alias-layouts";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    attach_value_layout_slot(state, function);
    retained = &function->childFunctionList[0];
    ZrCore_Memory_RawFreeWithType(state->global,
                                  retained->frameSlotLayouts,
                                  sizeof(SZrFunctionFrameSlotLayout),
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);
    memset(layouts, 0, sizeof(SZrFunctionFrameSlotLayout) * 2u);
    layouts[0].stackSlot = 0u;
    layouts[0].byteSize = 16u;
    layouts[0].byteAlign = 16u;
    layouts[0].typeLayoutId = 1u;
    layouts[0].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[1] = layouts[0];
    layouts[1].stackSlot = 1u;
    layouts[1].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS;
    retained->stackSize = 2u;
    retained->frameByteSize = 16u;
    retained->frameByteAlign = 16u;
    retained->frameSlotLayoutLength = 2u;
    retained->frameSlotLayouts = layouts;
    initialize_test_union_layout(&function->prototypeFrameTypeLayouts[1], 16u, 16u, 1u);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "legal_overlapping_frame_aliases",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsBefore = 4 */");
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsAfter = 3 */");
    assert_text_contains(generatedCText, "/* reachability.frameLayoutManifest.count = 3 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.frameLayoutManifest.node[0] = reason=edge.frame_layout "
            "predecessorFunction=0 ownerFunction=0 slotLayout=0 stackSlot=0 byteOffset=0 "
            "byteSize=8 byteAlign=8 typeLayoutId=4294967295 slotKind=value isParameter=0 "
            "flags=0x0000 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.frameLayoutManifest.node[1] = reason=edge.frame_layout "
            "predecessorFunction=1 ownerFunction=1 slotLayout=0 stackSlot=0 byteOffset=0 "
            "byteSize=16 byteAlign=16 typeLayoutId=1 slotKind=inline_struct isParameter=0 "
            "flags=0x0000 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.frameLayoutManifest.node[2] = reason=edge.frame_layout "
            "predecessorFunction=1 ownerFunction=1 slotLayout=1 stackSlot=1 byteOffset=0 "
            "byteSize=16 byteAlign=16 typeLayoutId=1 slotKind=inline_struct isParameter=0 "
            "flags=0x0001 */");
    assert_text_does_not_contain(generatedCText, "reachability.frameLayoutManifest.node[3]");
    free(generatedCText);

    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    retained = &function->childFunctionList[0];
    layouts = retained->frameSlotLayouts;
    layouts[0].byteSize = 16u;
    layouts[0].byteAlign = 16u;
    layouts[0].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                           ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS;
    retained->frameByteSize = (TZrUInt32)sizeof(SZrFunctionFrameIndirectAliasBinding);
    retained->frameByteAlign = (TZrUInt32)_Alignof(SZrFunctionFrameIndirectAliasBinding);
    initialize_test_union_layout(&function->prototypeFrameTypeLayouts[1], 16u, 16u, 1u);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "legal_indirect_frame_alias",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    assert_text_contains(generatedCText,
                         "byteSize=16 byteAlign=16 typeLayoutId=1 slotKind=inline_struct "
                         "isParameter=0 flags=0x0003 */");
    free(generatedCText);

    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    retained = &function->childFunctionList[0];
    layouts = retained->frameSlotLayouts;
    layouts[0].byteSize = 16u;
    layouts[0].byteAlign = 16u;
    layouts[0].isParameter = 1u;
    layouts[0].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                           ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                           ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS;
    retained->parameterCount = 1u;
    retained->frameByteSize = (TZrUInt32)sizeof(SZrFunctionFrameBorrowedAliasBinding);
    retained->frameByteAlign = (TZrUInt32)_Alignof(SZrFunctionFrameBorrowedAliasBinding);
    initialize_test_union_layout(&function->prototypeFrameTypeLayouts[1], 16u, 16u, 1u);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "legal_borrowed_frame_alias",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    assert_text_contains(generatedCText,
                         "byteSize=16 byteAlign=16 typeLayoutId=1 slotKind=inline_struct "
                         "isParameter=1 flags=0x0013 */");
    free(generatedCText);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_property_accessor_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_property_accessor_trim_fixture(state, 1u, 0u);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_property_accessor_root";
    options.sourceHash = "aot-c-code-stripping-property-accessor-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-property-accessor-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "property_accessor_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.functionManifest.node[1] = reason=root.property_accessor predecessor=none */");
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsBefore = 0 */");
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsAfter = 0 */");
    assert_text_contains(generatedCText, "/* code_stripping.frameLayoutSlotsRemoved = 0 */");
    assert_text_contains(generatedCText, "/* reachability.frameLayoutManifest.count = 0 */");
    assert_text_does_not_contain(generatedCText, "reachability.frameLayoutManifest.node[0]");
    assert_text_contains(generatedCText, "/* code_stripping.debugLocationsBefore = 0 */");
    assert_text_contains(generatedCText, "/* code_stripping.debugLocationsAfter = 0 */");
    assert_text_contains(generatedCText, "/* code_stripping.debugLocationsRemoved = 0 */");
    assert_text_contains(generatedCText, "/* reachability.debugSidecarManifest.count = 0 */");
    assert_text_does_not_contain(generatedCText, "reachability.debugSidecarManifest.node[0]");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unresolved_property_accessor_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_property_accessor_trim_fixture(state, 3u, 1u);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_unresolved_property_accessor_root";
    options.sourceHash = "aot-c-code-stripping-unresolved-property-accessor-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-unresolved-property-accessor-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "unresolved_property_accessor_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_resource_drop_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_resource_drop_trim_fixture(state, 0u);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_resource_drop_root";
    options.sourceHash = "aot-c-code-stripping-resource-drop-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-resource-drop-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "resource_drop_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.functionManifest.node[1] = reason=root.resource_drop predecessor=none */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unresolved_resource_drop_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_resource_drop_trim_fixture(state, 1u);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_unresolved_resource_drop_root";
    options.sourceHash = "aot-c-code-stripping-unresolved-resource-drop-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-unresolved-resource-drop-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "unresolved_resource_drop_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_reflection_constructor_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_constructor_trim_fixture(state, 0u);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_reflection_constructor_root";
    options.sourceHash = "aot-c-code-stripping-reflection-constructor-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-reflection-constructor-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "reflection_constructor_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.functionManifest.node[1] = reason=root.reflection_constructor predecessor=none */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unresolved_reflection_constructor_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_constructor_trim_fixture(state, 1u);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_unresolved_reflection_constructor_root";
    options.sourceHash = "aot-c-code-stripping-unresolved-reflection-constructor-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-unresolved-reflection-constructor-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "unresolved_reflection_constructor_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_reports_zero_type_layout_frame_edge(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    enable_static_callable_trim_type_layout_zero(function);
    function->childFunctionList[0].frameSlotLayouts[0].typeLayoutId = 0u;

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_zero_type_layout_frame";
    options.sourceHash = "aot-c-code-stripping-zero-type-layout-frame";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-zero-type-layout-frame";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "zero_type_layout_frame",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.count = 1 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.typeLayoutManifest.node[0] = reason=edge.frame_layout predecessorFunction=1 */");
    assert_text_does_not_contain(generatedCText, "reachability.typeLayoutManifest.node[1]");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_reports_zero_type_layout_annotation_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    enable_static_callable_trim_type_layout_zero(function);
    mark_function_dynamic_dependency_type_layout(state, function, 0u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_zero_type_layout_root";
    options.sourceHash = "aot-c-code-stripping-zero-type-layout-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-zero-type-layout-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "zero_type_layout_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.typeLayoutManifest.node[0] = reason=root.reflection_annotation predecessorFunction=none */\n"
            "/* reachability.typeLayoutManifest.node[1] = reason=edge.frame_layout predecessorFunction=1 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_reports_stable_flat_frame_predecessor(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList[0] =
            test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 1u, 0u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_stable_flat_frame_predecessor";
    options.sourceHash = "aot-c-code-stripping-stable-flat-frame-predecessor";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-stable-flat-frame-predecessor";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "stable_flat_frame_predecessor",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(
            generatedCText,
            "/* reachability.typeLayoutManifest.node[2] = reason=edge.frame_layout predecessorFunction=2 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_type_layout_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    mark_function_dynamic_dependency_type_layout(state, function, 2u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_dynamic_dependency_type_layout";
    options.sourceHash = "aot-c-code-stripping-dynamic-dependency-type-layout";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-dynamic-dependency-type-layout";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "dynamic_dependency_type_layout",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "static const SZrTypeLayout ZrTypeLayout_2 = {");
    assert_text_contains(generatedCText,
                         "static const SZrTypeLayout *const zr_aot_type_layouts[] = {\n"
                         "    ZR_NULL,\n"
                         "    &ZrTypeLayout_1,\n"
                         "    &ZrTypeLayout_2,\n"
                         "};");
    assert_text_contains(generatedCText, ".typeLayoutCount = 3,");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoot[0] = 2 */");
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.typeLayoutManifest.node[1] = reason=edge.frame_layout predecessorFunction=1 */\n"
            "/* reachability.typeLayoutManifest.node[2] = reason=root.reflection_annotation predecessorFunction=none */");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 0u);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 1u);
    assert_code_stripping_function_body_bytes_missing(generatedCText, 2u);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_type_token_layout_metadata(void) {
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    metadataBytes = build_zrp_metadata_type_token_layout_fixture(metadataBlob, sizeof(metadataBlob));
    mark_function_dynamic_dependency_type_token(
            state,
            function,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_dynamic_dependency_type_token";
    options.sourceHash = "aot-c-code-stripping-dynamic-dependency-type-token";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-code-stripping-dynamic-dependency-type-token";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytes;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "dynamic_dependency_type_token",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "static const SZrTypeLayout ZrTypeLayout_2 = {");
    assert_text_contains(generatedCText,
                         "static const SZrTypeLayout *const zr_aot_type_layouts[] = {\n"
                         "    ZR_NULL,\n"
                         "    &ZrTypeLayout_1,\n"
                         "    &ZrTypeLayout_2,\n"
                         "};");
    assert_text_contains(generatedCText,
                         "static const TZrUInt32 zr_aot_type_layout_tokens[] = {\n"
                         "    0u,\n"
                         "    0u,\n"
                         "    0x02000001u,\n"
                         "};");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoot[0] = 2 */");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_function_body_bytes_missing(generatedCText, 2u);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_type_ref_token_layout_metadata(void) {
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    metadataBytes = build_zrp_metadata_type_ref_token_layout_fixture(metadataBlob, sizeof(metadataBlob));
    mark_function_dynamic_dependency_type_token(
            state,
            function,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_dynamic_dependency_type_ref_token";
    options.sourceHash = "aot-c-code-stripping-dynamic-dependency-type-ref-token";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-code-stripping-dynamic-dependency-type-ref-token";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytes;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "dynamic_dependency_type_ref_token",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "static const SZrTypeLayout ZrTypeLayout_2 = {");
    assert_text_contains(generatedCText,
                         "static const TZrUInt32 zr_aot_type_layout_tokens[] = {\n"
                         "    0u,\n"
                         "    0u,\n"
                         "    0x02000001u,\n"
                         "};");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoot[0] = 2 */");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_function_body_bytes_missing(generatedCText, 2u);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_type_spec_token_layout_metadata(void) {
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    metadataBytes = build_zrp_metadata_type_spec_token_layout_fixture(metadataBlob, sizeof(metadataBlob));
    mark_function_dynamic_dependency_type_token(
            state,
            function,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_dynamic_dependency_type_spec_token";
    options.sourceHash = "aot-c-code-stripping-dynamic-dependency-type-spec-token";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-code-stripping-dynamic-dependency-type-spec-token";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytes;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "dynamic_dependency_type_spec_token",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "static const SZrTypeLayout ZrTypeLayout_2 = {");
    assert_text_contains(generatedCText,
                         "static const TZrUInt32 zr_aot_type_layout_tokens[] = {\n"
                         "    0u,\n"
                         "    0u,\n"
                         "    0x07000001u,\n"
                         "};");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoot[0] = 2 */");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_function_body_bytes_missing(generatedCText, 2u);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_field_token_layout_metadata(void) {
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    metadataBytes = build_zrp_metadata_field_token_layout_fixture(metadataBlob, sizeof(metadataBlob));
    mark_function_dynamic_dependency_field_token(
            state,
            function,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_dynamic_dependency_field_token";
    options.sourceHash = "aot-c-code-stripping-dynamic-dependency-field-token";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-code-stripping-dynamic-dependency-field-token";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytes;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "dynamic_dependency_field_token",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "static const SZrTypeLayout ZrTypeLayout_1 = {");
    assert_text_contains(generatedCText, "static const SZrTypeLayout ZrTypeLayout_2 = {");
    assert_text_contains(generatedCText,
                         "static const SZrTypeLayout *const zr_aot_type_layouts[] = {\n"
                         "    ZR_NULL,\n"
                         "    &ZrTypeLayout_1,\n"
                         "    &ZrTypeLayout_2,\n"
                         "};");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoots = 2 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoot[0] = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationTypeLayoutRoot[1] = 2 */");
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.typeLayoutManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.typeLayoutManifest.node[1] = reason=root.reflection_annotation predecessorFunction=none */\n"
            "/* reachability.typeLayoutManifest.node[2] = reason=root.reflection_annotation predecessorFunction=none */");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_TRUE);
    assert_code_stripping_function_body_bytes_missing(generatedCText, 2u);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_option_preserves_exported_callable_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    add_exported_second_child_callable_binding(state, function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_export_root";
    options.sourceHash = "aot-c-code-stripping-export-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-export-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "static_callable_export_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 3u, 0u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 0u);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 1u);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 2u);
    assert_code_stripping_function_body_bytes_total_present(generatedCText);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 3 */");
    assert_text_contains(generatedCText,
                         "/* reachability.functionManifest.node[2] = reason=root.export predecessor=none */");
    assert_text_contains(generatedCText,
                         "static const FZrAotEntryThunk zr_aot_function_thunks[] = {\n"
                         "    zr_aot_fn_0,\n"
                         "    zr_aot_fn_1,\n"
                         "    zr_aot_fn_2,\n"
                         "};");
    assert_text_contains(generatedCText,
                         "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {\n"
                         "    &zr_aot_method_info_0,\n"
                         "    &zr_aot_method_info_1,\n"
                         "    &zr_aot_method_info_2,\n"
                         "};");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_option_preserves_manifest_function_root(void) {
    static const TZrUInt32 manifestRoots[] = {2u};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_manifest_root";
    options.sourceHash = "aot-c-code-stripping-manifest-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-manifest-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.manifestPreserveFunctionFlatIndices = manifestRoots;
    options.manifestPreserveFunctionFlatIndexCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "static_callable_manifest_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 3u, 0u);
    assert_code_stripping_type_layout_stats(generatedCText, 2u, 2u, 0u);
    assert_code_stripping_type_layout_byte_stats(generatedCText, 16u, 16u, 0u);
    assert_code_stripping_type_layout_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_method_metadata_generated_byte_stats(generatedCText, ZR_FALSE);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 0u);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 1u);
    assert_code_stripping_function_body_bytes_contains(generatedCText, 2u);
    assert_code_stripping_function_body_bytes_total_present(generatedCText);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 3 */");
    assert_text_contains(generatedCText,
                         "/* reachability.functionManifest.node[2] = reason=root.manifest predecessor=none */");
    assert_text_contains(generatedCText,
                         "static const FZrAotEntryThunk zr_aot_function_thunks[] = {\n"
                         "    zr_aot_fn_0,\n"
                         "    zr_aot_fn_1,\n"
                         "    zr_aot_fn_2,\n"
                         "};");
    assert_text_contains(generatedCText,
                         "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {\n"
                         "    &zr_aot_method_info_0,\n"
                         "    &zr_aot_method_info_1,\n"
                         "    &zr_aot_method_info_2,\n"
                         "};");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_generic_methodspec_root(void) {
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotManifestGenericRoot genericRoot;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    add_typed_second_child_method_token(state, function, methodToken);

    memset(&genericRoot, 0, sizeof(genericRoot));
    genericRoot.target = "Factory.make";
    genericRoot.hasMethodSpecBinding = ZR_TRUE;
    genericRoot.methodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u);
    genericRoot.methodSpecMethodToken = methodToken;
    genericRoot.methodSpecSignatureHash = (TZrUInt64)0x2233445566778899ULL;
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_generic_methodspec_root";
    options.sourceHash = "aot-c-code-stripping-generic-methodspec-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-generic-methodspec-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.manifestPreserveGenericRoots = &genericRoot;
    options.manifestPreserveGenericRootCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "generic_methodspec_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 3u, 0u);
    assert_text_contains(generatedCText, "/* manifest.genericRoots = 1 */");
    assert_text_contains(generatedCText,
                         "/* manifest.genericRoot[0].methodSpec.methodToken = 0x03000007 */");
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 3 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.functionManifest.node[2] = reason=root.generic_methodspec predecessor=none */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unresolved_generic_methodspec_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotManifestGenericRoot genericRoot;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&genericRoot, 0, sizeof(genericRoot));
    genericRoot.target = "Factory.missing";
    genericRoot.hasMethodSpecBinding = ZR_TRUE;
    genericRoot.methodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 3u);
    genericRoot.methodSpecMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 99u);
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_unresolved_generic_methodspec_root";
    options.sourceHash = "aot-c-code-stripping-unresolved-generic-methodspec-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-unresolved-generic-methodspec-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.manifestPreserveGenericRoots = &genericRoot;
    options.manifestPreserveGenericRootCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "unresolved_generic_methodspec_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_package_method_export_root(void) {
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 11u);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotManifestExportDeclaration declaration;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    add_typed_second_child_method_token(state, function, methodToken);

    memset(&declaration, 0, sizeof(declaration));
    declaration.kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declaration.target = "Factory.make";
    declaration.hasMemberTokenBinding = ZR_TRUE;
    declaration.memberToken = methodToken;
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_package_method_export_root";
    options.sourceHash = "aot-c-code-stripping-package-method-export-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-package-method-export-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.manifestExportDeclarations = &declaration;
    options.manifestExportDeclarationCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "package_method_export_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 3u, 0u);
    assert_text_contains(generatedCText, "/* manifest.exports = 1 */");
    assert_text_contains(generatedCText, "/* manifest.export[0].memberToken = 0x0300000b */");
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 3 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.functionManifest.node[2] = reason=root.package_export predecessor=none */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unresolved_package_method_export_root(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotManifestExportDeclaration declaration;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&declaration, 0, sizeof(declaration));
    declaration.kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declaration.target = "Factory.missing";
    declaration.hasMemberTokenBinding = ZR_TRUE;
    declaration.memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 99u);
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_unresolved_package_method_export_root";
    options.sourceHash = "aot-c-code-stripping-unresolved-package-method-export-root";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-unresolved-package-method-export-root";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.manifestExportDeclarations = &declaration;
    options.manifestExportDeclarationCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "unresolved_package_method_export_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_reports_native_callback_materialization_edge(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    add_native_callback_escape_binding(state, function, 0u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_native_callback_edge";
    options.sourceHash = "aot-c-code-stripping-native-callback-edge";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-native-callback-edge";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "native_callback_edge",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_text_contains(generatedCText, "/* reachability.functionManifest.count = 2 */");
    assert_text_contains(
            generatedCText,
            "/* reachability.functionManifest.node[1] = reason=edge.native_callback predecessor=0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_malformed_native_callback_escape_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->escapeBindingLength = 1u;

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_malformed_native_callback_escape_metadata";
    options.sourceHash = "aot-c-code-stripping-malformed-native-callback-escape-metadata";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-code-stripping-malformed-native-callback-escape-metadata";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "malformed_native_callback_escape_metadata",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_reports_native_import_contract_reachability(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;
    const char *manifestNode0;
    const char *manifestNode1;
    const char *manifestNode2;
    const char *retainedEntryA;
    const char *retainedEntryB;
    const char *retainedSparseEntry;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList[0].instruction.operand.operand1[0] = 1u;
    add_native_import_contract(state, function, 0x101u, "retained_native_a");
    add_native_import_contract(state, function, 0x102u, "retained_native_b");
    add_native_import_contract(state,
                               &function->childFunctionList[0],
                               0x202u,
                               "trimmed_native");
    add_native_import_contract(state,
                               &function->childFunctionList[1],
                               0x301u,
                               "retained_sparse_native");

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_native_import_contract_reachability";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "native-import-contract-reachability";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "native_import_contract_reachability",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.nativeImportsBefore = 4 */");
    assert_text_contains(generatedCText, "/* code_stripping.nativeImportsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.nativeImportsRemoved = 1 */");
    assert_text_contains(generatedCText, "/* reachability.nativeImportManifest.version = 1 */");
    assert_text_contains(generatedCText, "/* reachability.nativeImportManifest.count = 3 */");
    manifestNode0 = strstr(
            generatedCText,
            "/* reachability.nativeImportManifest.node[0] = ownerFunction=0 contractIndex=0 symbolId=0x0000000000000101 callableContractHash=");
    manifestNode1 = strstr(
            generatedCText,
            "/* reachability.nativeImportManifest.node[1] = ownerFunction=0 contractIndex=1 symbolId=0x0000000000000102 callableContractHash=");
    manifestNode2 = strstr(
            generatedCText,
            "/* reachability.nativeImportManifest.node[2] = ownerFunction=2 contractIndex=0 symbolId=0x0000000000000301 callableContractHash=");
    TEST_ASSERT_NOT_NULL(manifestNode0);
    TEST_ASSERT_NOT_NULL(manifestNode1);
    TEST_ASSERT_NOT_NULL(manifestNode2);
    TEST_ASSERT_TRUE(manifestNode0 < manifestNode1);
    TEST_ASSERT_TRUE(manifestNode1 < manifestNode2);
    assert_text_contains(generatedCText, "reason=edge.native_import predecessor=0 */");
    assert_text_contains(generatedCText, "reason=edge.native_import predecessor=2 */");
    assert_text_contains(
            generatedCText,
            "static const SZrAotNativeImportRange zr_aot_native_import_ranges[] = {\n"
            "    { .contractStart = 0u, .contractCount = 2u },\n"
            "    { .contractStart = 2u, .contractCount = 0u },\n"
            "    { .contractStart = 2u, .contractCount = 1u },\n"
            "};");
    retainedEntryA = strstr(generatedCText, ".entryPoint = \"retained_native_a\"");
    retainedEntryB = strstr(generatedCText, ".entryPoint = \"retained_native_b\"");
    retainedSparseEntry = strstr(
            generatedCText, ".entryPoint = \"retained_sparse_native\"");
    TEST_ASSERT_NOT_NULL(retainedEntryA);
    TEST_ASSERT_NOT_NULL(retainedEntryB);
    TEST_ASSERT_NOT_NULL(retainedSparseEntry);
    TEST_ASSERT_TRUE(retainedEntryA < retainedEntryB);
    TEST_ASSERT_TRUE(retainedEntryB < retainedSparseEntry);
    assert_text_does_not_contain(generatedCText, "trimmed_native");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_malformed_unreachable_native_import_contract(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    add_native_import_contract(state,
                               &function->childFunctionList[1],
                               0x202u,
                               "malformed_trimmed_native");
    function->childFunctionList[1].nativeImportContracts[0].schemaVersion = 0u;

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_malformed_unreachable_native_import";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "malformed-unreachable-native-import";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "malformed_unreachable_native_import",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_reports_zrp_metadata_section_table_pool_byte_stats(void) {
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    TZrSize tokenRecordBytes;
    TZrSize definitionTableBytes;
    TZrSize poolBytes;
    TZrSize typeDefBytes;
    TZrSize stringPoolBytes;
    TZrSize signatureBlobPoolBytes;
    TZrSize constantPoolBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    metadataBytes = build_zrp_metadata_size_fixture(metadataBlob,
                                                    sizeof(metadataBlob),
                                                    &tokenRecordBytes,
                                                    &definitionTableBytes,
                                                    &poolBytes,
                                                    &typeDefBytes,
                                                    &stringPoolBytes,
                                                    &signatureBlobPoolBytes,
                                                    &constantPoolBytes);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_zrp_metadata_size";
    options.sourceHash = "aot-c-code-stripping-zrp-metadata-size";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-code-stripping-zrp-metadata-size";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytes;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "zrp_metadata_size",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_zrp_metadata_size_stats(generatedCText,
                                   metadataBytes,
                                   tokenRecordBytes,
                                   definitionTableBytes,
                                   poolBytes,
                                   typeDefBytes,
                                   stringPoolBytes,
                                   signatureBlobPoolBytes,
                                   constantPoolBytes);
    assert_zrp_metadata_code_stripping_delta_stats(generatedCText,
                                                   metadataBytes,
                                                   tokenRecordBytes,
                                                   definitionTableBytes,
                                                   poolBytes);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_prunes_zrp_method_defs_for_removed_functions(void) {
    TZrByte metadataBlob[768];
    TZrSize metadataBytesBeforeTrim;
    TZrSize metadataBytesAfterTrim;
    TZrSize tokenRecordBytes;
    TZrSize definitionTableBytesBeforeTrim;
    TZrSize definitionTableBytesAfterTrim;
    TZrSize poolBytesBeforeTrim;
    TZrSize poolBytesAfterTrim;
    TZrSize typeDefBytes;
    TZrSize methodDefBytesBeforeTrim;
    TZrSize methodDefBytesAfterTrim;
    TZrSize stringPoolBytesBeforeTrim;
    TZrSize stringPoolBytesAfterTrim;
    TZrSize signatureBlobPoolBytesBeforeTrim;
    TZrSize signatureBlobPoolBytesAfterTrim;
    TZrSize constantPoolBytesBeforeTrim;
    TZrSize constantPoolBytesAfterTrim;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_static_callable_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    add_typed_exported_first_child_method_token(state,
                                                function,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u));
    metadataBytesBeforeTrim =
            build_zrp_metadata_method_def_trim_fixture(metadataBlob,
                                                       sizeof(metadataBlob),
                                                       &metadataBytesAfterTrim,
                                                       &tokenRecordBytes,
                                                       &definitionTableBytesBeforeTrim,
                                                       &definitionTableBytesAfterTrim,
                                                       &poolBytesBeforeTrim,
                                                       &poolBytesAfterTrim,
                                                       &typeDefBytes,
                                                       &methodDefBytesBeforeTrim,
                                                       &methodDefBytesAfterTrim,
                                                       &stringPoolBytesBeforeTrim,
                                                       &stringPoolBytesAfterTrim,
                                                       &signatureBlobPoolBytesBeforeTrim,
                                                       &signatureBlobPoolBytesAfterTrim,
                                                       &constantPoolBytesBeforeTrim,
                                                       &constantPoolBytesAfterTrim);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_code_stripping_zrp_metadata_prune";
    options.sourceHash = "aot-c-code-stripping-zrp-metadata-prune";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-code-stripping-zrp-metadata-prune";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytesBeforeTrim;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_code_stripping",
                                                       "generated",
                                                       "zrp_metadata_method_def_prune",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_code_stripping_stats(generatedCText, 3u, 2u, 1u);
    assert_descriptor_embedded_module_length_marker(generatedCText,
                                                    (unsigned long long)metadataBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "embeddedModuleBytes",
                                    (unsigned long long)metadataBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataBytes",
                                    (unsigned long long)metadataBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataTokenRecordBytes",
                                    (unsigned long long)tokenRecordBytes);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataDefinitionTableBytes",
                                    (unsigned long long)definitionTableBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText, "zrpMetadataPoolBytes", (unsigned long long)poolBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataSectionBytes.typeDefs",
                                    (unsigned long long)typeDefBytes);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataSectionBytes.methodDefs",
                                    (unsigned long long)methodDefBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataSectionBytes.stringPool",
                                    (unsigned long long)stringPoolBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataSectionBytes.signatureBlobPool",
                                    (unsigned long long)signatureBlobPoolBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText,
                                    "zrpMetadataSectionBytes.constantPool",
                                    (unsigned long long)constantPoolBytesAfterTrim);
    assert_zrp_metadata_size_marker(generatedCText, "zrpMetadataSectionBytes.manifestExports", 0u);
    assert_zrp_metadata_size_marker(generatedCText, "zrpMetadataSectionCounts.manifestExports", 0u);
    assert_text_contains(generatedCText,
                         "static const TZrUInt32 zr_aot_method_tokens[] = {\n"
                         "    0u,\n"
                         "    0x03000001u,\n"
                         "    0u,\n"
                         "};");
    assert_text_contains(generatedCText, "/* code_stripping.memberTokenRemaps = 1 */");
    assert_text_contains(generatedCText,
                         "/* code_stripping.memberTokenRemap[0].sourceToken = 0x03000002 */");
    assert_text_contains(generatedCText,
                         "/* code_stripping.memberTokenRemap[0].targetToken = 0x03000001 */");
    assert_text_contains(generatedCText,
                         "static const SZrAotMemberTokenRemap zr_aot_member_token_remaps[] = {\n"
                         "    { .sourceToken = 0x03000002u, .targetToken = 0x03000001u },\n"
                         "};");
    assert_text_contains(generatedCText,
                         "    .methodTokenCount = 3,\n"
                         "    .memberTokenRemaps = zr_aot_member_token_remaps,\n"
                         "    .memberTokenRemapCount = 1u,\n"
                         "    .manifestExports = ZR_NULL,\n"
                         "    .manifestExportCount = 0u,\n"
                         "    .invokers = zr_aot_reflection_invokers,");
    assert_text_contains(generatedCText,
                         "    .methodTokenCount = 3,\n"
                         "    .memberTokenRemaps = zr_aot_member_token_remaps,\n"
                         "    .memberTokenRemapCount = 1u,\n"
                         "    .manifestExports = ZR_NULL,\n"
                         "    .manifestExportCount = 0u,\n"
                         "    .typeLayouts = ");
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataBytesBefore",
                                                   (unsigned long long)metadataBytesBeforeTrim);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataBytesAfter",
                                                   (unsigned long long)metadataBytesAfterTrim);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataBytesRemoved",
                                                   (unsigned long long)(metadataBytesBeforeTrim -
                                                                        metadataBytesAfterTrim));
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataDefinitionTableBytesBefore",
                                                   (unsigned long long)definitionTableBytesBeforeTrim);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataDefinitionTableBytesAfter",
                                                   (unsigned long long)definitionTableBytesAfterTrim);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataDefinitionTableBytesRemoved",
                                                   (unsigned long long)(definitionTableBytesBeforeTrim -
                                                                        definitionTableBytesAfterTrim));
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataPoolBytesBefore",
                                                   (unsigned long long)poolBytesBeforeTrim);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataPoolBytesAfter",
                                                   (unsigned long long)poolBytesAfterTrim);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataPoolBytesRemoved",
                                                   (unsigned long long)(poolBytesBeforeTrim - poolBytesAfterTrim));
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataSectionBytes.manifestExportsBefore",
                                                   0u);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataSectionBytes.manifestExportsAfter",
                                                   0u);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataSectionBytes.manifestExportsRemoved",
                                                   0u);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataSectionCounts.manifestExportsBefore",
                                                   0u);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataSectionCounts.manifestExportsAfter",
                                                   0u);
    assert_code_stripping_zrp_metadata_size_marker(generatedCText,
                                                   "zrpMetadataSectionCounts.manifestExportsRemoved",
                                                   0u);
    TEST_ASSERT_EQUAL_UINT64((methodDefBytesBeforeTrim - methodDefBytesAfterTrim) +
                                     (stringPoolBytesBeforeTrim - stringPoolBytesAfterTrim) +
                                     (signatureBlobPoolBytesBeforeTrim - signatureBlobPoolBytesAfterTrim) +
                                     (constantPoolBytesBeforeTrim - constantPoolBytesAfterTrim),
                             metadataBytesBeforeTrim - metadataBytesAfterTrim);
    TEST_ASSERT_EQUAL_UINT64(methodDefBytesBeforeTrim - methodDefBytesAfterTrim,
                             definitionTableBytesBeforeTrim - definitionTableBytesAfterTrim);
    TEST_ASSERT_EQUAL_UINT64((stringPoolBytesBeforeTrim - stringPoolBytesAfterTrim) +
                                     (signatureBlobPoolBytesBeforeTrim - signatureBlobPoolBytesAfterTrim) +
                                     (constantPoolBytesBeforeTrim - constantPoolBytesAfterTrim),
                             poolBytesBeforeTrim - poolBytesAfterTrim);

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_code_stripping_option_filters_unreachable_static_callable);
    RUN_TEST(test_aot_c_code_stripping_rejects_malformed_unreachable_debug_sidecar);
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_retained_frame_type_layout);
    RUN_TEST(test_aot_c_code_stripping_rejects_malformed_unreachable_frame_type_layout);
    RUN_TEST(test_aot_c_code_stripping_rejects_unreachable_materialized_parameter_undercount);
    RUN_TEST(test_aot_c_code_stripping_rejects_malformed_unreachable_constructor_bitmap);
    RUN_TEST(test_aot_c_code_stripping_rejects_malformed_unreachable_frame_layout);
    RUN_TEST(test_aot_c_code_stripping_preserves_legal_frame_alias_layouts);
    RUN_TEST(test_aot_c_code_stripping_preserves_property_accessor_root);
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_property_accessor_root);
    RUN_TEST(test_aot_c_code_stripping_preserves_resource_drop_root);
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_resource_drop_root);
    RUN_TEST(test_aot_c_code_stripping_preserves_reflection_constructor_root);
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_reflection_constructor_root);
    RUN_TEST(test_aot_c_code_stripping_reports_zero_type_layout_frame_edge);
    RUN_TEST(test_aot_c_code_stripping_reports_zero_type_layout_annotation_root);
    RUN_TEST(test_aot_c_code_stripping_reports_stable_flat_frame_predecessor);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_type_layout_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_type_token_layout_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_type_ref_token_layout_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_type_spec_token_layout_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_field_token_layout_metadata);
    RUN_TEST(test_aot_c_code_stripping_option_preserves_exported_callable_root);
    RUN_TEST(test_aot_c_code_stripping_option_preserves_manifest_function_root);
    RUN_TEST(test_aot_c_code_stripping_preserves_generic_methodspec_root);
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_generic_methodspec_root);
    RUN_TEST(test_aot_c_code_stripping_preserves_package_method_export_root);
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_package_method_export_root);
    RUN_TEST(test_aot_c_code_stripping_reports_native_callback_materialization_edge);
    RUN_TEST(test_aot_c_code_stripping_rejects_malformed_native_callback_escape_metadata);
    RUN_TEST(test_aot_c_code_stripping_reports_native_import_contract_reachability);
    RUN_TEST(test_aot_c_code_stripping_rejects_malformed_unreachable_native_import_contract);
    RUN_TEST(test_aot_c_reports_zrp_metadata_section_table_pool_byte_stats);
    RUN_TEST(test_aot_c_code_stripping_prunes_zrp_method_defs_for_removed_functions);
    return UNITY_END();
}
