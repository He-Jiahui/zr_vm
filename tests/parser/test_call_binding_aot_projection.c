#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.h"

/* Unity 单用例 VM，由 tearDown 释放；测试局部 malloc 缓冲区不归此状态管理。 */
static SZrState *g_state;

/* 供注册表模拟 AOT 函数地址，绑定检查只验证指针身份而不执行返回值。 */
static TZrInt64 test_runtime_aot_thunk(struct SZrState *state) {
    (void)state;
    return 71;
}

/* 注册表使用的空 invoker；此组用例只检验重定位，不发起 AOT 调用。 */
static void test_runtime_aot_invoker(struct SZrState *state,
                                     FZrAotEntryThunk target,
                                     const SZrAotMethodInfo *method,
                                     SZrTypeValue *self,
                                     SZrTypeValue *args,
                                     SZrTypeValue *outReturn) {
    (void)state;
    (void)target;
    (void)method;
    (void)self;
    (void)args;
    (void)outReturn;
}

/** @brief 为每个绑定投影用例创建独立 VM。 */
void setUp(void) { g_state = ZrTests_Runtime_State_Create(ZR_NULL); }
/** @brief 回收当前用例 VM；局部堆资源仍须在各测试函数中释放。 */
void tearDown(void) { ZrTests_Runtime_State_Destroy(g_state); }

/* 使用虚方法调用生成真实 call-site cache，供源端投影和运行时链接共用。 */
static SZrFunction *compile_source(void) {
    const char *source = "class Box { pub virtual fn value(): int { return 29; } } var box = new Box(); return box.value();";
    SZrString *name = ZrCore_String_Create(g_state, "call_binding_aot.zr", 19u);
    return ZrParser_Source_Compile(g_state, source, strlen(source), name);
}

/* 投影行保留原始契约和位置，并拒绝被篡改的签名哈希。 */
static void test_aot_projection_preserves_contract_and_resolves_index(void) {
    SZrFunction *function = compile_source();
    SZrAotFunctionTable table;
    TZrUInt32 bindings = 0u;
    memset(&table, 0, sizeof(table));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(backend_aot_build_function_table(g_state, function, &table));
    for (TZrUInt32 index = 0u; index < table.count; ++index) {
        const SZrAotFunctionEntry *entry = &table.entries[index];
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            const SZrFunctionCallSiteCacheEntry *cache = &entry->function->callSiteCaches[cacheIndex];
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            if (cache->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            TEST_ASSERT_TRUE(backend_aot_c_project_call_binding(
                    g_state, &table, entry, cacheIndex, &row, &targetIndex, ZR_NULL));
            TEST_ASSERT_EQUAL_MEMORY(&cache->binding.contract, &row.contract, sizeof(row.contract));
            TEST_ASSERT_EQUAL_MEMORY(&cache->bindingLocation, &row.location, sizeof(row.location));
            TEST_ASSERT_EQUAL_UINT32(entry->flatIndex, row.functionIndex);
            TEST_ASSERT_EQUAL_UINT32(cacheIndex, row.cacheIndex);
            TEST_ASSERT_LESS_THAN_UINT32(table.indexSpace, targetIndex);
            {
                SZrArtifactDiagnostic diagnostic;
                SZrFunctionCallSiteCacheEntry *mutableCache = &entry->function->callSiteCaches[cacheIndex];
                mutableCache->binding.contract.signatureHash ^= 1u;
                TEST_ASSERT_FALSE(backend_aot_c_project_call_binding(
                        g_state, &table, entry, cacheIndex, &row, &targetIndex, &diagnostic));
                TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH, diagnostic.status);
                mutableCache->binding.contract.signatureHash ^= 1u;
            }
            ++bindings;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, bindings);
    backend_aot_release_function_table(g_state, &table);
}

