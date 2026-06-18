#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/type_layout.h"

void setUp(void) {}

void tearDown(void) {}

static TZrUInt32 test_align_up(TZrUInt32 offset, TZrUInt32 align) {
    TZrUInt32 remainder = align > 0u ? offset % align : 0u;
    return remainder == 0u ? offset : offset + (align - remainder);
}

static void test_pod_struct_inline_copy_uses_byte_span_and_handles_overlap(void) {
    SZrTypeLayout layout;
    TZrByte buffer[32];
    TZrByte expected[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, expected, sizeof(expected));

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(expected),
            4u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CanRawCopy(&layout));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CopyInline(ZR_NULL, &layout, buffer + 4u, buffer));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer + 4u, sizeof(expected));
}

static void test_managed_struct_inline_copy_copies_and_drops_value_fields_by_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *text;
    SZrTypeLayoutField fields[1];
    SZrTypeLayout layout;
    union {
        TZrByte bytes[sizeof(SZrTypeValue)];
        SZrTypeValue value;
    } source;
    union {
        TZrByte bytes[sizeof(SZrTypeValue)];
        SZrTypeValue value;
    } destination;
    SZrTypeValue *sourceValue = &source.value;
    SZrTypeValue *destinationValue = &destination.value;

    TEST_ASSERT_NOT_NULL(state);

    text = ZrCore_String_CreateFromNative(state, "inline-managed-field");
    TEST_ASSERT_NOT_NULL(text);

    ZrCore_Value_InitAsRawObject(state, sourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    sourceValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(destinationValue);

    fields[0].byteOffset = 0u;
    fields[0].byteSize = sizeof(SZrTypeValue);
    fields[0].typeLayoutIndex = 0u;
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(SZrTypeValue),
            ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
            fields,
            1u);

    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanRawCopy(&layout));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CopyInline(state, &layout, destination.bytes, source.bytes));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, destinationValue->type);
    TEST_ASSERT_EQUAL_PTR(sourceValue->value.object, destinationValue->value.object);

    ZrCore_TypeLayout_DropInline(state, &layout, destination.bytes);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, destinationValue->type);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_gc_only_value_field_drop_keeps_non_owned_reference_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *text;
    SZrTypeLayoutField fields[1];
    SZrTypeLayout layout;
    union {
        TZrByte bytes[sizeof(SZrTypeValue)];
        SZrTypeValue value;
    } storage;
    SZrTypeValue *value = &storage.value;

    TEST_ASSERT_NOT_NULL(state);

    text = ZrCore_String_CreateFromNative(state, "inline-gc-only-field");
    TEST_ASSERT_NOT_NULL(text);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    value->type = ZR_VALUE_TYPE_STRING;

    fields[0].byteOffset = 0u;
    fields[0].byteSize = sizeof(SZrTypeValue);
    fields[0].typeLayoutIndex = 0u;
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(SZrTypeValue),
            ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
            fields,
            1u);

    TEST_ASSERT_EQUAL_UINT32(1u, layout.gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout.ownershipFieldCount);

    ZrCore_TypeLayout_DropInline(state, &layout, storage.bytes);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, value->type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(text), value->value.object);

    ZrCore_Value_ResetAsNull(value);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_layout_inline_copy_uses_value_semantics_for_struct_objects(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    SZrObject *copiedObject;
    SZrTypeLayout valueLayout;
    SZrTypeValue source;
    SZrTypeValue destination;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(state, "InlineValueStruct");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);

    sourceObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(sourceObject);
    sourceObject->prototype = &prototype->super;
    ZrCore_Object_Init(state, sourceObject);

    ZrCore_Value_InitAsRawObject(state, &source, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    source.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&destination);
    ZrCore_TypeLayout_InitValue(&valueLayout);

    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CopyInline(state, &valueLayout, &destination, &source));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, destination.type);
    TEST_ASSERT_NOT_EQUAL(source.value.object, destination.value.object);

    copiedObject = ZR_CAST_OBJECT(state, destination.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(sourceObject->prototype, copiedObject->prototype);

    ZrCore_TypeLayout_DropInline(state, &valueLayout, &destination);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, destination.type);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_frame_layout_assigns_aligned_byte_offsets_for_inline_structs(void) {
    SZrTypeLayout valueLayout;
    SZrTypeLayout structLayout;
    SZrStackFrameLayoutSlot slots[3];
    SZrStackFrameLayout frameLayout;
    TZrUInt32 expectedSlot1;
    TZrUInt32 expectedSlot2;
    TZrUInt32 expectedFrameSize;
    TZrUInt32 expectedMaxAlign;

    ZrCore_TypeLayout_InitValue(&valueLayout);
    ZrCore_TypeLayout_InitStruct(
            &structLayout,
            12u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    slots[0].typeLayout = &valueLayout;
    slots[1].typeLayout = &structLayout;
    slots[2].typeLayout = &valueLayout;

    TEST_ASSERT_TRUE(ZrCore_StackFrameLayout_BuildSequential(&frameLayout, slots, 3u));

    expectedMaxAlign = valueLayout.byteAlign > structLayout.byteAlign ? valueLayout.byteAlign : structLayout.byteAlign;
    expectedSlot1 = test_align_up(valueLayout.byteSize, structLayout.byteAlign);
    expectedSlot2 = test_align_up(expectedSlot1 + structLayout.byteSize, valueLayout.byteAlign);
    expectedFrameSize = test_align_up(expectedSlot2 + valueLayout.byteSize, expectedMaxAlign);

    TEST_ASSERT_EQUAL_UINT32(0u, slots[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedSlot1, slots[1].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedSlot2, slots[2].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedFrameSize, frameLayout.byteSize);
    TEST_ASSERT_EQUAL_UINT32(expectedMaxAlign, frameLayout.maxAlign);
}

static void test_stack_inline_copy_uses_byte_offsets_from_stack_base(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayout layout;
    TZrByte pattern[8] = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u};
    TZrByte *stackBytes;
    TZrMemoryOffset sourceOffset = 3u;
    TZrMemoryOffset destinationOffset = 19u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    stackBytes = (TZrByte *)state->stackBase.valuePointer;
    memcpy(stackBytes + sourceOffset, pattern, sizeof(pattern));

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(pattern),
            1u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    TEST_ASSERT_EQUAL_PTR(stackBytes + sourceOffset, ZrCore_Stack_LoadByteOffsetToAddress(state, sourceOffset));
    TEST_ASSERT_EQUAL_UINT32(sourceOffset, ZrCore_Stack_SaveByteAddressAsOffset(state, stackBytes + sourceOffset));
    TEST_ASSERT_TRUE(ZrCore_Stack_CopyInline(state, &layout, destinationOffset, sourceOffset));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pattern, stackBytes + destinationOffset, sizeof(pattern));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_stack_frame_place_resolves_byte_offset_from_frame_base(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer frameBase;
    TZrByte *expectedAddress;
    SZrStackFramePlace place;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    frameBase = state->stackBase.valuePointer + 2;
    expectedAddress = (TZrByte *)frameBase + 12u;

    TEST_ASSERT_TRUE(ZrCore_Stack_MakeFramePlace(state, frameBase, 12u, 8u, 4u, &place));
    TEST_ASSERT_EQUAL_PTR(expectedAddress, place.address);
    TEST_ASSERT_EQUAL_UINT32(8u, place.byteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, place.byteAlign);
    TEST_ASSERT_EQUAL_INT(ZrCore_Stack_SaveByteAddressAsOffset(state, expectedAddress), place.byteOffset);

    TEST_ASSERT_FALSE(ZrCore_Stack_MakeFramePlace(state, frameBase, 13u, 8u, 4u, &place));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_stack_inline_place_copy_uses_resolved_frame_places(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayout layout;
    TZrStackValuePointer frameBase;
    SZrStackFramePlace source;
    SZrStackFramePlace destination;
    TZrByte pattern[8] = {0x90u, 0x81u, 0x72u, 0x63u, 0x54u, 0x45u, 0x36u, 0x27u};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    frameBase = state->stackBase.valuePointer + 1;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(pattern),
            1u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    TEST_ASSERT_TRUE(ZrCore_Stack_MakeFramePlace(state, frameBase, 3u, sizeof(pattern), 1u, &source));
    TEST_ASSERT_TRUE(ZrCore_Stack_MakeFramePlace(state, frameBase, 19u, sizeof(pattern), 1u, &destination));

    memcpy(source.address, pattern, sizeof(pattern));

    TEST_ASSERT_TRUE(ZrCore_Stack_CopyInlinePlace(state, &layout, &destination, &source));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pattern, destination.address, sizeof(pattern));

    destination.byteSize = 4u;
    TEST_ASSERT_FALSE(ZrCore_Stack_CopyInlinePlace(state, &layout, &destination, &source));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_frame_slot_place_uses_function_layout_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layouts[2];
    TZrStackValuePointer frameBase;
    TZrByte *expectedAddress;
    SZrStackFramePlace place;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    layouts[0].stackSlot = 0u;
    layouts[0].byteOffset = 0u;
    layouts[0].byteSize = sizeof(SZrTypeValue);
    layouts[0].byteAlign = ZR_ALIGN_SIZE;
    layouts[0].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layouts[0].isParameter = ZR_TRUE;
    layouts[0].reserved0 = 0u;

    layouts[1].stackSlot = 2u;
    layouts[1].byteOffset = 32u;
    layouts[1].byteSize = 16u;
    layouts[1].byteAlign = 8u;
    layouts[1].typeLayoutId = 7u;
    layouts[1].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[1].isParameter = ZR_TRUE;
    layouts[1].reserved0 = 0u;

    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = ZR_ARRAY_COUNT(layouts);
    function.frameByteSize = 48u;
    function.frameByteAlign = 8u;

    frameBase = state->stackBase.valuePointer + 2;
    expectedAddress = (TZrByte *)frameBase + layouts[1].byteOffset;

    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &function, frameBase, 2u, &place));
    TEST_ASSERT_EQUAL_PTR(expectedAddress, place.address);
    TEST_ASSERT_EQUAL_UINT32(layouts[1].byteSize, place.byteSize);
    TEST_ASSERT_EQUAL_UINT32(layouts[1].byteAlign, place.byteAlign);
    TEST_ASSERT_EQUAL_INT(ZrCore_Stack_SaveByteAddressAsOffset(state, expectedAddress), place.byteOffset);

    TEST_ASSERT_FALSE(ZrCore_Function_MakeFrameSlotPlace(state, &function, frameBase, 1u, &place));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_frame_slot_inline_copy_uses_source_and_destination_layouts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction sourceFunction = {0};
    SZrFunction destinationFunction = {0};
    SZrFunctionFrameSlotLayout sourceLayouts[1];
    SZrFunctionFrameSlotLayout destinationLayouts[1];
    SZrTypeLayout layout;
    SZrStackFramePlace sourcePlace;
    SZrStackFramePlace destinationPlace;
    TZrStackValuePointer sourceFrameBase;
    TZrStackValuePointer destinationFrameBase;
    TZrByte pattern[12] = {
            0x01u, 0x13u, 0x25u, 0x37u, 0x49u, 0x5bu,
            0x6du, 0x7fu, 0x91u, 0xa3u, 0xb5u, 0xc7u};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(pattern),
            4u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    sourceLayouts[0].stackSlot = 3u;
    sourceLayouts[0].byteOffset = 16u;
    sourceLayouts[0].byteSize = sizeof(pattern);
    sourceLayouts[0].byteAlign = 4u;
    sourceLayouts[0].typeLayoutId = 3u;
    sourceLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    sourceLayouts[0].isParameter = ZR_FALSE;
    sourceLayouts[0].reserved0 = 0u;

    destinationLayouts[0].stackSlot = 0u;
    destinationLayouts[0].byteOffset = 64u;
    destinationLayouts[0].byteSize = sizeof(pattern);
    destinationLayouts[0].byteAlign = 4u;
    destinationLayouts[0].typeLayoutId = 9u;
    destinationLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    destinationLayouts[0].isParameter = ZR_TRUE;
    destinationLayouts[0].reserved0 = 0u;

    sourceFunction.frameSlotLayouts = sourceLayouts;
    sourceFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(sourceLayouts);
    sourceFunction.frameByteSize = 32u;
    sourceFunction.frameByteAlign = 4u;
    destinationFunction.frameSlotLayouts = destinationLayouts;
    destinationFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(destinationLayouts);
    destinationFunction.frameByteSize = 80u;
    destinationFunction.frameByteAlign = 4u;

    sourceFrameBase = state->stackBase.valuePointer + 1;
    destinationFrameBase = state->stackBase.valuePointer + 6;

    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &sourceFunction, sourceFrameBase, 3u, &sourcePlace));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            &destinationFunction,
            destinationFrameBase,
            0u,
            &destinationPlace));
    memcpy(sourcePlace.address, pattern, sizeof(pattern));

    TEST_ASSERT_TRUE(ZrCore_Function_CopyFrameSlotInline(
            state,
            &layout,
            &destinationFunction,
            destinationFrameBase,
            0u,
            &sourceFunction,
            sourceFrameBase,
            3u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pattern, destinationPlace.address, sizeof(pattern));

    destinationLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Function_CopyFrameSlotInline(
            state,
            &layout,
            &destinationFunction,
            destinationFrameBase,
            0u,
            &sourceFunction,
            sourceFrameBase,
            3u));

    destinationLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    destinationLayouts[0].byteSize = 4u;
    TEST_ASSERT_FALSE(ZrCore_Function_CopyFrameSlotInline(
            state,
            &layout,
            &destinationFunction,
            destinationFrameBase,
            0u,
            &sourceFunction,
            sourceFrameBase,
            3u));

    ZrTests_Runtime_State_Destroy(state);
}

