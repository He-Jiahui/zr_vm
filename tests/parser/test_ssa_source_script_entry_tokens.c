#include <string.h>

/* Parse CRT headers before Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/zrp_metadata.h"
#include "zr_vm_parser/compiler.h"

/* Only inspect ordinary Source_Compile results. No execution, synthetic export,
 * manufactured token, retained canonical graph, or artifact publication. */
static SZrState *g_state;
static SZrFunction *g_entries[2];
static TZrBool g_rooted[2];

void setUp(void) {
    memset(g_entries, 0, sizeof(g_entries));
    memset(g_rooted, 0, sizeof(g_rooted));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    if (g_state == ZR_NULL) return;
    for (index = 0u; index < 2u; ++index) {
        if (g_entries[index] == ZR_NULL) continue;
        if (g_rooted[index] != ZR_FALSE) {
            ZrCore_GarbageCollector_UnignoreObject(g_state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(g_entries[index]));
        }
        ZrCore_Function_Free(g_state, g_entries[index]);
        g_entries[index] = ZR_NULL;
    }
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrFunction *compile_public_entry(const char *source, TZrUInt32 index) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state,
            index == 0u ? "ssa_entry_tokens_first.zr" : "ssa_entry_tokens_second.zr");
    SZrFunction *entry;
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: source name allocation");
    entry = ZrParser_Source_Compile(g_state, source, strlen(source), name);
    g_entries[index] = entry;
    TEST_ASSERT_NOT_NULL_MESSAGE(entry,
            "PRECONDITION: legal script must compile through public Source_Compile");
    g_rooted[index] = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(entry));
    TEST_ASSERT_TRUE_MESSAGE(g_rooted[index], "PRECONDITION: root returned entry");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, entry->parameterCount, "PRECONDITION: no parameters");
    TEST_ASSERT_FALSE_MESSAGE(entry->hasVariableArguments, "PRECONDITION: no varargs");
    TEST_ASSERT_EQUAL_UINT32(0u, entry->childFunctionLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->moduleEntryEffectLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->typedExportedSymbolLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->exportedVariableLength);
    return entry;
}

static void assert_i64_precondition(const SZrFunction *entry) {
    TEST_ASSERT_TRUE_MESSAGE(entry->hasCallableReturnType,
            "PRECONDITION: prior public script callable-return gate");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_VALUE_TYPE_INT64, entry->callableReturnType.baseType,
            "PRECONDITION: prior public script i64 return");
    TEST_ASSERT_FALSE(entry->callableReturnType.isNullable);
    TEST_ASSERT_FALSE(entry->callableReturnType.isArray);
    TEST_ASSERT_NULL(entry->callableReturnType.typeName);
}

static const SZrMetadataTokenRecord *unique_module_record(const SZrFunction *entry) {
    const SZrMetadataTokenRecord *module = ZR_NULL;
    TZrUInt32 index;
    if (entry->metadataTokenRecords == ZR_NULL) return ZR_NULL;
    for (index = 0u; index < entry->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *row = &entry->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(row->token) != ZR_METADATA_TABLE_MODULE) continue;
        TEST_ASSERT_NULL_MESSAGE(module, "script has duplicate MODULE definitions");
        module = row;
    }
    return module;
}

