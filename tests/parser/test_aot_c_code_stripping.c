#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/zrp_metadata.h"
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
        root->prototypeFrameTypeLayouts[typeLayoutId].kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION;
        root->prototypeFrameTypeLayouts[typeLayoutId].byteSize = 8u;
        root->prototypeFrameTypeLayouts[typeLayoutId].byteAlign = 8u;
        root->prototypeFrameTypeLayouts[typeLayoutId].cTypeId = typeLayoutId;
        root->prototypeFrameTypeLayouts[typeLayoutId].blittable = ZR_TRUE;
        root->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_AOT_TEST_TYPE_LAYOUT_CACHE_READY;
    }
}

static void enable_static_callable_trim_type_layout_zero(SZrFunction *root) {
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(root->prototypeFrameTypeLayouts);
    TEST_ASSERT_NOT_NULL(root->prototypeFrameTypeLayoutStates);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, root->prototypeFrameTypeLayoutLength);

    root->prototypeFrameTypeLayouts[0].kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION;
    root->prototypeFrameTypeLayouts[0].byteSize = 8u;
    root->prototypeFrameTypeLayouts[0].byteAlign = 8u;
    root->prototypeFrameTypeLayouts[0].cTypeId = 0u;
    root->prototypeFrameTypeLayouts[0].blittable = ZR_TRUE;
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
    RUN_TEST(test_aot_c_code_stripping_rejects_unresolved_retained_frame_type_layout);
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
    RUN_TEST(test_aot_c_reports_zrp_metadata_section_table_pool_byte_stats);
    RUN_TEST(test_aot_c_code_stripping_prunes_zrp_method_defs_for_removed_functions);
    return UNITY_END();
}
