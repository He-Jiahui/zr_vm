#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == NULL) {
        return NULL;
    }

    marker = strstr(sourceFile, "tests/parser/test_aot_c_type_layout_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_type_layout_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    for (size_t index = 0u; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void test_aot_c_type_layouts_emit_generated_struct_static_asserts(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_type_layout_declarations(",
            "backend_aot_c_type_layout_resolve_from_table(",
            "backend_aot_c_type_layout_gc_descriptor_index_space(",
            "backend_aot_write_c_type_layout_gc_descriptor_table(",
            "backend_aot_write_c_type_layout_token_table(",
            "SZrState *state",
            "const SZrAotFunctionTable *table",
    };
    static const char *const sourceNeedles[] = {
            "#include \"backend_aot_c_type_layouts.h\"",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout(",
            "ZrCore_Function_VisitPrototypeFrameFieldLayouts(",
            "#define ZR_AOT_C_LAYOUT_STRUCT(name, bytes)",
            "typedef ZR_AOT_C_LAYOUT_STRUCT(ZrLayout_%u, %u)",
            "TZrByte zr_pad_",
            "zr_field_",
            "_Static_assert(sizeof(ZrLayout_%u) == %u",
            "_Static_assert(_Alignof(ZrLayout_%u) == %u",
            "_Static_assert(offsetof(ZrLayout_%u, zr_field_%u) == %u",
            "backend_aot_c_type_layout_write_gc_descriptor(",
            "backend_aot_c_type_layout_has_gc_descriptor(",
            "backend_aot_c_type_layout_gc_descriptor_index_space(",
            "/* zr_aot_gc_descriptor_offsets layout=%u count=%u */",
            "static const TZrUInt32 ZrGcOffsets_%u[]",
            "static const SZrAotGcDescriptor ZrGcDescriptor_%u",
            "ZrGcOffsets_%u,",
            "backend_aot_c_type_layout_can_emit_ownership_offsets(",
            "static const TZrUInt32 ZrOwnershipOffsets_%u[]",
            ".ownershipFieldOffsets = ZrOwnershipOffsets_%u,",
            "static const SZrAotGcDescriptor *const zr_aot_gc_descriptors[]",
            "&ZrGcDescriptor_%u,",
            "zr_aot_gc_descriptor_offsets_failed",
            "typeLayout->gcFieldCount == 0u",
    };
    static const char *const tokenNeedles[] = {
            "#include \"backend_aot_c_type_layouts.h\"",
            "#include \"zr_vm_core/metadata_token.h\"",
            "backend_aot_c_type_layout_token_from_table(",
            "backend_aot_c_type_layout_type_name_from_table(",
            "backend_aot_c_type_layout_type_def_token_from_table(",
            "ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_TYPE_DEF",
            "ZR_METADATA_SIGNATURE_NODE_TYPE_DEF",
            "backend_aot_c_type_layout_resolve_from_table(state, table, typeLayoutId)",
            "static const TZrUInt32 zr_aot_type_layout_tokens[]",
            "0x%08xu",
    };
    static const char *const emitterNeedles[] = {
            "#include \"backend_aot_c_type_layouts.h\"",
            "#include <stddef.h>",
            "TZrUInt32 gcDescriptorIndexSpace;",
            "gcDescriptorIndexSpace = backend_aot_c_type_layout_gc_descriptor_index_space(state, &functionTable);",
            "backend_aot_write_c_type_layout_declarations(file, state, &functionTable);",
            "backend_aot_write_c_type_layout_gc_descriptor_table(file, state, &functionTable, gcDescriptorIndexSpace);",
            "gcDescriptorIndexSpace > 0u ? \"zr_aot_gc_descriptors\" : \"ZR_NULL\"",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h");
    char *sourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c");
    char *tokenText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(sourceText);
    TEST_ASSERT_NOT_NULL(tokenText);
    TEST_ASSERT_NOT_NULL(emitterText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(sourceText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(tokenText, tokenNeedles, ARRAY_COUNT(tokenNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));

    free(headerText);
    free(sourceText);
    free(tokenText);
    free(emitterText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_type_layouts_emit_generated_struct_static_asserts);
    return UNITY_END();
}
