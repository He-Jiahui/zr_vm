#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

/* Declared lookup tables only: IDs and zero hashes have no interner/Binder provenance. */
static SZrCanonicalTypeNode g_rows[3];
static SZrSemanticContext g_context;

static void table(SZrSemanticContext *context,SZrCanonicalTypeNode *rows,TZrSize count,TZrSize capacity){
    memset(context,0,sizeof(*context));
    context->canonicalTypes.head=(TZrUInt8 *)rows;
    context->canonicalTypes.elementSize=sizeof(*rows);
    context->canonicalTypes.length=count;
    context->canonicalTypes.capacity=capacity;
    context->canonicalTypes.isValid=ZR_TRUE;
}
void setUp(void){
    memset(g_rows,0,sizeof(g_rows));
    g_rows[0].id=17u;g_rows[1].id=43u;g_rows[2].id=109u;
    table(&g_context,g_rows,3u,3u);
}
void tearDown(void){}

static void test_null_context_and_invalid_id_return_null(void){
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(ZR_NULL,17u));
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(ZR_NULL,ZR_SEMANTIC_ID_INVALID));
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&g_context,ZR_SEMANTIC_ID_INVALID));
}
static void test_empty_declared_table_returns_null(void){
    SZrSemanticContext context;
    table(&context,ZR_NULL,0u,0u);
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&context,1u));
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&context,UINT32_MAX));
    /* A readable spare row is also untouched when logical length is zero. */
    table(&context,g_rows,0u,3u);
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&context,17u));
}
static void test_single_declared_row_returns_its_borrowed_address(void){
    SZrCanonicalTypeNode row={0};SZrSemanticContext context;
    row.id=71u;table(&context,&row,1u,1u);
    TEST_ASSERT_EQUAL_PTR(&row,ZrParser_CanonicalType_Find(&context,71u));
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&context,70u));
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&context,72u));
}
static void test_sparse_first_middle_last_return_exact_rows(void){
    TEST_ASSERT_EQUAL_PTR(&g_rows[0],ZrParser_CanonicalType_Find(&g_context,17u));
    TEST_ASSERT_EQUAL_PTR(&g_rows[1],ZrParser_CanonicalType_Find(&g_context,43u));
    TEST_ASSERT_EQUAL_PTR(&g_rows[2],ZrParser_CanonicalType_Find(&g_context,109u));
}
static void test_gaps_outside_and_maximum_id_are_distinct(void){
    const TZrTypeId misses[]={1u,16u,18u,42u,44u,108u,110u,UINT32_MAX};
    SZrCanonicalTypeNode maximum[2]={{0},{0}};SZrSemanticContext context;
    for(unsigned i=0u;i<sizeof(misses)/sizeof(misses[0]);++i)
        TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&g_context,misses[i]));
    maximum[0].id=UINT32_MAX-2u;maximum[1].id=UINT32_MAX;
    table(&context,maximum,2u,2u);
    TEST_ASSERT_EQUAL_PTR(&maximum[0],ZrParser_CanonicalType_Find(&context,UINT32_MAX-2u));
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(&context,UINT32_MAX-1u));
    TEST_ASSERT_EQUAL_PTR(&maximum[1],ZrParser_CanonicalType_Find(&context,UINT32_MAX));
}
static void test_independent_contexts_keep_equal_ids_in_distinct_storage(void){
    SZrCanonicalTypeNode row={0};SZrSemanticContext context;
    const SZrCanonicalTypeNode *first,*second;
    row.id=43u;table(&context,&row,1u,1u);
    first=ZrParser_CanonicalType_Find(&g_context,43u);
    second=ZrParser_CanonicalType_Find(&context,43u);
    TEST_ASSERT_EQUAL_PTR(&g_rows[1],first);TEST_ASSERT_EQUAL_PTR(&row,second);
    TEST_ASSERT_TRUE(first!=second);TEST_ASSERT_EQUAL_UINT32(first->id,second->id);
}
static void test_repeated_mixed_queries_preserve_borrowed_identity(void){
    const TZrTypeId ids[]={109u,18u,17u,UINT32_MAX,43u,0u,108u,43u};
    const SZrCanonicalTypeNode *expected[]={&g_rows[2],ZR_NULL,&g_rows[0],ZR_NULL,&g_rows[1],ZR_NULL,ZR_NULL,&g_rows[1]};
    for(unsigned repeat=0u;repeat<16u;++repeat)
        for(unsigned i=0u;i<sizeof(ids)/sizeof(ids[0]);++i)
            TEST_ASSERT_EQUAL_PTR(expected[i],ZrParser_CanonicalType_Find(&g_context,ids[i]));
}
static void test_complete_context_header_and_declared_nodes_remain_const(void){
    unsigned char beforeContext[sizeof(g_context)],beforeRows[sizeof(g_rows)];
    const TZrTypeId ids[]={0u,1u,17u,18u,43u,108u,109u,110u,UINT32_MAX};
    /* Complete live input representations are witnesses, never alternate inputs. */
    memcpy(beforeContext,&g_context,sizeof(g_context));memcpy(beforeRows,g_rows,sizeof(g_rows));
    for(unsigned i=0u;i<sizeof(ids)/sizeof(ids[0]);++i)
        (void)ZrParser_CanonicalType_Find(&g_context,ids[i]);
    TEST_ASSERT_EQUAL_MEMORY(beforeContext,&g_context,sizeof(g_context));
    TEST_ASSERT_EQUAL_MEMORY(beforeRows,g_rows,sizeof(g_rows));
}
int main(void){UNITY_BEGIN();
    RUN_TEST(test_null_context_and_invalid_id_return_null);
    RUN_TEST(test_empty_declared_table_returns_null);
    RUN_TEST(test_single_declared_row_returns_its_borrowed_address);
    RUN_TEST(test_sparse_first_middle_last_return_exact_rows);
    RUN_TEST(test_gaps_outside_and_maximum_id_are_distinct);
    RUN_TEST(test_independent_contexts_keep_equal_ids_in_distinct_storage);
    RUN_TEST(test_repeated_mixed_queries_preserve_borrowed_identity);
    RUN_TEST(test_complete_context_header_and_declared_nodes_remain_const);
    return UNITY_END();}