typedef struct TestFrameLayoutResolver {
    TZrUInt32 typeLayoutId;
    const SZrTypeLayout *layout;
} TestFrameLayoutResolver;

typedef struct TestGcValueVisit {
    TZrUInt32 count;
    SZrTypeValue *value;
} TestGcValueVisit;

static TZrUInt32 test_write_compiled_prototype_data(TZrByte *buffer,
                                                    TZrUInt32 bufferSize,
                                                    const SZrCompiledPrototypeInfo *prototype,
                                                    const SZrCompiledMemberInfo *members,
                                                    TZrUInt32 memberCount) {
    TZrUInt32 prototypeCount = 1u;
    TZrUInt32 cursor = 0u;
    TZrUInt32 memberBytes = sizeof(SZrCompiledMemberInfo) * memberCount;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_TRUE(bufferSize >= sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + memberBytes);

    memcpy(buffer + cursor, &prototypeCount, sizeof(prototypeCount));
    cursor += (TZrUInt32)sizeof(prototypeCount);
    memcpy(buffer + cursor, prototype, sizeof(*prototype));
    cursor += (TZrUInt32)sizeof(*prototype);
    if (memberBytes > 0u) {
        TEST_ASSERT_NOT_NULL(members);
        memcpy(buffer + cursor, members, memberBytes);
        cursor += memberBytes;
    }
    return cursor;
}