/* 去掉目标函数表后，投影必须失败且不能留下看似有效的绑定行。 */
static void test_aot_projection_rejects_missing_target_after_stripping(void) {
    SZrFunction *function = compile_source();
    SZrAotFunctionTable table;
    TZrUInt32 failures = 0u;
    memset(&table, 0, sizeof(table));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(backend_aot_build_function_table(g_state, function, &table));
    for (TZrUInt32 index = 0u; index < table.count; ++index) {
        const SZrAotFunctionEntry *entry = &table.entries[index];
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            const SZrFunctionCallSiteCacheEntry *cache = &entry->function->callSiteCaches[cacheIndex];
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            SZrAotFunctionTable emptyTable;
            if (cache->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            memset(&emptyTable, 0, sizeof(emptyTable));
            TEST_ASSERT_FALSE(backend_aot_c_project_call_binding(
                    g_state, &emptyTable, entry, cacheIndex, &row, &targetIndex, ZR_NULL));
            TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_NONE, row.contract.bindingKind);
            ++failures;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, failures);
    backend_aot_release_function_table(g_state, &table);
}

/* 任一次运行时链接失败后，所有缓存都不应保留上次链接的目标或代际。 */
static void assert_runtime_bindings_invalidated(const SZrAotFunctionTable *table) {
    for (TZrUInt32 index = 0u; index < table->count; ++index) {
        const SZrFunction *function = table->entries[index].function;
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < function->callSiteCacheLength; ++cacheIndex) {
            const SZrCallBinding *binding = &function->callSiteCaches[cacheIndex].binding;
            if (binding->contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            TEST_ASSERT_EQUAL_UINT64(0u, binding->generation);
            TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, binding->target.targetKind);
            TEST_ASSERT_NULL(binding->target.callableObject);
        }
    }
}