static const TZrByte *assert_signature_pair(const SZrFunction *entry,
        const SZrMetadataTokenRecord *entity, EZrMetadataSignatureNode rootKind) {
    const SZrMetadataTokenRecord *signature;
    const TZrByte *blob;
    TEST_ASSERT_NOT_EQUAL(0u, ZR_METADATA_TOKEN_RID(entity->token));
    TEST_ASSERT_TRUE(ZrCore_Function_FindMetadataTokenRecord(entry, entity->token) == entity);
    signature = ZrCore_Function_FindMetadataSignatureRecord(entry, entity->token);
    TEST_ASSERT_NOT_NULL_MESSAGE(signature, "script entity lacks a paired SIGNATURE row");
    TEST_ASSERT_EQUAL_UINT32(entity->relatedToken, signature->token);
    TEST_ASSERT_EQUAL_UINT32(entity->token, signature->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(entity->token, signature->relatedToken);
    TEST_ASSERT_NOT_EQUAL(0u, ZR_METADATA_TOKEN_RID(signature->token));
    TEST_ASSERT_EQUAL_UINT32(entity->signatureBlobOffset, signature->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(entity->signatureBlobLength, signature->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT64(entity->signatureHash, signature->signatureHash);
    TEST_ASSERT_TRUE(entity->signatureHash != 0u);
    TEST_ASSERT_NOT_NULL(entry->signatureBlobHeap);
    TEST_ASSERT_TRUE(entity->signatureBlobLength > 0u);
    TEST_ASSERT_TRUE(entity->signatureBlobOffset <= entry->signatureBlobHeapLength);
    TEST_ASSERT_TRUE(entity->signatureBlobLength <=
            entry->signatureBlobHeapLength - entity->signatureBlobOffset);
    blob = entry->signatureBlobHeap + entity->signatureBlobOffset;
    TEST_ASSERT_EQUAL_UINT8(rootKind, blob[0]);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ZrpMetadata_ValidateSignatureBlob(blob,
            entity->signatureBlobLength), "script entity signature is malformed");
    return blob;
}

static TZrUInt32 read_u32_le(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] | ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) | ((TZrUInt32)bytes[3] << 24u);
}

static void assert_script_entry_tokens(const SZrFunction *entry) {
    const SZrMetadataTokenRecord *module = unique_module_record(entry);
    const SZrMetadataTokenRecord *method = ZR_NULL;
    const TZrByte *blob;
    TZrUInt32 index;
    TEST_ASSERT_NOT_NULL_MESSAGE(module,
            "zero-export script did not publish its MODULE definition");
    assert_signature_pair(entry, module, ZR_METADATA_SIGNATURE_NODE_MODULE);
    TEST_ASSERT_TRUE_MESSAGE(entry->moduleSignatureHash != 0u,
            "zero-export script did not publish its module signature hash");
    /* MEMBER_DEF is the existing token table for method definitions. The pure
     * script has no exports, child functions or callable constants to alias. */
    for (index = 0u; index < entry->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *row = &entry->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(row->token) != ZR_METADATA_TABLE_MEMBER_DEF ||
            row->ownerToken != module->token) continue;
        TEST_ASSERT_NULL_MESSAGE(method, "script has ambiguous owned entry definitions");
        method = row;
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(method,
            "zero-export script did not publish its entry method definition");
    blob = assert_signature_pair(entry, method, ZR_METADATA_SIGNATURE_NODE_METHOD_SIG);
    /* Existing metadata method format: node, version, generic arity, reserved
     * u32, primitive return node/u32, parameter count. No canonical-ID guess. */
    TEST_ASSERT_EQUAL_UINT32(16u, method->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT8(1u, blob[1]);
    TEST_ASSERT_EQUAL_UINT8(0u, blob[2]);
    TEST_ASSERT_EQUAL_UINT32(0u, read_u32_le(blob + 3u));
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, blob[7]);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, read_u32_le(blob + 8u));
    TEST_ASSERT_EQUAL_UINT32(0u, read_u32_le(blob + 12u));
}

static TZrUInt64 signature_heap_hash(const SZrFunction *entry) {
    if (entry->signatureBlobHeap == ZR_NULL || entry->signatureBlobHeapLength == 0u) return 0u;
    return ZrCore_Hash_CreateStable64(entry->signatureBlobHeap, entry->signatureBlobHeapLength);
}

