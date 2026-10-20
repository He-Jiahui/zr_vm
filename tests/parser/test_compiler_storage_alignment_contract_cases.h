#ifndef ZR_VM_TEST_COMPILER_STORAGE_ALIGNMENT_CONTRACT_CASES_H
#define ZR_VM_TEST_COMPILER_STORAGE_ALIGNMENT_CONTRACT_CASES_H

#include <stddef.h>

typedef struct SZrTestStorageAlignmentPair {
    TZrInt64 left;
    TZrInt64 right;
} SZrTestStorageAlignmentPair;

typedef struct SZrTestStorageAlignmentNaturalPad {
    TZrInt8 tag;
    TZrInt64 value;
} SZrTestStorageAlignmentNaturalPad;

typedef struct SZrTestStorageAlignmentSnapshot {
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 fieldOffsets[2];
    TZrUInt32 fieldSizes[2];
} SZrTestStorageAlignmentSnapshot;

static TZrBool compiler_storage_alignment_capture(
        SZrState *state,
        const SZrFunction *function,
        const TZrChar *prototypeName,
        const TZrChar *const fieldNames[2],
        const TZrChar *const fieldTypeNames[2],
        SZrTestStorageAlignmentSnapshot *snapshot) {
    const SZrCompiledPrototypeInfoView *prototype =
            find_compiled_prototype_by_name(state, function, prototypeName);
    const SZrCompiledMemberInfoView *previousField = ZR_NULL;

    if (prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
        (prototype->modifierFlags & ZR_TYPE_MODIFIER_FLAG_OPEN_GENERIC) != 0u) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < 2u; index++) {
        const SZrCompiledMemberInfoView *field =
                find_compiled_member_by_name(state, function, prototype, fieldNames[index]);
        SZrString *typeName;

        if (field == ZR_NULL || field == previousField ||
            field->memberType != ZR_AST_STRUCT_FIELD || field->isStatic) {
            return ZR_FALSE;
        }
        typeName = get_string_constant_at(state, function, field->fieldTypeNameStringIndex);
        if (typeName == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(typeName), fieldTypeNames[index]) != 0) {
            return ZR_FALSE;
        }
        snapshot->fieldOffsets[index] = field->fieldOffset;
        snapshot->fieldSizes[index] = field->fieldSize;
        previousField = field;
    }
    snapshot->byteSize = prototype->layoutByteSize;
    snapshot->byteAlign = prototype->layoutByteAlign;
    return ZR_TRUE;
}

static void assert_compiler_storage_alignment_contracts(void) {
    static const TZrChar *source =
            "struct AlignmentPair<T> {\n"
            "  pub var left: T;\n"
            "  pub var right: T;\n"
            "  pub @constructor(left: T, right: T) {\n"
            "    this.left = left;\n"
            "    this.right = right;\n"
            "  }\n"
            "}\n"
            "var pair: AlignmentPair<int> = init AlignmentPair<int>(1, 2);\n"
            "native extern(\"fixture\") {\n"
            "  struct NaturalPad { var tag: i8; var value: i64; }\n"
            "}\n"
            "fn consume(value: NaturalPad): int { return 0; }\n"
            "return pair.left + pair.right;\n";
    static const TZrChar *const pairFields[2] = {"left", "right"};
    static const TZrChar *const pairTypes[2] = {"int", "int"};
    static const TZrChar *const naturalFields[2] = {"tag", "value"};
    static const TZrChar *const naturalTypes[2] = {"i8", "i64"};
    SZrTestStorageAlignmentSnapshot pair = {0};
    SZrTestStorageAlignmentSnapshot natural = {0};
    SZrState *state = create_test_state();
    SZrFunction *function = ZR_NULL;
    SZrString *sourceName;
    const TZrChar *failure = ZR_NULL;

    if (state == ZR_NULL) {
        failure = "Storage alignment fixture must create its independent VM";
        goto cleanup;
    }
    sourceName = ZrCore_String_CreateFromNative(state, "compiler_storage_alignment_contracts.zr");
    if (sourceName == ZR_NULL) {
        failure = "Storage alignment fixture must create its source name";
        goto cleanup;
    }
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    if (function == ZR_NULL || state->hasCurrentException ||
        state->threadStatus != ZR_THREAD_STATUS_FINE) {
        failure = "Storage alignment fixture must compile through the normal source path";
        goto cleanup;
    }
    if (!compiler_storage_alignment_capture(
                state, function, "AlignmentPair<int>", pairFields, pairTypes, &pair)) {
        failure = "Exact closed AlignmentPair<int> must serialize its two int instance fields";
        goto cleanup;
    }
    if (!compiler_storage_alignment_capture(
                state, function, "NaturalPad", naturalFields, naturalTypes, &natural)) {
        failure = "Extern NaturalPad must serialize its i8 and i64 instance fields";
        goto cleanup;
    }

cleanup:
    if (function != ZR_NULL) {
        ZrCore_Function_Free(state, function);
    }
    destroy_test_state(state);

    TEST_ASSERT_NULL_MESSAGE(failure, failure);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)sizeof(SZrTestStorageAlignmentPair), pair.byteSize,
            "Closed generic prototype size must match its C field storage");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)_Alignof(SZrTestStorageAlignmentPair), pair.byteAlign,
            "Closed generic prototype alignment must match its C field storage");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)offsetof(SZrTestStorageAlignmentPair, left), pair.fieldOffsets[0],
            "Closed generic left field offset must match its C field storage");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)sizeof(((SZrTestStorageAlignmentPair *)0)->left), pair.fieldSizes[0],
            "Closed generic left field size must match its C field storage");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)offsetof(SZrTestStorageAlignmentPair, right), pair.fieldOffsets[1],
            "Closed generic right field offset must match its C field storage");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)sizeof(((SZrTestStorageAlignmentPair *)0)->right), pair.fieldSizes[1],
            "Closed generic right field size must match its C field storage");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)sizeof(SZrTestStorageAlignmentNaturalPad), natural.byteSize,
            "Implicit extern prototype size must match its natural C layout");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)_Alignof(SZrTestStorageAlignmentNaturalPad), natural.byteAlign,
            "Implicit extern prototype alignment must match its natural C layout");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)offsetof(SZrTestStorageAlignmentNaturalPad, tag), natural.fieldOffsets[0],
            "Implicit extern tag field offset must match its natural C layout");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)sizeof(((SZrTestStorageAlignmentNaturalPad *)0)->tag), natural.fieldSizes[0],
            "Implicit extern tag field size must match its natural C layout");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)offsetof(SZrTestStorageAlignmentNaturalPad, value), natural.fieldOffsets[1],
            "Implicit extern value field offset must match its natural C layout");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)sizeof(((SZrTestStorageAlignmentNaturalPad *)0)->value), natural.fieldSizes[1],
            "Implicit extern value field size must match its natural C layout");
}

#endif