/* 把投影行编码为无进程指针字节，再检验加载、失败回滚及多态延迟绑定。 */
/* BUG: 分配 rows/indices/函数指针数组后任一 Unity 断言失败会 longjmp，跳过局部 free；tearDown 只释放 VM。 */
static void test_aot_runtime_links_pointer_free_rows_and_preserves_interpreter_callable(void) {
    SZrFunction *function = compile_source();
    SZrAotFunctionTable table;
    SZrObjectModule module = {0};
    SZrAotCodeRegistration registration;
    SZrMetadataRuntime *runtime;
    FZrAotEntryThunk *functionPointers;
    SZrAotMethodInfo *methodInfoStorage;
    const SZrAotMethodInfo **methodInfos;
    TZrByte *rows;
    TZrUInt32 *targetIndices;
    TZrUInt32 rowCount = 0u;
    TZrUInt32 rowIndex = 0u;
    SZrArtifactSectionView section = {0};
    SZrArtifactCallBindingRow originalRow;
    SZrArtifactCallBindingRow changedRow;
    TZrUInt32 originalTargetIndex;
    memset(&table, 0, sizeof(table));
    memset(&registration, 0, sizeof(registration));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(backend_aot_build_function_table(g_state, function, &table));
    for (TZrUInt32 index = 0u; index < table.count; ++index) {
        const SZrFunction *entryFunction = table.entries[index].function;
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entryFunction->callSiteCacheLength; ++cacheIndex) {
            if (entryFunction->callSiteCaches[cacheIndex].binding.contract.bindingKind != ZR_CALL_BINDING_NONE) {
                ++rowCount;
            }
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, rowCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, table.indexSpace);
    rows = (TZrByte *)calloc((size_t)rowCount, ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE);
    targetIndices = (TZrUInt32 *)calloc(rowCount, sizeof(*targetIndices));
    functionPointers = (FZrAotEntryThunk *)calloc(table.indexSpace, sizeof(*functionPointers));
    methodInfoStorage = (SZrAotMethodInfo *)calloc(table.indexSpace, sizeof(*methodInfoStorage));
    methodInfos = (const SZrAotMethodInfo **)calloc(table.indexSpace, sizeof(*methodInfos));
    TEST_ASSERT_NOT_NULL(rows);
    TEST_ASSERT_NOT_NULL(targetIndices);
    TEST_ASSERT_NOT_NULL(functionPointers);
    TEST_ASSERT_NOT_NULL(methodInfoStorage);
    TEST_ASSERT_NOT_NULL(methodInfos);
    for (TZrUInt32 index = 0u; index < table.indexSpace; ++index) {
        functionPointers[index] = test_runtime_aot_thunk;
        methodInfoStorage[index].functionIndex = index;
        methodInfoStorage[index].invoker = test_runtime_aot_invoker;
        methodInfos[index] = &methodInfoStorage[index];
    }
    for (TZrUInt32 index = 0u; index < table.count; ++index) {
        const SZrAotFunctionEntry *entry = &table.entries[index];
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entry->function->callSiteCacheLength; ++cacheIndex) {
            SZrArtifactCallBindingRow row;
            TZrUInt32 targetIndex;
            if (entry->function->callSiteCaches[cacheIndex].binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            TEST_ASSERT_TRUE(backend_aot_c_project_call_binding(
                    g_state, &table, entry, cacheIndex, &row, &targetIndex, ZR_NULL));
            TEST_ASSERT_TRUE(ZrCore_Artifact_WriteCallBindingRow(
                    &row, rows + (size_t)rowIndex * ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                    ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE, ZR_NULL) == ZR_ARTIFACT_STATUS_OK);
            targetIndices[rowIndex++] = targetIndex;
        }
    }
    /* 编码行与目标索引共享遍历顺序，loader 以行号配对两张表。 */
    registration.functionCount = table.indexSpace;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = table.indexSpace;
    registration.callBindingRows = rows;
    registration.callBindingRowCount = rowCount;
    registration.callBindingRowSize = ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE;
    registration.callBindingTargetFunctionIndices = targetIndices;
    runtime = ZrCore_Module_AttachMetadataRuntime(&module, function, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    for (TZrUInt32 index = 0u; index < table.count; ++index) {
        ZrCore_MetadataRuntime_AttachFunction(runtime, (SZrFunction *)table.entries[index].function);
    }
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    for (TZrUInt32 index = 0u; index < table.count; ++index) {
        SZrFunction *entryFunction = (SZrFunction *)table.entries[index].function;
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < entryFunction->callSiteCacheLength; ++cacheIndex) {
            SZrFunctionCallSiteCacheEntry *cache = &entryFunction->callSiteCaches[cacheIndex];
            if (cache->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
            TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_AOT, cache->binding.target.targetKind);
            TEST_ASSERT_TRUE(cache->binding.target.aot.thunk == test_runtime_aot_thunk);
            TEST_ASSERT_NOT_NULL(cache->binding.target.aot.methodInfo);
            TEST_ASSERT_NOT_NULL(cache->binding.target.callableObject);
        }
    }
    section.kind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE;
    section.data = rows;
    section.elementSize = ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE;
    section.elementCount = rowCount;
    section.byteLength = (TZrUInt64)rowCount * section.elementSize;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_ReadCallBindingRow(&section, 0u, &originalRow, ZR_NULL));

    /* 逐次破坏签名、模块、布局、目标和行大小；每次失败均须清除旧绑定。 */
    changedRow = originalRow;
    changedRow.contract.signatureHash ^= 1u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK, ZrCore_Artifact_WriteCallBindingRow(
            &changedRow, rows, ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_SIGNATURE_MISMATCH, g_state->lastCallBindingError.status);
    TEST_ASSERT_EQUAL_UINT64(originalRow.contract.signatureHash, g_state->lastCallBindingError.expected);
    TEST_ASSERT_EQUAL_UINT64(changedRow.contract.signatureHash, g_state->lastCallBindingError.actual);
    assert_runtime_bindings_invalidated(&table);

    changedRow = originalRow;
    changedRow.contract.moduleSignatureHash ^= 1u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK, ZrCore_Artifact_WriteCallBindingRow(
            &changedRow, rows, ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_MODULE_MISMATCH, g_state->lastCallBindingError.status);
    assert_runtime_bindings_invalidated(&table);

    changedRow = originalRow;
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, changedRow.contract.layoutHash);
    changedRow.contract.layoutHash ^= 1u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK, ZrCore_Artifact_WriteCallBindingRow(
            &changedRow, rows, ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_LAYOUT_MISMATCH, g_state->lastCallBindingError.status);
    assert_runtime_bindings_invalidated(&table);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK, ZrCore_Artifact_WriteCallBindingRow(
            &originalRow, rows, ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE, ZR_NULL));
    originalTargetIndex = targetIndices[0];
    targetIndices[0] = (originalTargetIndex + 1u) % table.indexSpace;
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_TARGET_NOT_FOUND, g_state->lastCallBindingError.status);
    assert_runtime_bindings_invalidated(&table);
    targetIndices[0] = originalTargetIndex;

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    registration.callBindingRowSize = 0u;
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_INVALID_RELOCATION, g_state->lastCallBindingError.status);
    assert_runtime_bindings_invalidated(&table);
    registration.callBindingRowSize = ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE;
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));

    /* 将多态行改为 MODULE 重定位，仅留下成员位置；接收者分派前不能提前晋升进程指针。 */
    {
        SZrMetadataRuntimeCallBindingView deferredView;
        SZrFunction *deferredFunction;
        SZrFunctionCallSiteCacheEntry *deferredEntry;
        SZrMetadataTokenRecord *deferredRecord = ZR_NULL;
        TZrUInt32 deferredRowIndex = UINT32_MAX;
        for (TZrUInt32 index = 0u; index < rowCount; ++index) {
            SZrArtifactSectionView deferredSection = {
                .kind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE,
                .elementSize = ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                .elementCount = rowCount,
                .byteLength = rowCount * ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                .data = rows};
            SZrArtifactCallBindingRow deferredRow;
            TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                    ZrCore_Artifact_ReadCallBindingRow(&deferredSection, index, &deferredRow, ZR_NULL));
            if (deferredRow.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL ||
                deferredRow.contract.bindingKind == ZR_CALL_BINDING_INTERFACE) {
                deferredRowIndex = index;
                break;
            }
        }
        TEST_ASSERT_NOT_EQUAL_UINT32(UINT32_MAX, deferredRowIndex);
        TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadCallBindingView(runtime, deferredRowIndex, &deferredView));
        deferredFunction = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                g_state, runtime->metadataFunction, deferredView.functionIndex);
        TEST_ASSERT_NOT_NULL(deferredFunction);
        TEST_ASSERT_LESS_THAN_UINT32(deferredFunction->callSiteCacheLength, deferredView.cacheIndex);
        deferredEntry = &deferredFunction->callSiteCaches[deferredView.cacheIndex];
        for (TZrUInt32 index = 0u; index < deferredFunction->metadataTokenRecordLength; ++index) {
            if (deferredFunction->metadataTokenRecords[index].token == deferredView.contract.targetMetadataToken) {
                deferredRecord = &deferredFunction->metadataTokenRecords[index];
                break;
            }
        }
        TEST_ASSERT_NOT_NULL(deferredRecord);
        deferredRecord->reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_MODULE;
        deferredRecord->ownerIndex = deferredEntry->memberEntryIndex;
        deferredEntry->bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
        deferredEntry->bindingLocation.targetIndex = deferredEntry->memberEntryIndex;
        {
            SZrArtifactSectionView deferredSection = {
                .kind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE,
                .elementSize = ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                .elementCount = rowCount,
                .byteLength = rowCount * ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                .data = rows};
            SZrArtifactCallBindingRow deferredRow;
            TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                    ZrCore_Artifact_ReadCallBindingRow(&deferredSection, deferredRowIndex, &deferredRow, ZR_NULL));
            deferredRow.location = deferredEntry->bindingLocation;
            TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                    ZrCore_Artifact_WriteCallBindingRow(
                            &deferredRow,
                            rows + (size_t)deferredRowIndex * ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                            ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE,
                            ZR_NULL));
        }
        targetIndices[deferredRowIndex] = UINT32_MAX;
        TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_LinkCallBindings(g_state, runtime));
        TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadCallBindingView(runtime, deferredRowIndex, &deferredView));
        TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_MODULE, deferredView.location.kind);
        TEST_ASSERT_EQUAL_UINT32(deferredEntry->memberEntryIndex, deferredView.location.targetIndex);
        TEST_ASSERT_EQUAL_UINT32(0u, deferredView.location.ownerDepth);
        TEST_ASSERT_EQUAL_UINT32(0u, deferredView.location.flags);
        TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, deferredView.targetFunctionIndex);
        TEST_ASSERT_NULL(deferredView.functionPointer);
        TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, deferredEntry->binding.target.targetKind);
        TEST_ASSERT_EQUAL_UINT64(deferredFunction->callBindingGeneration, deferredEntry->binding.generation);
    }
    free(methodInfos);
    free(methodInfoStorage);
    free(functionPointers);
    free(targetIndices);
    free(rows);
    backend_aot_release_function_table(g_state, &table);
}