static void assert_public_pair(const char *source, const char *secondSource) {
    SZrFunction *first = compile_public_entry(source, 0u);
    TZrUInt32 rowCount = first->metadataTokenRecordLength;
    TZrUInt32 heapLength = first->signatureBlobHeapLength;
    TZrUInt64 moduleHash = first->moduleSignatureHash;
    TZrUInt64 heapHash = signature_heap_hash(first);
    const SZrMetadataTokenRecord *rows = first->metadataTokenRecords;
    const TZrByte *heap = first->signatureBlobHeap;
    SZrFunction *second;
    assert_i64_precondition(first);
    second = compile_public_entry(secondSource, 1u);
    assert_i64_precondition(second);
    TEST_ASSERT_EQUAL_UINT32(rowCount, first->metadataTokenRecordLength);
    TEST_ASSERT_EQUAL_UINT32(heapLength, first->signatureBlobHeapLength);
    TEST_ASSERT_EQUAL_UINT64(moduleHash, first->moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(heapHash, signature_heap_hash(first));
    TEST_ASSERT_TRUE(rows == first->metadataTokenRecords);
    TEST_ASSERT_TRUE(heap == first->signatureBlobHeap);
    assert_script_entry_tokens(first);
    assert_script_entry_tokens(second);
}

static void test_less_true_has_script_entry_tokens(void) {
    assert_public_pair("if (1 < 2) { return 9; } else { return 8; }\n",
            "if (2 < 1) { return 9; } else { return 8; }\n");
}
static void test_less_swapped_false_has_script_entry_tokens(void) {
    assert_public_pair("if (2 < 1) { return 9; } else { return 8; }\n",
            "if (1 < 2) { return 9; } else { return 8; }\n");
}
static void test_greater_true_has_script_entry_tokens(void) {
    assert_public_pair("if (2 > 1) { return 9; } else { return 8; }\n",
            "if (1 > 2) { return 9; } else { return 8; }\n");
}
static void test_greater_swapped_false_has_script_entry_tokens(void) {
    assert_public_pair("if (1 > 2) { return 9; } else { return 8; }\n",
            "if (2 > 1) { return 9; } else { return 8; }\n");
}

static void assert_no_script_entry_definition(const char *source) {
    const SZrFunction *entry = compile_public_entry(source, 0u);
    const SZrMetadataTokenRecord *module = unique_module_record(entry);
    TZrUInt32 index;
    TEST_ASSERT_FALSE_MESSAGE(entry->hasCallableReturnType != ZR_FALSE &&
            entry->callableReturnType.baseType == ZR_VALUE_TYPE_INT64,
            "incompatible return path must not advertise i64");
    /* Local bindings can legitimately publish TYPE_SPEC metadata. */
    if (module == ZR_NULL) return;
    for (index = 0u; index < entry->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *row = &entry->metadataTokenRecords[index];
        TEST_ASSERT_FALSE_MESSAGE(ZR_METADATA_TOKEN_TABLE(row->token) ==
                ZR_METADATA_TABLE_MEMBER_DEF && row->ownerToken == module->token,
                "non-admitted script published an entry method definition");
    }
}

static void test_fallthrough_has_no_script_entry_definition(void) {
    assert_no_script_entry_definition("if (1 < 2) { return 9; }\n");
}
static void test_mixed_bool_has_no_script_entry_definition(void) {
    assert_no_script_entry_definition("if (1 < 2) { return 9; } else { return false; }\n");
}
static void test_none_return_has_no_script_entry_definition(void) {
    assert_no_script_entry_definition("if (1 < 2) { return 9; } else { return; }\n");
}
static void test_implicit_return_has_no_script_entry_definition(void) {
    assert_no_script_entry_definition("let x = 1;\n");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_less_true_has_script_entry_tokens);
    RUN_TEST(test_less_swapped_false_has_script_entry_tokens);
    RUN_TEST(test_greater_true_has_script_entry_tokens);
    RUN_TEST(test_greater_swapped_false_has_script_entry_tokens);
    RUN_TEST(test_fallthrough_has_no_script_entry_definition);
    RUN_TEST(test_mixed_bool_has_no_script_entry_definition);
    RUN_TEST(test_none_return_has_no_script_entry_definition);
    RUN_TEST(test_implicit_return_has_no_script_entry_definition);
    return UNITY_END();
}
