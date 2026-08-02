#ifndef ZR_VM_TEST_CLI_COMPTIME_CACHE_INCREMENTAL_CASES_H
#define ZR_VM_TEST_CLI_COMPTIME_CACHE_INCREMENTAL_CASES_H

static TZrBool cli_comptime_cache_read_bytes(
        const TZrChar *path,
        TZrByte **outBytes,
        TZrSize *outSize) {
    FILE *file;
    long length;
    TZrByte *bytes;

    if (path == ZR_NULL || outBytes == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }
    *outBytes = ZR_NULL;
    *outSize = 0U;
    file = fopen(path, "rb");
    if (file == ZR_NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != ZR_NULL) {
            fclose(file);
        }
        return ZR_FALSE;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }
    bytes = (TZrByte *)malloc((size_t)(length > 0 ? length : 1));
    if (bytes == ZR_NULL ||
        (length > 0 && fread(bytes, 1, (size_t)length, file) != (size_t)length)) {
        free(bytes);
        fclose(file);
        return ZR_FALSE;
    }
    fclose(file);
    *outBytes = bytes;
    *outSize = (TZrSize)length;
    return ZR_TRUE;
}

static void test_cli_incremental_persists_comptime_cache_and_recovers_corruption(void) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"name\": \"comptime_cache_incremental\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";
    static const TZrChar *sourceContent =
            "comptime fn twice(value: int): int { return value * 2; }\n"
            "comptime if (twice(21) == 42) {\n"
            "    pub fn answer(): int { return 42; }\n"
            "}\n"
            "return 42;\n";
    static const TZrChar *semanticEditContent =
            "comptime fn twice(value: int): int { return value + 2; }\n"
            "comptime if (twice(21) == 23) {\n"
            "    pub fn answer(): int { return 42; }\n"
            "}\n"
            "return 42;\n";
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    SZrCliCommand command;
    SZrCliCompileSummary firstSummary;
    SZrCliCompileSummary hitSummary;
    SZrCliCompileSummary recoveredSummary;
    SZrCliCompileSummary semanticEditSummary;
    SZrCliProjectContext context;
    SZrGlobalState *global;
    TZrByte *firstZro = ZR_NULL;
    TZrByte *hitZro = ZR_NULL;
    TZrByte *recoveredZro = ZR_NULL;
    TZrByte *semanticEditZro = ZR_NULL;
    TZrByte *cacheBytes = ZR_NULL;
    TZrSize firstZroSize = 0U;
    TZrSize hitZroSize = 0U;
    TZrSize recoveredZroSize = 0U;
    TZrSize semanticEditZroSize = 0U;
    TZrSize cacheSize = 0U;

    memset(&firstSummary, 0, sizeof(firstSummary));
    memset(&hitSummary, 0, sizeof(hitSummary));
    memset(&recoveredSummary, 0, sizeof(recoveredSummary));
    memset(&semanticEditSummary, 0, sizeof(semanticEditSummary));
    memset(&context, 0, sizeof(context));
    build_generated_project_root(
            "comptime_cache_incremental", projectRoot, sizeof(projectRoot));
    clean_directory_tree(projectRoot);
    ZrLibrary_File_PathJoin(
            projectRoot, "comptime_cache_incremental.zrp", projectPath);
    ZrLibrary_File_PathJoin(projectRoot, "src/main.zr", sourcePath);
    TEST_ASSERT_NOT_EQUAL('\0', projectPath[0]);
    TEST_ASSERT_NOT_EQUAL('\0', sourcePath[0]);
    TEST_ASSERT_TRUE(write_text_file(projectPath, projectContent));
    TEST_ASSERT_TRUE(write_text_file(sourcePath, sourceContent));

    init_incremental_compile_command(&command, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&command, &firstSummary));
    TEST_ASSERT_EQUAL_UINT32(1U, (unsigned int)firstSummary.compiledCount);
    TEST_ASSERT_EQUAL_UINT64(0U, firstSummary.comptimeCacheHitCount);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, firstSummary.comptimeCacheMissCount);

    global = ZrCli_Project_CreateProjectGlobal(projectPath);
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(ZrCli_ProjectContext_FromGlobal(&context, global, projectPath));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveBinaryPath(
            &context, "main", zroPath, sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(context.comptimeCachePath));
    TEST_ASSERT_TRUE(cli_comptime_cache_read_bytes(
            context.comptimeCachePath, &cacheBytes, &cacheSize));
    TEST_ASSERT_GREATER_THAN_UINT32(48U, (unsigned int)cacheSize);
    TEST_ASSERT_EQUAL_MEMORY("ZRCCV005", cacheBytes, 8U);
    free(cacheBytes);
    cacheBytes = ZR_NULL;
    TEST_ASSERT_TRUE(cli_comptime_cache_read_bytes(
            zroPath, &firstZro, &firstZroSize));

    TEST_ASSERT_TRUE(ZrCli_Project_RemoveFileIfExists(zroPath));
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&command, &hitSummary));
    TEST_ASSERT_EQUAL_UINT32(1U, (unsigned int)hitSummary.compiledCount);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, hitSummary.comptimeCacheHitCount);
    TEST_ASSERT_EQUAL_UINT64(0U, hitSummary.comptimeCacheRejectedCount);
    TEST_ASSERT_TRUE(cli_comptime_cache_read_bytes(zroPath, &hitZro, &hitZroSize));
    TEST_ASSERT_EQUAL_UINT64(firstZroSize, hitZroSize);
    TEST_ASSERT_EQUAL_MEMORY(firstZro, hitZro, firstZroSize);

    TEST_ASSERT_EQUAL_UINT64(strlen(sourceContent), strlen(semanticEditContent));
    TEST_ASSERT_TRUE(write_text_file(sourcePath, semanticEditContent));
    TEST_ASSERT_TRUE(ZrCli_Project_RemoveFileIfExists(zroPath));
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(
            &command, &semanticEditSummary));
    TEST_ASSERT_EQUAL_UINT64(0U, semanticEditSummary.comptimeCacheHitCount);
    TEST_ASSERT_GREATER_THAN_UINT64(
            0U, semanticEditSummary.comptimeCacheMissCount);
    TEST_ASSERT_TRUE(cli_comptime_cache_read_bytes(
            zroPath, &semanticEditZro, &semanticEditZroSize));
    TEST_ASSERT_FALSE(
            semanticEditZroSize == firstZroSize &&
            memcmp(firstZro, semanticEditZro, firstZroSize) == 0);

    TEST_ASSERT_TRUE(write_text_file(sourcePath, sourceContent));
    TEST_ASSERT_TRUE(write_text_file(context.comptimeCachePath, "corrupt"));
    TEST_ASSERT_TRUE(ZrCli_Project_RemoveFileIfExists(zroPath));
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(
            &command, &recoveredSummary));
    TEST_ASSERT_EQUAL_UINT64(1U, recoveredSummary.comptimeCacheRejectedCount);
    TEST_ASSERT_GREATER_THAN_UINT64(0U, recoveredSummary.comptimeCacheMissCount);
    TEST_ASSERT_TRUE(cli_comptime_cache_read_bytes(
            context.comptimeCachePath, &cacheBytes, &cacheSize));
    TEST_ASSERT_GREATER_THAN_UINT32(48U, (unsigned int)cacheSize);
    TEST_ASSERT_EQUAL_MEMORY("ZRCCV005", cacheBytes, 8U);
    TEST_ASSERT_TRUE(cli_comptime_cache_read_bytes(
            zroPath, &recoveredZro, &recoveredZroSize));
    TEST_ASSERT_EQUAL_UINT64(firstZroSize, recoveredZroSize);
    TEST_ASSERT_EQUAL_MEMORY(firstZro, recoveredZro, firstZroSize);

    free(recoveredZro);
    free(semanticEditZro);
    free(hitZro);
    free(firstZro);
    free(cacheBytes);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

#endif