/* C 发射物必须以固定 ABI 字节行和独立目标索引表注册，而非写入进程指针。 */
/* BUG: 读入 bytes 后断言失败会泄漏 bytes；text 分配后失败还会泄漏 text，tearDown 无法代为释放。 */
static void test_generated_c_registers_pointer_free_binding_bytes(void) {
    SZrFunction *function = compile_source();
    SZrAotWriterOptions options;
    TZrChar path[ZR_TESTS_PATH_MAX];
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0u;
    char *text;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("call_binding_aot", "projection", "main", ".c", path, sizeof(path)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    memset(&options, 0, sizeof(options));
    options.moduleName = "call_binding_aot";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(g_state, function, path, &options));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &length));
    text = (char *)malloc(length + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memcpy(text, bytes, length);
    text[length] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(text, "static const TZrByte zr_aot_call_binding_rows[] = {"));
    TEST_ASSERT_NOT_NULL(strstr(text, ".callBindingRows = zr_aot_call_binding_rows,"));
    TEST_ASSERT_NOT_NULL(strstr(text, ".callBindingRowSize = 96u,"));
    TEST_ASSERT_NOT_NULL(strstr(text, ".callBindingTargetFunctionIndices = zr_aot_call_binding_target_function_indices,"));
    free(text);
    free(bytes);
}