static TZrUInt32 test_write_two_compiled_prototype_data(TZrByte *buffer,
                                                        TZrUInt32 bufferSize,
                                                        const SZrCompiledPrototypeInfo *firstPrototype,
                                                        const SZrCompiledMemberInfo *firstMembers,
                                                        TZrUInt32 firstMemberCount,
                                                        const SZrCompiledPrototypeInfo *secondPrototype,
                                                        const SZrCompiledMemberInfo *secondMembers,
                                                        TZrUInt32 secondMemberCount) {
    TZrUInt32 prototypeCount = 2u;
    TZrUInt32 cursor = 0u;
    TZrUInt32 firstMemberBytes = sizeof(SZrCompiledMemberInfo) * firstMemberCount;
    TZrUInt32 secondMemberBytes = sizeof(SZrCompiledMemberInfo) * secondMemberCount;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(firstPrototype);
    TEST_ASSERT_NOT_NULL(secondPrototype);
    TEST_ASSERT_TRUE(bufferSize >= sizeof(TZrUInt32) +
                                         sizeof(SZrCompiledPrototypeInfo) * 2u +
                                         firstMemberBytes +
                                         secondMemberBytes);

    memcpy(buffer + cursor, &prototypeCount, sizeof(prototypeCount));
    cursor += (TZrUInt32)sizeof(prototypeCount);
    memcpy(buffer + cursor, firstPrototype, sizeof(*firstPrototype));
    cursor += (TZrUInt32)sizeof(*firstPrototype);
    if (firstMemberBytes > 0u) {
        TEST_ASSERT_NOT_NULL(firstMembers);
        memcpy(buffer + cursor, firstMembers, firstMemberBytes);
        cursor += firstMemberBytes;
    }
    memcpy(buffer + cursor, secondPrototype, sizeof(*secondPrototype));
    cursor += (TZrUInt32)sizeof(*secondPrototype);
    if (secondMemberBytes > 0u) {
        TEST_ASSERT_NOT_NULL(secondMembers);
        memcpy(buffer + cursor, secondMembers, secondMemberBytes);
        cursor += secondMemberBytes;
    }
    return cursor;
}

static void test_init_compiled_struct_prototype(SZrCompiledPrototypeInfo *prototype,
                                                TZrUInt32 byteSize,
                                                TZrUInt32 byteAlign,
                                                TZrUInt32 memberCount) {
    memset(prototype, 0, sizeof(*prototype));
    prototype->type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype->membersCount = memberCount;
    prototype->layoutByteSize = byteSize;
    prototype->layoutByteAlign = byteAlign;
}

static void test_init_string_constant(SZrState *state, SZrTypeValue *value, const char *text) {
    SZrString *string;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(text);

    string = ZrCore_String_CreateFromNative(state, (TZrNativeString)text);
    TEST_ASSERT_NOT_NULL(string);
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value->type = ZR_VALUE_TYPE_STRING;
}

static void test_install_managed_value_inline_frame_metadata(SZrState *state,
                                                             SZrFunction *function,
                                                             TZrUInt32 stackSlot,
                                                             TZrUInt32 byteOffset) {
    SZrFunctionFrameSlotLayout *layouts;
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    TZrUInt32 prototypeDataLength =
            (TZrUInt32)(sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo));
    TZrByte *prototypeData;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);

    layouts[0].stackSlot = stackSlot;
    layouts[0].byteOffset = byteOffset;
    layouts[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layouts[0].byteAlign = ZR_ALIGN_SIZE;
    layouts[0].typeLayoutId = 0u;
    layouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[0].isParameter = ZR_FALSE;
    layouts[0].reserved0 = 0u;

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(SZrTypeValue), ZR_ALIGN_SIZE, 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);
    member.isUsingManaged = 1u;
    member.ownershipQualifier = 1u;

    prototypeData = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            prototypeDataLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(prototypeData);

    function->frameSlotLayouts = layouts;
    function->frameSlotLayoutLength = 1u;
    function->frameByteSize = byteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function->frameByteAlign = ZR_ALIGN_SIZE;
    function->stackSize = stackSlot + 1u;
    function->prototypeData = prototypeData;
    function->prototypeDataLength = test_write_compiled_prototype_data(
            prototypeData,
            prototypeDataLength,
            &prototype,
            &member,
            1u);
    function->prototypeCount = 1u;
}

static const SZrTypeLayout *test_resolve_function_frame_layout(const SZrFunction *function,
                                                               TZrUInt32 typeLayoutId,
                                                               TZrPtr userData) {
    const TestFrameLayoutResolver *resolver = (const TestFrameLayoutResolver *)userData;
    (void)function;

    if (resolver == ZR_NULL || resolver->typeLayoutId != typeLayoutId) {
        return ZR_NULL;
    }

    return resolver->layout;
}

static void test_visit_gc_value(struct SZrState *state, SZrTypeValue *value, TZrPtr userData) {
    TestGcValueVisit *visit = (TestGcValueVisit *)userData;
    (void)state;

    if (visit == ZR_NULL) {
        return;
    }

    visit->count++;
    visit->value = value;
}

