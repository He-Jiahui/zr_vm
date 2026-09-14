#include "unity.h"
#include "zr_vm_core/object_layout_map.h"
#include "zr_vm_parser/exec_ir_layout_visibility.h"

void setUp(void) {}
void tearDown(void) {}

static void test_layout_map_resolves_and_rejects_stale_shape(void) {
    SZrObjectLayoutMapEntry entry = { 7u, 8u, 12u, 16u, 8u, 0u };
    SZrObjectLayoutMap map = { 3u, 4u, 5u, 11u, 22u, &entry, 1u, ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE };
    SZrObjectMemberLocation location;
    TEST_ASSERT_EQUAL(ZR_OBJECT_LAYOUT_MAP_OK, ZrCore_Object_ResolveLayoutMember(&map, 7u, 4u, &location));
    TEST_ASSERT_EQUAL_UINT32(12u, location.physicalOffset);
    TEST_ASSERT_EQUAL(ZR_OBJECT_LAYOUT_MAP_STALE_SHAPE, ZrCore_Object_ResolveLayoutMember(&map, 7u, 9u, &location));
}

static void test_visibility_blocks_public_transform(void) {
    SZrExecIrLayoutVisibility visibility = { ZR_TRUE, ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_FALSE };
    EZrExecIrLayoutVisibilityReason reason;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_LayoutCanTransform(&visibility, &reason));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_LAYOUT_REFLECTION_VISIBLE, reason);
    visibility.reflectionVisible = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LayoutCanTransform(&visibility, &reason));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_layout_map_resolves_and_rejects_stale_shape);
    RUN_TEST(test_visibility_blocks_public_transform);
    return UNITY_END();
}