/* LLVM 发射物必须注册与 C 相同大小的绑定行及目标函数索引。 */
/* BUG: 读入 bytes 后断言失败会泄漏 bytes；text 分配后失败还会泄漏 text，tearDown 无法代为释放。 */
static void test_generated_llvm_registers_binding_bytes_with_current_abi(void) {
    SZrFunction *function = compile_source();
    SZrAotWriterOptions options;
    TZrChar path[ZR_TESTS_PATH_MAX];
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0u;
    char *text;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("call_binding_aot", "projection", "main", ".ll", path, sizeof(path)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    memset(&options, 0, sizeof(options));
    options.moduleName = "call_binding_aot";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(g_state, function, path, &options));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &length));
    text = (char *)malloc(length + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memcpy(text, bytes, length);
    text[length] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(text, "@zr_aot_call_binding_rows = private constant ["));
    TEST_ASSERT_NOT_NULL(strstr(text, "i32 96, ptr @zr_aot_call_binding_target_function_indices"));
    TEST_ASSERT_NOT_NULL(strstr(text, "ptr @zr_aot_code_registration,"));
    free(text);
    free(bytes);
}

/** @brief 注册 AOT 投影、重定位回滚及 C/LLVM 发射回归用例。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_projection_preserves_contract_and_resolves_index);
    RUN_TEST(test_aot_projection_rejects_missing_target_after_stripping);
    RUN_TEST(test_aot_runtime_links_pointer_free_rows_and_preserves_interpreter_callable);
    RUN_TEST(test_generated_c_registers_pointer_free_binding_bytes);
    RUN_TEST(test_generated_llvm_registers_binding_bytes_with_current_abi);
    return UNITY_END();
}
