#ifndef ZR_TESTS_REFERENCE_RECEIVER_LAYOUT_CONTRACT_SUPPORT_H
#define ZR_TESTS_REFERENCE_RECEIVER_LAYOUT_CONTRACT_SUPPORT_H

#include "zr_vm_core/type_layout.h"

static void assert_i64_receiver_storage_layout(
        const SZrFunction *function,
        const SZrCompiledPrototypeInfo *prototype,
        const SZrCompiledMemberInfo *members,
        const SZrFunction *readFunction) {
    const TZrUInt32 byteSize = (TZrUInt32)sizeof(TZrInt64);
    const TZrUInt32 byteAlign = (TZrUInt32)_Alignof(TZrInt64);
    const SZrFunctionFrameSlotLayout *argument =
            find_inline_receiver_argument_layout(function);
    const SZrFunctionFrameSlotLayout *receiver =
            ZrCore_Function_FindFrameSlotLayout(readFunction, 0U);
    const SZrTypeLayout *layout;

    TEST_ASSERT_NOT_NULL(argument);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, prototype->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTANT_STRUCT_FIELD, members[0].memberType);
    TEST_ASSERT_EQUAL_UINT32(0U, members[0].fieldOffset);
    TEST_ASSERT_EQUAL_UINT32(byteSize, members[0].fieldSize);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(byteAlign, prototype->layoutByteAlign,
            "int-only struct alignment must follow its TZrInt64 storage");
    TEST_ASSERT_EQUAL_UINT32(byteSize, prototype->layoutByteSize);
    layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
            function, argument->typeLayoutId, g_state);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(layout));
    TEST_ASSERT_EQUAL_INT(ZR_TYPE_LAYOUT_KIND_STRUCT, layout->kind);
    TEST_ASSERT_EQUAL_UINT32(byteAlign, layout->byteAlign);
    TEST_ASSERT_EQUAL_UINT32(byteSize, layout->byteSize);
    TEST_ASSERT_EQUAL_UINT32(1U, layout->fieldCount);
    TEST_ASSERT_NOT_NULL(layout->fields);
    TEST_ASSERT_EQUAL_UINT32(0U, layout->fields[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(byteSize, layout->fields[0].byteSize);
    TEST_ASSERT_BITS_LOW(ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT,
            layout->fields[0].flags);
    TEST_ASSERT_EQUAL_UINT32(byteAlign, argument->byteAlign);
    TEST_ASSERT_EQUAL_UINT32(byteSize, argument->byteSize);
    TEST_ASSERT_EQUAL_UINT32(byteAlign, receiver->byteAlign);
    TEST_ASSERT_EQUAL_UINT32(byteSize, receiver->byteSize);
    TEST_ASSERT_EQUAL_UINT32(0U, argument->byteOffset % byteAlign);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(byteAlign, function->frameByteAlign);
}

#endif