static void test_function_prototype_type_layout_resolver_accepts_pod_struct_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo)];
    } data;
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(state);

    test_init_compiled_struct_prototype(&prototype, 24u, 8u, 0u);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            ZR_NULL,
            0u);
    function.prototypeCount = 1u;

    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_KIND_STRUCT, layout->kind);
    TEST_ASSERT_EQUAL_UINT32(24u, layout->byteSize);
    TEST_ASSERT_EQUAL_UINT32(8u, layout->byteAlign);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->fieldCount);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CanRawCopy(layout));
    TEST_ASSERT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 1u, state));

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_accepts_primitive_scalar_field_as_pod(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    SZrTypeValue constants[3];
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&constants[0]);
    test_init_string_constant(state, &constants[1], "i32");

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(TZrInt32), (TZrUInt32)sizeof(TZrInt32), 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldTypeNameStringIndex = 1u;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(TZrInt32);

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->fieldCount);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CanRawCopy(layout));

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_builds_managed_value_field_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(state);

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(SZrTypeValue), ZR_ALIGN_SIZE, 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);
    member.isUsingManaged = 1u;
    member.ownershipQualifier = 1u;
    member.callsClose = 1u;
    member.callsDestructor = 1u;

    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_KIND_STRUCT, layout->kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY, layout->copyKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP, layout->dropKind);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->ownershipFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->fields[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(sizeof(SZrTypeValue), layout->fields[0].byteSize);
    TEST_ASSERT_TRUE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u);
    TEST_ASSERT_TRUE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) != 0u);
    TEST_ASSERT_TRUE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u);

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_builds_gc_value_field_for_value_sized_reference_type(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    SZrTypeValue constants[3];
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&constants[0]);
    test_init_string_constant(state, &constants[1], "string");

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(SZrTypeValue), ZR_ALIGN_SIZE, 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldTypeNameStringIndex = 1u;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY, layout->copyKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP, layout->dropKind);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->ownershipFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->fields[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(sizeof(SZrTypeValue), layout->fields[0].byteSize);
    TEST_ASSERT_TRUE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u);
    TEST_ASSERT_TRUE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) != 0u);
    TEST_ASSERT_FALSE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u);

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_flattens_nested_managed_struct_fields(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo parentPrototype;
    SZrCompiledPrototypeInfo childPrototype;
    SZrCompiledMemberInfo parentMember;
    SZrCompiledMemberInfo childMember;
    SZrTypeValue constants[3];
    SZrString *parentName;
    SZrString *childName;
    enum {
        parentFieldOffset = 8u
    };
    TZrUInt32 parentLayoutByteSize = parentFieldOffset + (TZrUInt32)sizeof(SZrTypeValue);
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) +
                      sizeof(SZrCompiledPrototypeInfo) * 2u +
                      sizeof(SZrCompiledMemberInfo) * 2u];
    } data;
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(state);

    parentName = ZrCore_String_CreateFromNative(state, "NestedInlineParent");
    childName = ZrCore_String_CreateFromNative(state, "NestedInlineChild");
    TEST_ASSERT_NOT_NULL(parentName);
    TEST_ASSERT_NOT_NULL(childName);
    ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(parentName));
    constants[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &constants[1], ZR_CAST_RAW_OBJECT_AS_SUPER(childName));
    constants[1].type = ZR_VALUE_TYPE_STRING;
    test_init_string_constant(state, &constants[2], "int");

    test_init_compiled_struct_prototype(&parentPrototype, parentLayoutByteSize, ZR_ALIGN_SIZE, 1u);
    parentPrototype.nameStringIndex = 0u;
    test_init_compiled_struct_prototype(&childPrototype, (TZrUInt32)sizeof(SZrTypeValue), ZR_ALIGN_SIZE, 1u);
    childPrototype.nameStringIndex = 1u;

    memset(&parentMember, 0, sizeof(parentMember));
    parentMember.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    parentMember.fieldTypeNameStringIndex = 1u;
    parentMember.fieldOffset = parentFieldOffset;
    parentMember.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);

    memset(&childMember, 0, sizeof(childMember));
    childMember.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    childMember.fieldOffset = 0u;
    childMember.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);
    childMember.isUsingManaged = 1u;
    childMember.ownershipQualifier = 1u;

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_two_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &parentPrototype,
            &parentMember,
            1u,
            &childPrototype,
            &childMember,
            1u);
    function.prototypeCount = 2u;

    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY, layout->copyKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP, layout->dropKind);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->ownershipFieldCount);
    TEST_ASSERT_EQUAL_UINT32(parentFieldOffset, layout->fields[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(sizeof(SZrTypeValue), layout->fields[0].byteSize);
    TEST_ASSERT_TRUE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u);

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_marks_nested_inline_struct_field(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo parentPrototype;
    SZrCompiledPrototypeInfo childPrototype;
    SZrCompiledMemberInfo parentMember;
    SZrCompiledMemberInfo childMember;
    SZrTypeValue constants[3];
    SZrString *parentName;
    SZrString *childName;
    enum {
        childLayoutIndex = 1u,
        pointByteSize = sizeof(TZrInt64) * 2u
    };
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) +
                      sizeof(SZrCompiledPrototypeInfo) * 2u +
                      sizeof(SZrCompiledMemberInfo) * 2u];
    } data;
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(state);

    parentName = ZrCore_String_CreateFromNative(state, "InlineBox");
    childName = ZrCore_String_CreateFromNative(state, "InlinePoint");
    TEST_ASSERT_NOT_NULL(parentName);
    TEST_ASSERT_NOT_NULL(childName);
    ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(parentName));
    constants[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &constants[1], ZR_CAST_RAW_OBJECT_AS_SUPER(childName));
    constants[1].type = ZR_VALUE_TYPE_STRING;
    test_init_string_constant(state, &constants[2], "int");

    test_init_compiled_struct_prototype(&parentPrototype, pointByteSize, ZR_ALIGN_SIZE, 1u);
    parentPrototype.nameStringIndex = 0u;
    test_init_compiled_struct_prototype(&childPrototype, pointByteSize, ZR_ALIGN_SIZE, 1u);
    childPrototype.nameStringIndex = 1u;

    memset(&parentMember, 0, sizeof(parentMember));
    parentMember.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    parentMember.fieldTypeNameStringIndex = 1u;
    parentMember.fieldOffset = 0u;
    parentMember.fieldSize = pointByteSize;

    memset(&childMember, 0, sizeof(childMember));
    childMember.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    childMember.fieldTypeNameStringIndex = 2u;
    childMember.fieldOffset = 0u;
    childMember.fieldSize = (TZrUInt32)sizeof(TZrInt64);

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_two_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &parentPrototype,
            &parentMember,
            1u,
            &childPrototype,
            &childMember,
            1u);
    function.prototypeCount = 2u;

    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY, layout->copyKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP, layout->dropKind);
    TEST_ASSERT_EQUAL_UINT32(1u, layout->fieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->ownershipFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout->fields[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(pointByteSize, layout->fields[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(childLayoutIndex, layout->fields[0].typeLayoutIndex);
    TEST_ASSERT_FALSE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u);
    TEST_ASSERT_FALSE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) != 0u);
    TEST_ASSERT_FALSE((layout->fields[0].flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u);

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_fails_when_managed_field_is_not_value_sized(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;

    TEST_ASSERT_NOT_NULL(state);

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(TZrPtr), ZR_ALIGN_SIZE, 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(TZrPtr);
    member.isUsingManaged = 1u;
    member.ownershipQualifier = 1u;

    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    TEST_ASSERT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state));

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_fails_reference_pointer_field_without_value_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    SZrTypeValue constants[2];
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&constants[0]);
    test_init_string_constant(state, &constants[1], "string");

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(TZrPtr), ZR_ALIGN_SIZE, 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldTypeNameStringIndex = 1u;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(TZrPtr);

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    TEST_ASSERT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state));

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_fails_unknown_nonlocal_field_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    SZrTypeValue constants[2];
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&constants[0]);
    test_init_string_constant(state, &constants[1], "external.UnknownStruct");

    test_init_compiled_struct_prototype(&prototype, 16u, ZR_ALIGN_SIZE, 1u);
    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldTypeNameStringIndex = 1u;
    member.fieldOffset = 0u;
    member.fieldSize = 16u;

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    TEST_ASSERT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state));

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_prototype_type_layout_resolver_fails_recursive_struct_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    SZrTypeValue constants[2];
    SZrString *selfName;
    union {
        TZrUInt64 align;
        TZrByte bytes[sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo)];
    } data;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&constants[0]);
    selfName = ZrCore_String_CreateFromNative(state, "RecursiveInlineNode");
    TEST_ASSERT_NOT_NULL(selfName);
    ZrCore_Value_InitAsRawObject(state, &constants[1], ZR_CAST_RAW_OBJECT_AS_SUPER(selfName));
    constants[1].type = ZR_VALUE_TYPE_STRING;

    test_init_compiled_struct_prototype(&prototype, (TZrUInt32)sizeof(SZrTypeValue), ZR_ALIGN_SIZE, 1u);
    prototype.nameStringIndex = 1u;

    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldTypeNameStringIndex = 1u;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);

    function.constantValueList = constants;
    function.constantValueLength = ZR_ARRAY_COUNT(constants);
    function.prototypeData = data.bytes;
    function.prototypeDataLength = test_write_compiled_prototype_data(
            data.bytes,
            (TZrUInt32)sizeof(data.bytes),
            &prototype,
            &member,
            1u);
    function.prototypeCount = 1u;

    TEST_ASSERT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state));
    TEST_ASSERT_NULL(ZrCore_Function_ResolvePrototypeFrameTypeLayout(&function, 0u, state));

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, &function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_inline_parameters_copy_by_frame_layout_copies_byte_payloads(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction callerFunction = {0};
    SZrFunction calleeFunction = {0};
    SZrFunctionFrameSlotLayout callerLayouts[1];
    SZrFunctionFrameSlotLayout calleeLayouts[2];
    SZrTypeLayout layout;
    TestFrameLayoutResolver resolver;
    SZrStackFramePlace sourcePlace;
    SZrStackFramePlace destinationPlace;
    TZrStackValuePointer callerFrameBase;
    TZrStackValuePointer calleeFrameBase;
    TZrByte payload[12] = {
            0x41u, 0x52u, 0x63u, 0x74u, 0x85u, 0x96u,
            0xa7u, 0xb8u, 0xc9u, 0xdau, 0xebu, 0xfcu};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(payload),
            4u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    resolver.typeLayoutId = 23u;
    resolver.layout = &layout;

    callerLayouts[0].stackSlot = 4u;
    callerLayouts[0].byteOffset = 40u;
    callerLayouts[0].byteSize = sizeof(payload);
    callerLayouts[0].byteAlign = 4u;
    callerLayouts[0].typeLayoutId = resolver.typeLayoutId;
    callerLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerLayouts[0].isParameter = ZR_FALSE;
    callerLayouts[0].reserved0 = 0u;

    calleeLayouts[0].stackSlot = 0u;
    calleeLayouts[0].byteOffset = 16u;
    calleeLayouts[0].byteSize = sizeof(payload);
    calleeLayouts[0].byteAlign = 4u;
    calleeLayouts[0].typeLayoutId = resolver.typeLayoutId;
    calleeLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    calleeLayouts[0].isParameter = ZR_TRUE;
    calleeLayouts[0].reserved0 = 0u;

    calleeLayouts[1].stackSlot = 1u;
    calleeLayouts[1].byteOffset = 32u;
    calleeLayouts[1].byteSize = sizeof(SZrTypeValue);
    calleeLayouts[1].byteAlign = ZR_ALIGN_SIZE;
    calleeLayouts[1].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    calleeLayouts[1].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    calleeLayouts[1].isParameter = ZR_TRUE;
    calleeLayouts[1].reserved0 = 0u;

    callerFunction.frameSlotLayouts = callerLayouts;
    callerFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(callerLayouts);
    callerFunction.frameByteSize = 64u;
    callerFunction.frameByteAlign = 4u;
    calleeFunction.frameSlotLayouts = calleeLayouts;
    calleeFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(calleeLayouts);
    calleeFunction.frameByteSize = 48u;
    calleeFunction.frameByteAlign = ZR_ALIGN_SIZE;

    callerFrameBase = state->stackBase.valuePointer + 1;
    calleeFrameBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &callerFunction, callerFrameBase, 4u, &sourcePlace));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &calleeFunction, calleeFrameBase, 0u, &destinationPlace));

    memcpy(sourcePlace.address, payload, sizeof(payload));
    memset(destinationPlace.address, 0, sizeof(payload));

    TEST_ASSERT_TRUE(ZrCore_Function_CopyInlineFrameParameters(
            state,
            &calleeFunction,
            calleeFrameBase,
            &callerFunction,
            callerFrameBase,
            4u,
            test_resolve_function_frame_layout,
            &resolver));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, destinationPlace.address, sizeof(payload));

    resolver.typeLayoutId = 77u;
    TEST_ASSERT_FALSE(ZrCore_Function_CopyInlineFrameParameters(
            state,
            &calleeFunction,
            calleeFrameBase,
            &callerFunction,
            callerFrameBase,
            4u,
            test_resolve_function_frame_layout,
            &resolver));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_frame_parameter_copy_uses_dense_source_when_frame_value_slot_is_unmaterialized(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction callerFunction = {0};
    SZrFunction calleeFunction = {0};
    SZrFunctionFrameSlotLayout callerLayouts[1];
    SZrFunctionFrameSlotLayout calleeLayouts[2];
    TZrStackValuePointer callerFrameBase;
    TZrStackValuePointer calleeFrameBase;
    SZrStackFramePlace callerBytePlace;
    SZrStackFramePlace calleeValuePlace;
    SZrTypeValue *denseSource;
    SZrTypeValue *denseDestination;
    SZrTypeValue *callerByteSource;
    SZrTypeValue *calleeByteDestination;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    memset(callerLayouts, 0, sizeof(callerLayouts));
    memset(calleeLayouts, 0, sizeof(calleeLayouts));

    callerLayouts[0].stackSlot = 6u;
    callerLayouts[0].byteOffset = 16u * (TZrUInt32)sizeof(SZrTypeValueOnStack) + 32u;
    callerLayouts[0].byteSize = sizeof(SZrTypeValue);
    callerLayouts[0].byteAlign = ZR_ALIGN_SIZE;
    callerLayouts[0].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    callerLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    callerLayouts[0].isParameter = ZR_FALSE;

    calleeLayouts[0].stackSlot = 0u;
    calleeLayouts[0].byteOffset = 32u;
    calleeLayouts[0].byteSize = 16u;
    calleeLayouts[0].byteAlign = 8u;
    calleeLayouts[0].typeLayoutId = 7u;
    calleeLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    calleeLayouts[0].isParameter = ZR_TRUE;

    calleeLayouts[1].stackSlot = 1u;
    calleeLayouts[1].byteOffset = 4u * (TZrUInt32)sizeof(SZrTypeValueOnStack) + 32u;
    calleeLayouts[1].byteSize = sizeof(SZrTypeValue);
    calleeLayouts[1].byteAlign = ZR_ALIGN_SIZE;
    calleeLayouts[1].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    calleeLayouts[1].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    calleeLayouts[1].isParameter = ZR_TRUE;

    callerFunction.stackSize = 16u;
    callerFunction.frameSlotLayouts = callerLayouts;
    callerFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(callerLayouts);
    callerFunction.frameByteSize = callerLayouts[0].byteOffset + callerLayouts[0].byteSize;
    callerFunction.frameByteAlign = ZR_ALIGN_SIZE;

    calleeFunction.stackSize = 4u;
    calleeFunction.parameterCount = 2u;
    calleeFunction.frameSlotLayouts = calleeLayouts;
    calleeFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(calleeLayouts);
    calleeFunction.frameByteSize = calleeLayouts[1].byteOffset + calleeLayouts[1].byteSize;
    calleeFunction.frameByteAlign = ZR_ALIGN_SIZE;

    callerFrameBase = state->stackBase.valuePointer + 1;
    calleeFrameBase = state->stackBase.valuePointer + 24;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            &callerFunction,
            callerFrameBase,
            callerLayouts[0].stackSlot,
            &callerBytePlace));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            &calleeFunction,
            calleeFrameBase,
            calleeLayouts[1].stackSlot,
            &calleeValuePlace));

    denseSource = ZrCore_Stack_GetValue(callerFrameBase + callerLayouts[0].stackSlot);
    denseDestination = ZrCore_Stack_GetValue(calleeFrameBase + calleeLayouts[1].stackSlot);
    callerByteSource = (SZrTypeValue *)callerBytePlace.address;
    calleeByteDestination = (SZrTypeValue *)calleeValuePlace.address;
    ZrCore_Value_InitAsInt(state, denseSource, 42);
    ZrCore_Value_ResetAsNull(callerByteSource);
    ZrCore_Value_ResetAsNull(denseDestination);
    ZrCore_Value_ResetAsNull(calleeByteDestination);

    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParametersFromFrame(
            state,
            &calleeFunction,
            calleeFrameBase,
            &callerFunction,
            callerFrameBase,
            5u,
            2u));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, calleeByteDestination->type);
    TEST_ASSERT_EQUAL_INT64(42, calleeByteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, denseDestination->type);
    TEST_ASSERT_EQUAL_INT64(42, denseDestination->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_frame_parameter_copy_normalizes_reused_destination_slots(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction calleeFunction = {0};
    SZrFunctionFrameSlotLayout calleeLayouts[1];
    TZrStackValuePointer calleeFrameBase;
    TZrStackValuePointer argumentBase;
    SZrStackFramePlace calleeValuePlace;
    SZrTypeValue *argumentValue;
    SZrTypeValue *denseDestination;
    SZrTypeValue *byteDestination;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    memset(calleeLayouts, 0, sizeof(calleeLayouts));

    calleeLayouts[0].stackSlot = 2u;
    calleeLayouts[0].byteOffset = 8u * (TZrUInt32)sizeof(SZrTypeValueOnStack) + 32u;
    calleeLayouts[0].byteSize = sizeof(SZrTypeValue);
    calleeLayouts[0].byteAlign = ZR_ALIGN_SIZE;
    calleeLayouts[0].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    calleeLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    calleeLayouts[0].isParameter = ZR_TRUE;

    calleeFunction.stackSize = 8u;
    calleeFunction.parameterCount = 1u;
    calleeFunction.frameSlotLayouts = calleeLayouts;
    calleeFunction.frameSlotLayoutLength = ZR_ARRAY_COUNT(calleeLayouts);
    calleeFunction.frameByteSize = calleeLayouts[0].byteOffset + calleeLayouts[0].byteSize;
    calleeFunction.frameByteAlign = ZR_ALIGN_SIZE;

    argumentBase = state->stackBase.valuePointer + 1;
    calleeFrameBase = state->stackBase.valuePointer + 16;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            &calleeFunction,
            calleeFrameBase,
            calleeLayouts[0].stackSlot,
            &calleeValuePlace));

    argumentValue = ZrCore_Stack_GetValue(argumentBase);
    denseDestination = ZrCore_Stack_GetValue(calleeFrameBase + calleeLayouts[0].stackSlot);
    byteDestination = (SZrTypeValue *)calleeValuePlace.address;

    ZrCore_Value_InitAsInt(state, argumentValue, 77);
    ZrCore_Value_ResetAsNull(denseDestination);
    ZrCore_Value_ResetAsNull(byteDestination);
    denseDestination->ownershipControl = (struct SZrOwnershipControl *)state;
    denseDestination->ownershipWeakRef = (struct SZrOwnershipWeakRef *)state;
    byteDestination->ownershipControl = (struct SZrOwnershipControl *)state;
    byteDestination->ownershipWeakRef = (struct SZrOwnershipWeakRef *)state;

    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParameters(
            state,
            &calleeFunction,
            calleeFrameBase,
            argumentBase,
            1u));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, byteDestination->type);
    TEST_ASSERT_EQUAL_INT64(77, byteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(byteDestination->ownershipControl);
    TEST_ASSERT_NULL(byteDestination->ownershipWeakRef);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, byteDestination->ownershipKind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, denseDestination->type);
    TEST_ASSERT_EQUAL_INT64(77, denseDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(denseDestination->ownershipControl);
    TEST_ASSERT_NULL(denseDestination->ownershipWeakRef);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, denseDestination->ownershipKind);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_copy_normalizes_reused_no_ownership_destination(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue destination;
    SZrTypeValue source;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_ResetAsNull(&destination);
    ZrCore_Value_InitAsInt(state, &source, 91);
    destination.ownershipControl = (struct SZrOwnershipControl *)state;
    destination.ownershipWeakRef = (struct SZrOwnershipWeakRef *)state;

    ZrCore_Value_CopyNoProfile(state, &destination, &source);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, destination.type);
    TEST_ASSERT_EQUAL_INT64(91, destination.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination.ownershipKind);
    TEST_ASSERT_NULL(destination.ownershipControl);
    TEST_ASSERT_NULL(destination.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_prepared_resolved_vm_precall_copies_inline_parameter_payload_from_caller_frame(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *calleeFunction;
    SZrFunctionFrameSlotLayout *callerLayouts;
    SZrFunctionFrameSlotLayout *calleeLayouts;
    SZrCompiledPrototypeInfo prototype;
    TZrByte *prototypeData;
    TZrUInt32 prototypeDataLength = (TZrUInt32)(sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo));
    TZrStackValuePointer callerFunctionBase;
    TZrStackValuePointer callerFrameBase;
    TZrStackValuePointer callStackPointer;
    SZrStackFramePlace sourcePlace;
    SZrStackFramePlace destinationPlace;
    SZrCallInfo *callerCallInfo;
    SZrCallInfo *calleeCallInfo;
    TZrByte payload[12] = {
            0x11u, 0x23u, 0x35u, 0x47u, 0x59u, 0x6bu,
            0x7du, 0x8fu, 0x91u, 0xa3u, 0xb5u, 0xc7u};

    TEST_ASSERT_NOT_NULL(state);

    callerFunction = ZrCore_Function_New(state);
    calleeFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    callerLayouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    calleeLayouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    prototypeData = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            prototypeDataLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(callerLayouts);
    TEST_ASSERT_NOT_NULL(calleeLayouts);
    TEST_ASSERT_NOT_NULL(prototypeData);
    memset(callerLayouts, 0, sizeof(SZrFunctionFrameSlotLayout));
    memset(calleeLayouts, 0, sizeof(SZrFunctionFrameSlotLayout));

    callerLayouts[0].stackSlot = 3u;
    callerLayouts[0].byteOffset = 512u;
    callerLayouts[0].byteSize = sizeof(payload);
    callerLayouts[0].byteAlign = 4u;
    callerLayouts[0].typeLayoutId = 0u;
    callerLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerLayouts[0].isParameter = ZR_FALSE;

    calleeLayouts[0].stackSlot = 0u;
    calleeLayouts[0].byteOffset = 16u;
    calleeLayouts[0].byteSize = sizeof(payload);
    calleeLayouts[0].byteAlign = 4u;
    calleeLayouts[0].typeLayoutId = 0u;
    calleeLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    calleeLayouts[0].isParameter = ZR_TRUE;

    callerFunction->stackSize = 20u;
    callerFunction->frameSlotLayouts = callerLayouts;
    callerFunction->frameSlotLayoutLength = 1u;
    callerFunction->frameByteSize = callerLayouts[0].byteOffset + callerLayouts[0].byteSize;
    callerFunction->frameByteAlign = 4u;

    test_init_compiled_struct_prototype(&prototype, sizeof(payload), 4u, 0u);
    calleeFunction->stackSize = 4u;
    calleeFunction->parameterCount = 1u;
    calleeFunction->vmEntryClearStackSizePlusOne = 2u;
    calleeFunction->frameSlotLayouts = calleeLayouts;
    calleeFunction->frameSlotLayoutLength = 1u;
    calleeFunction->frameByteSize = calleeLayouts[0].byteOffset + calleeLayouts[0].byteSize;
    calleeFunction->frameByteAlign = 4u;
    calleeFunction->prototypeData = prototypeData;
    calleeFunction->prototypeDataLength = test_write_compiled_prototype_data(
            prototypeData,
            prototypeDataLength,
            &prototype,
            ZR_NULL,
            0u);
    calleeFunction->prototypeCount = 1u;

    callerFunctionBase = state->stackTop.valuePointer;
    callerFunctionBase = ZrCore_Function_CheckStackAndGc(state, 40u, callerFunctionBase);
    callerFrameBase = callerFunctionBase + 1;
    callStackPointer = callerFrameBase + 2;

    ZrCore_Value_InitAsRawObject(state,
                                 ZrCore_Stack_GetValue(callerFunctionBase),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    ZrCore_Value_InitAsRawObject(state,
                                 ZrCore_Stack_GetValue(callStackPointer),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(calleeFunction));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            callerFunction,
            callerFrameBase,
            callerLayouts[0].stackSlot,
            &sourcePlace));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            calleeFunction,
            callStackPointer + 1,
            calleeLayouts[0].stackSlot,
            &destinationPlace));
    memcpy(sourcePlace.address, payload, sizeof(payload));
    memset(destinationPlace.address, 0xa5, sizeof(payload));

    callerCallInfo = &state->baseCallInfo;
    callerCallInfo->functionBase.valuePointer = callerFunctionBase;
    callerCallInfo->functionTop.valuePointer =
            callerFrameBase + ZrCore_Function_GetFrameStorageSlotCount(callerFunction);
    callerCallInfo->previous = ZR_NULL;
    callerCallInfo->next = ZR_NULL;
    callerCallInfo->callStatus = ZR_CALL_STATUS_NONE;
    state->callInfoList = callerCallInfo;
    state->stackTop.valuePointer = callStackPointer + 2;
    state->debugHookSignal = 0u;

    TEST_ASSERT_NULL(ZrCore_Function_TryPreCallPreparedResolvedVmFunctionExactArgsSteadyState(
            state,
            callStackPointer,
            calleeFunction,
            1u,
            1u,
            ZR_NULL));

    calleeCallInfo = ZrCore_Function_PreCallPreparedResolvedVmFunction(
            state,
            callStackPointer,
            calleeFunction,
            1u,
            1u,
            ZR_NULL);
    TEST_ASSERT_NOT_NULL(calleeCallInfo);
    TEST_ASSERT_EQUAL_PTR(callStackPointer, calleeCallInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, destinationPlace.address, sizeof(payload));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_inline_frame_gc_and_drop_scan_inline_struct_payload(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *text;
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layouts[2];
    SZrTypeLayoutField fields[1];
    SZrTypeLayout layout;
    TestFrameLayoutResolver resolver;
    TestGcValueVisit visit = {0};
    SZrStackFramePlace place;
    TZrStackValuePointer frameBase;
    SZrTypeValue *inlineValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    text = ZrCore_String_CreateFromNative(state, "inline-frame-gc-drop");
    TEST_ASSERT_NOT_NULL(text);

    fields[0].byteOffset = 0u;
    fields[0].byteSize = sizeof(SZrTypeValue);
    fields[0].typeLayoutIndex = 0u;
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(SZrTypeValue),
            ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
            fields,
            ZR_ARRAY_COUNT(fields));
    resolver.typeLayoutId = 31u;
    resolver.layout = &layout;

    layouts[0].stackSlot = 0u;
    layouts[0].byteOffset = 32u;
    layouts[0].byteSize = sizeof(SZrTypeValue);
    layouts[0].byteAlign = ZR_ALIGN_SIZE;
    layouts[0].typeLayoutId = resolver.typeLayoutId;
    layouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[0].isParameter = ZR_FALSE;
    layouts[0].reserved0 = 0u;

    layouts[1].stackSlot = 1u;
    layouts[1].byteOffset = 32u + (TZrUInt32)sizeof(SZrTypeValue);
    layouts[1].byteSize = sizeof(SZrTypeValue);
    layouts[1].byteAlign = ZR_ALIGN_SIZE;
    layouts[1].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layouts[1].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layouts[1].isParameter = ZR_FALSE;
    layouts[1].reserved0 = 0u;

    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = ZR_ARRAY_COUNT(layouts);
    function.frameByteSize = layouts[1].byteOffset + layouts[1].byteSize;
    function.frameByteAlign = ZR_ALIGN_SIZE;

    frameBase = state->stackBase.valuePointer + 2;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &function, frameBase, 0u, &place));

    inlineValue = (SZrTypeValue *)place.address;
    ZrCore_Value_InitAsRawObject(state, inlineValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    inlineValue->type = ZR_VALUE_TYPE_STRING;

    TEST_ASSERT_TRUE(ZrCore_Function_VisitInlineFrameGcValues(
            state,
            &function,
            frameBase,
            test_resolve_function_frame_layout,
            &resolver,
            test_visit_gc_value,
            &visit));
    TEST_ASSERT_EQUAL_UINT32(1u, visit.count);
    TEST_ASSERT_EQUAL_PTR(inlineValue, visit.value);

    TEST_ASSERT_TRUE(ZrCore_Function_DropInlineFrameValues(
            state,
            &function,
            frameBase,
            test_resolve_function_frame_layout,
            &resolver));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, inlineValue->type);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_post_call_drops_inline_frame_values_with_prototype_resolver(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrString *text;
    SZrCallInfo *callInfo;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer frameBase;
    SZrStackFramePlace place;
    SZrTypeValue *callableValue;
    SZrTypeValue *inlineValue;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    text = ZrCore_String_CreateFromNative(state, "post-call-inline-drop");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(text);

    test_install_managed_value_inline_frame_metadata(state, function, 0u, 0u);

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(
            state,
            1u + ZrCore_Function_GetFrameStorageSlotCount(function),
            functionBase);
    frameBase = functionBase + 1;

    callableValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(callableValue);
    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, 0u, &place));
    inlineValue = (SZrTypeValue *)place.address;
    ZrCore_Value_InitAsRawObject(state, inlineValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    inlineValue->type = ZR_VALUE_TYPE_STRING;

    callInfo = &state->baseCallInfo;
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = frameBase + ZrCore_Function_GetFrameStorageSlotCount(function);
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->previous = ZR_NULL;
    callInfo->next = ZR_NULL;
    callInfo->expectedReturnCount = 1u;
    callInfo->returnDestination = ZR_NULL;
    callInfo->returnDestinationReusableOffset = 0u;
    callInfo->hasReturnDestination = ZR_FALSE;
    state->callInfoList = callInfo;
    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    state->debugHookSignal = 0u;

    ZrCore_Function_PostCall(state, callInfo, 0u);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, inlineValue->type);
    TEST_ASSERT_EQUAL_PTR(functionBase + 1, state->stackTop.valuePointer);
    TEST_ASSERT_NULL(state->callInfoList);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_post_call_copies_inline_return_payload_before_frame_drop(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *calleeFunction;
    SZrFunctionFrameSlotLayout *callerLayouts;
    SZrFunctionFrameSlotLayout *calleeLayouts;
    SZrCompiledPrototypeInfo callerPrototype;
    SZrCompiledPrototypeInfo calleePrototype;
    TZrByte *callerPrototypeData;
    TZrByte *calleePrototypeData;
    TZrUInt32 prototypeDataLength = (TZrUInt32)(sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo));
    TZrStackValuePointer callerFunctionBase;
    TZrStackValuePointer callerFrameBase;
    TZrStackValuePointer calleeFunctionBase;
    TZrStackValuePointer calleeFrameBase;
    TZrStackValuePointer returnSource;
    TZrStackValuePointer returnDestination;
    SZrStackFramePlace sourcePlace;
    SZrStackFramePlace destinationPlace;
    SZrCallInfo callerCallInfo;
    SZrCallInfo calleeCallInfo;
    TZrByte payload[12] = {
            0x21u, 0x32u, 0x43u, 0x54u, 0x65u, 0x76u,
            0x87u, 0x98u, 0xa9u, 0xbau, 0xcbu, 0xdcu};

    TEST_ASSERT_NOT_NULL(state);

    callerFunction = ZrCore_Function_New(state);
    calleeFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    callerLayouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    calleeLayouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    callerPrototypeData = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            prototypeDataLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    calleePrototypeData = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            prototypeDataLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(callerLayouts);
    TEST_ASSERT_NOT_NULL(calleeLayouts);
    TEST_ASSERT_NOT_NULL(callerPrototypeData);
    TEST_ASSERT_NOT_NULL(calleePrototypeData);
    memset(callerLayouts, 0, sizeof(SZrFunctionFrameSlotLayout));
    memset(calleeLayouts, 0, sizeof(SZrFunctionFrameSlotLayout));

    callerLayouts[0].stackSlot = 4u;
    callerLayouts[0].byteOffset = 160u;
    callerLayouts[0].byteSize = sizeof(payload);
    callerLayouts[0].byteAlign = 4u;
    callerLayouts[0].typeLayoutId = 0u;
    callerLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerLayouts[0].isParameter = ZR_FALSE;

    calleeLayouts[0].stackSlot = 1u;
    calleeLayouts[0].byteOffset = 48u;
    calleeLayouts[0].byteSize = sizeof(payload);
    calleeLayouts[0].byteAlign = 4u;
    calleeLayouts[0].typeLayoutId = 0u;
    calleeLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    calleeLayouts[0].isParameter = ZR_FALSE;

    test_init_compiled_struct_prototype(&callerPrototype, sizeof(payload), 4u, 0u);
    test_init_compiled_struct_prototype(&calleePrototype, sizeof(payload), 4u, 0u);

    callerFunction->stackSize = 16u;
    callerFunction->frameSlotLayouts = callerLayouts;
    callerFunction->frameSlotLayoutLength = 1u;
    callerFunction->frameByteSize = callerLayouts[0].byteOffset + callerLayouts[0].byteSize;
    callerFunction->frameByteAlign = 4u;
    callerFunction->prototypeData = callerPrototypeData;
    callerFunction->prototypeDataLength = test_write_compiled_prototype_data(
            callerPrototypeData,
            prototypeDataLength,
            &callerPrototype,
            ZR_NULL,
            0u);
    callerFunction->prototypeCount = 1u;

    calleeFunction->stackSize = 8u;
    calleeFunction->frameSlotLayouts = calleeLayouts;
    calleeFunction->frameSlotLayoutLength = 1u;
    calleeFunction->frameByteSize = calleeLayouts[0].byteOffset + calleeLayouts[0].byteSize;
    calleeFunction->frameByteAlign = 4u;
    calleeFunction->prototypeData = calleePrototypeData;
    calleeFunction->prototypeDataLength = test_write_compiled_prototype_data(
            calleePrototypeData,
            prototypeDataLength,
            &calleePrototype,
            ZR_NULL,
            0u);
    calleeFunction->prototypeCount = 1u;

    callerFunctionBase = state->stackTop.valuePointer;
    callerFunctionBase = ZrCore_Function_CheckStackAndGc(state, 64u, callerFunctionBase);
    callerFrameBase = callerFunctionBase + 1;
    calleeFunctionBase = callerFrameBase + 8;
    calleeFrameBase = calleeFunctionBase + 1;
    returnSource = calleeFrameBase + calleeLayouts[0].stackSlot;
    returnDestination = callerFrameBase + callerLayouts[0].stackSlot;

    ZrCore_Value_InitAsRawObject(state,
                                 ZrCore_Stack_GetValue(callerFunctionBase),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    ZrCore_Value_InitAsRawObject(state,
                                 ZrCore_Stack_GetValue(calleeFunctionBase),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(calleeFunction));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            calleeFunction,
            calleeFrameBase,
            calleeLayouts[0].stackSlot,
            &sourcePlace));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            state,
            callerFunction,
            callerFrameBase,
            callerLayouts[0].stackSlot,
            &destinationPlace));
    memcpy(sourcePlace.address, payload, sizeof(payload));
    memset(destinationPlace.address, 0xa5, sizeof(payload));

    memset(&callerCallInfo, 0, sizeof(callerCallInfo));
    callerCallInfo.functionBase.valuePointer = callerFunctionBase;
    callerCallInfo.functionTop.valuePointer =
            callerFrameBase + ZrCore_Function_GetFrameStorageSlotCount(callerFunction);
    callerCallInfo.callStatus = ZR_CALL_STATUS_NONE;

    memset(&calleeCallInfo, 0, sizeof(calleeCallInfo));
    calleeCallInfo.functionBase.valuePointer = calleeFunctionBase;
    calleeCallInfo.functionTop.valuePointer =
            calleeFrameBase + ZrCore_Function_GetFrameStorageSlotCount(calleeFunction);
    calleeCallInfo.previous = &callerCallInfo;
    calleeCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    calleeCallInfo.expectedReturnCount = 1u;
    calleeCallInfo.returnDestination = returnDestination;
    calleeCallInfo.hasReturnDestination = ZR_TRUE;

    callerCallInfo.next = &calleeCallInfo;
    state->callInfoList = &calleeCallInfo;
    state->stackTop.valuePointer = returnSource + 1;
    state->debugHookSignal = 0u;

    ZrCore_Function_PostCall(state, &calleeCallInfo, 1u);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, destinationPlace.address, sizeof(payload));
    TEST_ASSERT_EQUAL_PTR(returnDestination + 1, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(&callerCallInfo, state->callInfoList);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_pod_struct_inline_copy_uses_byte_span_and_handles_overlap);
    RUN_TEST(test_managed_struct_inline_copy_copies_and_drops_value_fields_by_layout);
    RUN_TEST(test_gc_only_value_field_drop_keeps_non_owned_reference_value);
    RUN_TEST(test_value_layout_inline_copy_uses_value_semantics_for_struct_objects);
    RUN_TEST(test_frame_layout_assigns_aligned_byte_offsets_for_inline_structs);
    RUN_TEST(test_stack_inline_copy_uses_byte_offsets_from_stack_base);
    RUN_TEST(test_stack_frame_place_resolves_byte_offset_from_frame_base);
    RUN_TEST(test_stack_inline_place_copy_uses_resolved_frame_places);
    RUN_TEST(test_function_frame_slot_place_uses_function_layout_metadata);
    RUN_TEST(test_function_frame_slot_inline_copy_uses_source_and_destination_layouts);
    RUN_TEST(test_function_prototype_type_layout_resolver_accepts_pod_struct_metadata);
    RUN_TEST(test_function_prototype_type_layout_resolver_accepts_primitive_scalar_field_as_pod);
    RUN_TEST(test_function_prototype_type_layout_resolver_builds_managed_value_field_layout);
    RUN_TEST(test_function_prototype_type_layout_resolver_builds_gc_value_field_for_value_sized_reference_type);
    RUN_TEST(test_function_prototype_type_layout_resolver_flattens_nested_managed_struct_fields);
    RUN_TEST(test_function_prototype_type_layout_resolver_marks_nested_inline_struct_field);
    RUN_TEST(test_function_prototype_type_layout_resolver_fails_when_managed_field_is_not_value_sized);
    RUN_TEST(test_function_prototype_type_layout_resolver_fails_reference_pointer_field_without_value_layout);
    RUN_TEST(test_function_prototype_type_layout_resolver_fails_unknown_nonlocal_field_metadata);
    RUN_TEST(test_function_prototype_type_layout_resolver_fails_recursive_struct_metadata);
    RUN_TEST(test_function_inline_parameters_copy_by_frame_layout_copies_byte_payloads);
    RUN_TEST(test_value_frame_parameter_copy_uses_dense_source_when_frame_value_slot_is_unmaterialized);
    RUN_TEST(test_value_frame_parameter_copy_normalizes_reused_destination_slots);
    RUN_TEST(test_value_copy_normalizes_reused_no_ownership_destination);
    RUN_TEST(test_prepared_resolved_vm_precall_copies_inline_parameter_payload_from_caller_frame);
    RUN_TEST(test_function_inline_frame_gc_and_drop_scan_inline_struct_payload);
    RUN_TEST(test_function_post_call_drops_inline_frame_values_with_prototype_resolver);
    RUN_TEST(test_function_post_call_copies_inline_return_payload_before_frame_drop);

    return UNITY_END();
}
