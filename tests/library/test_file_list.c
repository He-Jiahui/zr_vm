#include <stdio.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_library/file.h"

/* Unity 用例独占临时根目录；列表可被断言助手提前释放，tearDown 仍须可重入清理。 */
static TZrChar g_file_list_root[ZR_LIBRARY_MAX_PATH_LENGTH];
static SZrLibrary_File_List g_file_list;
static TZrBool g_file_list_owns_root;

/* 各用例使用独立生成目录，避免排序和空结果断言受残留文件干扰。 */
void setUp(void) {
    static unsigned int sequence;
    TZrChar name[96];
    TZrChar normalized[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(&g_file_list, 0, sizeof(g_file_list));
    g_file_list_owns_root = ZR_FALSE;
    snprintf(name, sizeof(name), "query_%lld_%lld_%u",
             (long long)time(ZR_NULL), (long long)clock(), sequence++);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "file_list", "native", name, ".tmp", g_file_list_root,
            sizeof(g_file_list_root)));
    TEST_ASSERT_TRUE(ZrLibrary_File_NormalizePath(
            g_file_list_root, normalized, sizeof(normalized)));
    strcpy(g_file_list_root, normalized);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_FILE_NOT_EXIST,
                          ZrLibrary_File_Exist(g_file_list_root));
    TEST_ASSERT_TRUE(ZrLibrary_File_CreateDirectorySingle(g_file_list_root));
    g_file_list_owns_root = ZR_TRUE;
}

/* 测试失败也要归还列表与目录；所有权标志避免创建失败时删除别人的路径。 */
void tearDown(void) {
    ZrLibrary_File_List_Free(&g_file_list);
    if (g_file_list_owns_root) {
        g_file_list_owns_root = ZR_FALSE;
        TEST_ASSERT_TRUE(ZrLibrary_File_Delete(g_file_list_root, ZR_TRUE));
    }
}

/* 通过公开文件 API 建夹具，同时覆盖嵌套路径创建的入口。 */
static void file_list_create_file(const TZrChar *relativePath) {
    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    ZrLibrary_File_PathJoin(g_file_list_root, relativePath, path);
    TEST_ASSERT_TRUE(ZrLibrary_File_CreateEmpty(path, ZR_TRUE));
}

/* 空结果应保留零容量且允许释放，不能悄悄分配空列表缓冲区。 */
static void file_list_assert_empty(void) {
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)g_file_list.count);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)g_file_list.capacity);
    TEST_ASSERT_NULL(g_file_list.entries);
    ZrLibrary_File_List_Free(&g_file_list);
}

/* 比较规范化后的完整路径和顺序；列表所有权在比较后交回库释放。 */
static void file_list_assert_paths(const TZrChar *const *expected, TZrSize count) {
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)count, (TZrUInt32)g_file_list.count);
    for (TZrSize index = 0U; index < count; index++) {
        TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrChar normalized[ZR_LIBRARY_MAX_PATH_LENGTH];
        ZrLibrary_File_PathJoin(g_file_list_root, expected[index], path);
        TEST_ASSERT_TRUE(ZrLibrary_File_NormalizePath(path, normalized, sizeof(normalized)));
        TEST_ASSERT_EQUAL_STRING(normalized, g_file_list.entries[index].path);
    }
    ZrLibrary_File_List_Free(&g_file_list);
}

/* 直接与递归遍历空目录都不得产生条目或存储。 */
static void test_file_list_empty_directory_has_no_storage(void) {
    TEST_ASSERT_TRUE(ZrLibrary_File_ListDirectory(g_file_list_root, ZR_FALSE, &g_file_list));
    file_list_assert_empty();
    TEST_ASSERT_TRUE(ZrLibrary_File_ListDirectory(g_file_list_root, ZR_TRUE, &g_file_list));
    file_list_assert_empty();
}

/* glob 的空目录结果与 ListDirectory 的零容量约定一致。 */
static void test_file_glob_empty_directory_has_no_storage(void) {
    TEST_ASSERT_TRUE(ZrLibrary_File_Glob(g_file_list_root, "*", ZR_FALSE, &g_file_list));
    file_list_assert_empty();
    TEST_ASSERT_TRUE(ZrLibrary_File_Glob(g_file_list_root, "*", ZR_TRUE, &g_file_list));
    file_list_assert_empty();
}

/* 目录非空但无模式命中时也不能留下分配的列表。 */
static void test_file_glob_no_matches_has_no_storage(void) {
    file_list_create_file("present.txt");
    TEST_ASSERT_TRUE(ZrLibrary_File_Glob(g_file_list_root, "*.missing", ZR_FALSE, &g_file_list));
    file_list_assert_empty();
    TEST_ASSERT_TRUE(ZrLibrary_File_Glob(g_file_list_root, "*.missing", ZR_TRUE, &g_file_list));
    file_list_assert_empty();
}

/* 单条目录项和 glob 命中均应保留规范化路径。 */
static void test_file_list_and_glob_preserve_single_entry(void) {
    const TZrChar *expected[] = {"only.txt"};
    file_list_create_file(expected[0]);
    TEST_ASSERT_TRUE(ZrLibrary_File_ListDirectory(g_file_list_root, ZR_FALSE, &g_file_list));
    file_list_assert_paths(expected, 1U);
    TEST_ASSERT_TRUE(ZrLibrary_File_Glob(g_file_list_root, "*.txt", ZR_TRUE, &g_file_list));
    file_list_assert_paths(expected, 1U);
}

/* 直接、递归和过滤列表都应保持确定的词典序供上层模块搜索使用。 */
static void test_file_list_and_glob_keep_lexical_order(void) {
    const TZrChar *direct[] = {"a.txt", "b.dat", "nested", "z.txt"};
    const TZrChar *recursive[] = {"a.txt", "b.dat", "nested", "nested/c.txt", "z.txt"};
    const TZrChar *matching[] = {"a.txt", "nested/c.txt", "z.txt"};
    file_list_create_file("z.txt");
    file_list_create_file("nested/c.txt");
    file_list_create_file("b.dat");
    file_list_create_file("a.txt");
    TEST_ASSERT_TRUE(ZrLibrary_File_ListDirectory(g_file_list_root, ZR_FALSE, &g_file_list));
    file_list_assert_paths(direct, 4U);
    TEST_ASSERT_TRUE(ZrLibrary_File_ListDirectory(g_file_list_root, ZR_TRUE, &g_file_list));
    file_list_assert_paths(recursive, 5U);
    TEST_ASSERT_TRUE(ZrLibrary_File_Glob(g_file_list_root, "*.txt", ZR_TRUE, &g_file_list));
    file_list_assert_paths(matching, 3U);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_file_list_empty_directory_has_no_storage);
    RUN_TEST(test_file_glob_empty_directory_has_no_storage);
    RUN_TEST(test_file_glob_no_matches_has_no_storage);
    RUN_TEST(test_file_list_and_glob_preserve_single_entry);
    RUN_TEST(test_file_list_and_glob_keep_lexical_order);
    return UNITY_END();
}
