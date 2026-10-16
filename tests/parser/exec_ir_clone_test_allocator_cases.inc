/* Opt-in fault cases: force-include exec_ir_clone_test_allocator.h into every
 * focused core-model translation unit and link its allocator support source. */

#define ZR_EXEC_IR_CLONE_SNAPSHOT_BYTES 8192u

typedef struct SZrExecIrCloneSnapshot {
    unsigned char bytes[ZR_EXEC_IR_CLONE_SNAPSHOT_BYTES];
    size_t count;
} SZrExecIrCloneSnapshot;

static void clone_snapshot_append(SZrExecIrCloneSnapshot *snapshot,
                                  const void *storage, size_t bytes) {
    expect_true(bytes <= sizeof(snapshot->bytes) - snapshot->count,
                "clone snapshot fixture exceeds its storage");
    if (bytes != 0u) {
        expect_true(storage != ZR_NULL, "clone snapshot has missing storage");
        memcpy(snapshot->bytes + snapshot->count, storage, bytes);
        snapshot->count += bytes;
    }
}

static void clone_snapshot_function(SZrExecIrCloneSnapshot *snapshot,
                                    const SZrExecIrFunction *function) {
    clone_snapshot_append(snapshot, function, sizeof(*function));
#define ZR_CLONE_SNAPSHOT_ARRAY(field, countField) \
    clone_snapshot_append(snapshot, function->field, \
                          (size_t)function->countField * sizeof(*function->field))
    ZR_CLONE_SNAPSHOT_ARRAY(values, valueCount);
    ZR_CLONE_SNAPSHOT_ARRAY(instructions, instructionCount);
    ZR_CLONE_SNAPSHOT_ARRAY(blocks, blockCount);
    ZR_CLONE_SNAPSHOT_ARRAY(operands, operandCount);
    ZR_CLONE_SNAPSHOT_ARRAY(results, resultCount);
    ZR_CLONE_SNAPSHOT_ARRAY(memoryTokenPool, memoryTokenCount);
    ZR_CLONE_SNAPSHOT_ARRAY(phiPool, phiCount);
    ZR_CLONE_SNAPSHOT_ARRAY(phiIncoming, phiIncomingCount);
    ZR_CLONE_SNAPSHOT_ARRAY(predecessors, predecessorCount);
    ZR_CLONE_SNAPSHOT_ARRAY(successors, successorCount);
    ZR_CLONE_SNAPSHOT_ARRAY(gcRoots, gcRootCount);
    ZR_CLONE_SNAPSHOT_ARRAY(deoptStates, deoptStateCount);
    ZR_CLONE_SNAPSHOT_ARRAY(deoptValues, deoptValueCount);
    ZR_CLONE_SNAPSHOT_ARRAY(deoptAggregates, deoptAggregateCount);
    ZR_CLONE_SNAPSHOT_ARRAY(deoptAggregateFields, deoptAggregateFieldCount);
    ZR_CLONE_SNAPSHOT_ARRAY(sourceMaps, sourceMapCount);
    ZR_CLONE_SNAPSHOT_ARRAY(bindingRows, bindingRowCount);
#undef ZR_CLONE_SNAPSHOT_ARRAY
    if (function->gcMap != ZR_NULL) {
        const SZrExecIrGcMap *map = function->gcMap;
        clone_snapshot_append(snapshot, map, sizeof(*map));
        clone_snapshot_append(snapshot, map->entries,
                              (size_t)map->entryCount * sizeof(*map->entries));
        clone_snapshot_append(snapshot, map->slotIndexPool,
                              (size_t)map->slotIndexCount * sizeof(*map->slotIndexPool));
        clone_snapshot_append(snapshot, map->inlineRefOffsetPool,
                              (size_t)map->inlineRefOffsetCount * sizeof(*map->inlineRefOffsetPool));
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        clone_snapshot_append(snapshot, map, sizeof(*map));
        clone_snapshot_append(snapshot, map->entries,
                              (size_t)map->entryCount * sizeof(*map->entries));
        clone_snapshot_append(snapshot, map->valuePool,
                              (size_t)map->valueCount * sizeof(*map->valuePool));
        clone_snapshot_append(snapshot, map->rootPool,
                              (size_t)map->rootCount * sizeof(*map->rootPool));
        clone_snapshot_append(snapshot, map->ownerStatePool,
                              (size_t)map->ownerStateCount * sizeof(*map->ownerStatePool));
    }
    if (function->frameLayout != ZR_NULL) {
        const SZrExecIrFrameLayout *layout = function->frameLayout;
        clone_snapshot_append(snapshot, layout, sizeof(*layout));
        clone_snapshot_append(snapshot, layout->slots,
                              (size_t)layout->slotCount * sizeof(*layout->slots));
    }
}

static void clone_snapshot_module(SZrExecIrCloneSnapshot *snapshot,
                                  const SZrExecIrModule *module) {
    TZrUInt32 index;
    clone_snapshot_append(snapshot, module, sizeof(*module));
    clone_snapshot_append(snapshot, module->constants,
                          (size_t)module->constantCount * sizeof(*module->constants));
    clone_snapshot_append(snapshot, module->layouts,
                          (size_t)module->layoutCount * sizeof(*module->layouts));
    clone_snapshot_append(snapshot, module->sourceMaps,
                          (size_t)module->sourceMapCount * sizeof(*module->sourceMaps));
    for (index = 0u; index < module->functionCount; ++index)
        clone_snapshot_function(snapshot, &module->functions[index]);
}

static void expect_clone_snapshot(const SZrExecIrCloneSnapshot *before,
                                  const SZrExecIrCloneSnapshot *after,
                                  const char *message) {
    expect_true(before->count == after->count &&
                    memcmp(before->bytes, after->bytes, before->count) == 0,
                message);
}

static void fill_clone_fault_function(SZrExecIrFunction *function,
                                      TZrMetadataToken token) {
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = token;
    function->signatureHash = UINT64_C(0x12345678) + token;
    function->contract.moduleHash = UINT64_C(0x87654321);
    function->entryBlockId = 1u;
#define ZR_CLONE_FIXTURE_ARRAY(field, countField, capacityField) \
    do { \
        function->field = calloc(1u, sizeof(*function->field)); \
        expect_true(function->field != ZR_NULL, "clone fault fixture allocation failed"); \
        function->countField = 1u; \
        function->capacityField = 1u; \
    } while (0)
    ZR_CLONE_FIXTURE_ARRAY(values, valueCount, valueCapacity);
    ZR_CLONE_FIXTURE_ARRAY(instructions, instructionCount, instructionCapacity);
    ZR_CLONE_FIXTURE_ARRAY(blocks, blockCount, blockCapacity);
    ZR_CLONE_FIXTURE_ARRAY(operands, operandCount, operandCapacity);
    ZR_CLONE_FIXTURE_ARRAY(results, resultCount, resultCapacity);
    ZR_CLONE_FIXTURE_ARRAY(memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    ZR_CLONE_FIXTURE_ARRAY(phiPool, phiCount, phiCapacity);
    ZR_CLONE_FIXTURE_ARRAY(phiIncoming, phiIncomingCount, phiIncomingCapacity);
    ZR_CLONE_FIXTURE_ARRAY(predecessors, predecessorCount, predecessorCapacity);
    ZR_CLONE_FIXTURE_ARRAY(successors, successorCount, successorCapacity);
    ZR_CLONE_FIXTURE_ARRAY(gcRoots, gcRootCount, gcRootCapacity);
    ZR_CLONE_FIXTURE_ARRAY(deoptStates, deoptStateCount, deoptStateCapacity);
    ZR_CLONE_FIXTURE_ARRAY(deoptValues, deoptValueCount, deoptValueCapacity);
    ZR_CLONE_FIXTURE_ARRAY(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    ZR_CLONE_FIXTURE_ARRAY(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    ZR_CLONE_FIXTURE_ARRAY(sourceMaps, sourceMapCount, sourceMapCapacity);
#undef ZR_CLONE_FIXTURE_ARRAY
    function->values[0].id = 1u;
    function->instructions[0].opcode = ZR_EXEC_IR_OPCODE_CALL;
    function->instructions[0].sourceId = 17u;
    row = make_owned_binding_row(0u, 1u, 0u, function->contract.moduleHash, 17u);
    expect_true(ZrCore_ExecIr_FunctionSetBindingRows(function, &row, 1u, &diagnostic),
                "clone fault fixture binding row install failed");
    function->gcMap = calloc(1u, sizeof(*function->gcMap));
    function->stateMap = calloc(1u, sizeof(*function->stateMap));
    function->frameLayout = calloc(1u, sizeof(*function->frameLayout));
    expect_true(function->gcMap != ZR_NULL && function->stateMap != ZR_NULL &&
                    function->frameLayout != ZR_NULL,
                "clone fault fixture nested object allocation failed");
#define ZR_CLONE_FIXTURE_NESTED(object, field, countField, capacityField) \
    do { \
        function->object->field = calloc(1u, sizeof(*function->object->field)); \
        expect_true(function->object->field != ZR_NULL, "clone fault nested array allocation failed"); \
        function->object->countField = 1u; \
        function->object->capacityField = 1u; \
    } while (0)
    ZR_CLONE_FIXTURE_NESTED(gcMap, entries, entryCount, entryCapacity);
    ZR_CLONE_FIXTURE_NESTED(gcMap, slotIndexPool, slotIndexCount, slotIndexCapacity);
    ZR_CLONE_FIXTURE_NESTED(gcMap, inlineRefOffsetPool, inlineRefOffsetCount, inlineRefOffsetCapacity);
    ZR_CLONE_FIXTURE_NESTED(stateMap, entries, entryCount, entryCapacity);
    ZR_CLONE_FIXTURE_NESTED(stateMap, valuePool, valueCount, valueCapacity);
    ZR_CLONE_FIXTURE_NESTED(stateMap, rootPool, rootCount, rootCapacity);
    ZR_CLONE_FIXTURE_NESTED(stateMap, ownerStatePool, ownerStateCount, ownerStateCapacity);
    ZR_CLONE_FIXTURE_NESTED(frameLayout, slots, slotCount, slotCapacity);
#undef ZR_CLONE_FIXTURE_NESTED
    function->gcMapCount = function->gcMapCapacity = 1u;
    function->stateMap->functionToken = token;
    function->stateMap->signatureHash = function->signatureHash;
    function->stateMap->generation = 19u;
    function->stateMap->valuePool[0] = 1u;
    function->stateMap->rootPool[0] = 1u;
    function->stateMap->ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    function->frameLayout->layoutHash = UINT64_C(0x11223344);
    function->sealed = ZR_TRUE;
}

static void fill_clone_fault_module(SZrExecIrModule *module,
                                    TZrMetadataToken firstToken, TZrUInt32 functionCount) {
    TZrUInt32 index;
    ZrCore_ExecIr_ModuleInit(module);
    module->id = 2u;
    module->moduleToken = 3u;
    module->moduleHash = UINT64_C(0x98765432);
    module->constants = calloc(1u, sizeof(*module->constants));
    module->layouts = calloc(1u, sizeof(*module->layouts));
    module->sourceMaps = calloc(1u, sizeof(*module->sourceMaps));
    module->functions = calloc(functionCount, sizeof(*module->functions));
    expect_true(module->constants != ZR_NULL && module->layouts != ZR_NULL &&
                    module->sourceMaps != ZR_NULL && module->functions != ZR_NULL,
                "clone fault module fixture allocation failed");
    module->constantCount = module->constantCapacity = 1u;
    module->layoutCount = module->layoutCapacity = 1u;
    module->sourceMapCount = module->sourceMapCapacity = 1u;
    module->functionCount = module->functionCapacity = functionCount;
    for (index = 0u; index < functionCount; ++index) {
        fill_clone_fault_function(&module->functions[index], firstToken + index);
        module->functions[index].id = index + 1u;
    }
}

static void expect_clone_oom(const SZrExecIrDiagnostic *diagnostic,
                             TZrMetadataToken token, const char *api,
                             TZrUInt64 allocationIndex) {
    if (diagnostic->code != ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY ||
        diagnostic->functionToken != token || diagnostic->instructionId != 0u ||
        diagnostic->blockId != 0u) {
        fprintf(stderr, "FAIL: %s allocation %llu has diagnostic %u token %u; expected OOM token %u\n",
                api, (unsigned long long)allocationIndex, (unsigned)diagnostic->code,
                (unsigned)diagnostic->functionToken, (unsigned)token);
        exit(EXIT_FAILURE);
    }
}

static void test_function_clone_reports_allocation_failures_atomically(void) {
    SZrExecIrFunction source, destination, probe;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrCloneSnapshot sourceBefore = {{0}, 0}, destinationBefore = {{0}, 0};
    TZrUInt64 count, index, live;
    fill_clone_fault_function(&source, 901u);
    fill_clone_fault_function(&destination, 902u);
    ZrCore_ExecIr_FunctionInit(&probe);
    clone_snapshot_function(&sourceBefore, &source);
    clone_snapshot_function(&destinationBefore, &destination);
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(ZrCore_ExecIr_CloneFunction(&source, &probe, &diagnostic),
                "full function fault fixture did not clone");
    count = ZrCore_ExecIr_TestAllocatorCount();
    expect_true(count == 28u, "full function fixture missed a clone allocation");
    ZrCore_ExecIr_FreeFunction(&probe);
    live = ZrCore_ExecIr_TestAllocatorLiveCount();
    for (index = 1u; index <= count; ++index) {
        SZrExecIrCloneSnapshot sourceAfter = {{0}, 0}, destinationAfter = {{0}, 0};
        ZrCore_ExecIr_TestAllocatorFailAt(index);
        memset(&diagnostic, 0x5a, sizeof(diagnostic));
        expect_true(!ZrCore_ExecIr_CloneFunction(&source, &destination, &diagnostic),
                    "injected function allocation failure was accepted");
        expect_true(ZrCore_ExecIr_TestAllocatorCount() == index,
                    "function clone continued allocating after failure");
        expect_clone_oom(&diagnostic, source.functionToken, "CloneFunction", index);
        expect_true(ZrCore_ExecIr_TestAllocatorLiveCount() == live,
                    "function clone leaked a partial allocation");
        clone_snapshot_function(&sourceAfter, &source);
        clone_snapshot_function(&destinationAfter, &destination);
        expect_clone_snapshot(&sourceBefore, &sourceAfter, "failed function clone changed source");
        expect_clone_snapshot(&destinationBefore, &destinationAfter, "failed function clone changed destination");
        printf("CloneFunction fault %llu/%llu PASS\n", (unsigned long long)index,
               (unsigned long long)count);
    }
    ZrCore_ExecIr_TestAllocatorReset();
    ZrCore_ExecIr_FreeFunction(&destination);
    ZrCore_ExecIr_FreeFunction(&source);
}

static void test_module_clone_reports_allocation_failures_atomically(void) {
    SZrExecIrModule source, destination, probe;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrCloneSnapshot sourceBefore = {{0}, 0}, destinationBefore = {{0}, 0};
    TZrUInt64 count, index, live;
    fill_clone_fault_module(&source, 911u, 2u);
    fill_clone_fault_module(&destination, 913u, 1u);
    ZrCore_ExecIr_ModuleInit(&probe);
    clone_snapshot_module(&sourceBefore, &source);
    clone_snapshot_module(&destinationBefore, &destination);
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(ZrCore_ExecIr_CloneModule(&source, &probe, &diagnostic),
                "full module fault fixture did not clone");
    count = ZrCore_ExecIr_TestAllocatorCount();
    expect_true(count == 60u, "full module fixture missed a clone allocation");
    ZrCore_ExecIr_FreeModule(&probe);
    live = ZrCore_ExecIr_TestAllocatorLiveCount();
    for (index = 1u; index <= count; ++index) {
        SZrExecIrCloneSnapshot sourceAfter = {{0}, 0}, destinationAfter = {{0}, 0};
        TZrMetadataToken token = index <= 4u ? 0u : (index <= 32u ? 911u : 912u);
        ZrCore_ExecIr_TestAllocatorFailAt(index);
        memset(&diagnostic, 0x5a, sizeof(diagnostic));
        expect_true(!ZrCore_ExecIr_CloneModule(&source, &destination, &diagnostic),
                    "injected module allocation failure was accepted");
        expect_true(ZrCore_ExecIr_TestAllocatorCount() == index,
                    "module clone continued allocating after failure");
        expect_clone_oom(&diagnostic, token, "CloneModule", index);
        expect_true(ZrCore_ExecIr_TestAllocatorLiveCount() == live,
                    "module clone leaked a partial allocation");
        clone_snapshot_module(&sourceAfter, &source);
        clone_snapshot_module(&destinationAfter, &destination);
        expect_clone_snapshot(&sourceBefore, &sourceAfter, "failed module clone changed source");
        expect_clone_snapshot(&destinationBefore, &destinationAfter, "failed module clone changed destination");
        printf("CloneModule fault %llu/%llu PASS\n", (unsigned long long)index,
               (unsigned long long)count);
    }
    ZrCore_ExecIr_TestAllocatorReset();
    ZrCore_ExecIr_FreeModule(&destination);
    ZrCore_ExecIr_FreeModule(&source);
}

static void test_function_clone_rejects_byte_overflow(void) {
#if SIZE_MAX == UINT32_MAX
    SZrExecIrFunction source, destination;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrFunction invalidSource;
    SZrExecIrCloneSnapshot sourceBefore = {{0}, 0}, destinationBefore = {{0}, 0};
    SZrExecIrCloneSnapshot sourceAfter = {{0}, 0}, destinationAfter = {{0}, 0};
    fill_clone_fault_function(&source, 931u);
    fill_clone_fault_function(&destination, 932u);
    clone_snapshot_function(&sourceBefore, &source);
    clone_snapshot_function(&destinationBefore, &destination);
    source.valueCount = source.valueCapacity =
            (TZrUInt32)(SIZE_MAX / sizeof(*source.values)) + 1u;
    invalidSource = source;
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(!ZrCore_ExecIr_CloneFunction(&source, &destination, &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW &&
                    diagnostic.functionToken == 931u,
                "function byte overflow did not report capacity overflow");
    expect_true(ZrCore_ExecIr_TestAllocatorCount() == 0u &&
                    destination.functionToken == 932u,
                "function byte overflow allocated or changed destination");
    expect_true(memcmp(&source, &invalidSource, sizeof(source)) == 0,
                "function byte overflow changed source metadata");
    source.valueCount = source.valueCapacity = 1u;
    clone_snapshot_function(&sourceAfter, &source);
    clone_snapshot_function(&destinationAfter, &destination);
    expect_clone_snapshot(&sourceBefore, &sourceAfter, "function overflow changed source contents");
    expect_clone_snapshot(&destinationBefore, &destinationAfter, "function overflow changed destination contents");
    ZrCore_ExecIr_FreeFunction(&source);
    ZrCore_ExecIr_FreeFunction(&destination);
    puts("CloneFunction native 32-bit capacity overflow PASS");
#else
    puts("CloneFunction byte overflow case requires native 32-bit size_t");
#endif
}

static void test_module_clone_rejects_byte_overflow(void) {
#if SIZE_MAX == UINT32_MAX
    SZrExecIrModule source, destination;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrModule invalidSource;
    SZrExecIrCloneSnapshot sourceBefore = {{0}, 0}, destinationBefore = {{0}, 0};
    SZrExecIrCloneSnapshot sourceAfter = {{0}, 0}, destinationAfter = {{0}, 0};
    fill_clone_fault_module(&source, 933u, 1u);
    fill_clone_fault_module(&destination, 934u, 1u);
    clone_snapshot_module(&sourceBefore, &source);
    clone_snapshot_module(&destinationBefore, &destination);
    source.constantCount = source.constantCapacity =
            (TZrUInt32)(SIZE_MAX / sizeof(*source.constants)) + 1u;
    invalidSource = source;
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(!ZrCore_ExecIr_CloneModule(&source, &destination, &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW &&
                    diagnostic.functionToken == 0u,
                "module byte overflow did not report capacity overflow");
    expect_true(ZrCore_ExecIr_TestAllocatorCount() == 0u &&
                    destination.functions[0].functionToken == 934u,
                "module byte overflow allocated or changed destination");
    expect_true(memcmp(&source, &invalidSource, sizeof(source)) == 0,
                "module byte overflow changed source metadata");
    source.constantCount = source.constantCapacity = 1u;
    clone_snapshot_module(&sourceAfter, &source);
    clone_snapshot_module(&destinationAfter, &destination);
    expect_clone_snapshot(&sourceBefore, &sourceAfter, "module overflow changed source contents");
    expect_clone_snapshot(&destinationBefore, &destinationAfter, "module overflow changed destination contents");
    ZrCore_ExecIr_FreeModule(&source);
    ZrCore_ExecIr_FreeModule(&destination);
    puts("CloneModule native 32-bit capacity overflow PASS");
#else
    puts("CloneModule byte overflow case requires native 32-bit size_t");
#endif
}

static void test_clone_distinguishes_invalid_input_from_capacity_overflow(void) {
    SZrExecIrFunction source, destination;
    SZrExecIrModule sourceModule, destinationModule;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrConstant *constants;
    SZrExecIrFunction invalidFunction;
    SZrExecIrModule invalidModule;
    SZrExecIrCloneSnapshot functionBefore = {{0}, 0}, functionDestinationBefore = {{0}, 0};
    SZrExecIrCloneSnapshot moduleBefore = {{0}, 0}, moduleDestinationBefore = {{0}, 0};
    fill_clone_fault_function(&source, 921u);
    fill_clone_fault_function(&destination, 922u);
    fill_clone_fault_module(&sourceModule, 923u, 1u);
    fill_clone_fault_module(&destinationModule, 924u, 1u);
    clone_snapshot_function(&functionBefore, &source);
    clone_snapshot_function(&functionDestinationBefore, &destination);
    clone_snapshot_module(&moduleBefore, &sourceModule);
    clone_snapshot_module(&moduleDestinationBefore, &destinationModule);
    source.valueCapacity = 0u;
    invalidFunction = source;
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(!ZrCore_ExecIr_CloneFunction(&source, &destination, &diagnostic) &&
                    diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.functionToken == 921u,
                "malformed function clone did not report invalid argument");
    expect_true(ZrCore_ExecIr_TestAllocatorCount() == 0u &&
                    destination.functionToken == 922u,
                "malformed function clone allocated or changed destination");
    expect_true(memcmp(&source, &invalidFunction, sizeof(source)) == 0,
                "malformed function clone changed source metadata");
    source.valueCapacity = 1u;
    {
        SZrExecIrCloneSnapshot after = {{0}, 0}, destinationAfter = {{0}, 0};
        clone_snapshot_function(&after, &source);
        clone_snapshot_function(&destinationAfter, &destination);
        expect_clone_snapshot(&functionBefore, &after, "malformed function clone changed source contents");
        expect_clone_snapshot(&functionDestinationBefore, &destinationAfter, "malformed function clone changed destination contents");
    }
    constants = sourceModule.constants;
    sourceModule.constants = ZR_NULL;
    invalidModule = sourceModule;
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(!ZrCore_ExecIr_CloneModule(&sourceModule, &destinationModule, &diagnostic) &&
                    diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                "malformed module clone did not report invalid argument");
    expect_true(ZrCore_ExecIr_TestAllocatorCount() == 0u &&
                    destinationModule.functions[0].functionToken == 924u,
                "malformed module clone allocated or changed destination");
    expect_true(memcmp(&sourceModule, &invalidModule, sizeof(sourceModule)) == 0,
                "malformed module clone changed source metadata");
    sourceModule.constants = constants;
    {
        SZrExecIrCloneSnapshot after = {{0}, 0}, destinationAfter = {{0}, 0};
        clone_snapshot_module(&after, &sourceModule);
        clone_snapshot_module(&destinationAfter, &destinationModule);
        expect_clone_snapshot(&moduleBefore, &after, "malformed module clone changed source contents");
        expect_clone_snapshot(&moduleDestinationBefore, &destinationAfter, "malformed module clone changed destination contents");
    }
    ZrCore_ExecIr_TestAllocatorReset();
    ZrCore_ExecIr_FreeFunction(&source);
    ZrCore_ExecIr_FreeFunction(&destination);
    ZrCore_ExecIr_FreeModule(&sourceModule);
    ZrCore_ExecIr_FreeModule(&destinationModule);
    puts("Clone invalid-input diagnostics PASS");
    test_function_clone_rejects_byte_overflow();
    test_module_clone_rejects_byte_overflow();
}

/**
 * @def ZR_EXEC_IR_CLONE_SNAPSHOT_BYTES
 *
 * ## Summary
 * 为克隆回滚fixture保留8192字节进程内快照空间，限制记录及逻辑池内容的累计复制长度。
 *
 * ## Value and domain
 * 8192u字节，供SZrExecIrCloneSnapshot.bytes使用；不计未复制的池闲置capacity字节。
 *
 * ## Constraints and lifecycle
 * 这是固定测试缓冲容量，不是生产克隆限制或任意函数均能容纳的保证。append要求已有count合法并断言新增bytes不超过剩余量；超过容量会exit。没有动态分配、所有权或线程同步。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:4
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:7
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:13
 * - tests/parser/test_ssa_core_model.c:16
 */

/**
 * @struct SZrExecIrCloneSnapshot
 *
 * ## Summary
 * 保存同一fixture在一次失败Clone前后的原始记录和逻辑池字节，用于检验旧source及destination未被改变。
 *
 * ## Fields and invariants
 * bytes是8192字节内嵌存储，count仅表示已写有效字节数；记录地址与嵌套池所有权不转移到此结构。
 *
 * ## Constraints and lifecycle
 * count须保持0..sizeof(bytes)，调用方用{{0},0}初始化；多个append累加而不自动清空。快照包含记录内指针、counts/capacities及填充字节，但不拥有或保活原池。仅供同进程同对象前后比较，不是可移植序列化或源与成功克隆地址相等的要求；未覆盖逻辑count之外的闲置容量字节。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:6
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:11
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:24
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:93
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:211
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:250
 */

/**
 * @fn static void clone_snapshot_append(SZrExecIrCloneSnapshot *snapshot, const void *storage, size_t bytes)
 *
 * ## Summary
 * 在已有快照尾部追加一段可读原始字节，先验证有限缓冲剩余空间。
 *
 * ## Parameters
 * @param[in,out] snapshot 已初始化且count不超过bytes容量的独立快照；正常调用在尾部追加。
 * @param[in] storage bytes非零时指向至少bytes个稳定可读字节；bytes为0可为空。
 * @param[in] bytes 本次追加字节数，须不大于剩余空间。
 *
 * ## Returns
 * 无C返回值；断言成立且bytes非零时复制后推进count，bytes为0不复制也不推进；断言不成立经expect_true退出程序。
 *
 * ## Constraints and lifecycle
 * 不分配内存、不清空先前内容，也不检查输入分配来源、完整对象形状或乘法溢出。count合法是减法前提；storage与目标写入区不得违反memcpy的非重叠要求。复制期间原数据稳定，snapshot独占写；此前已追加的内容没有失败回滚协议。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:11
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:13
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:16
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:17
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:18
 * - tests/parser/test_ssa_core_model.c:16
 * - tests/parser/test_ssa_core_model.c:19
 */

/**
 * @fn static void clone_snapshot_function(SZrExecIrCloneSnapshot *snapshot, const SZrExecIrFunction *function)
 *
 * ## Summary
 * 按固定顺序追加Function记录、17组直接逻辑池以及存在的GCMap、StateMap、FrameLayout记录和各自逻辑池，为失败前后比较保存旧图。
 *
 * ## Parameters
 * @param[in,out] snapshot 合法已初始化快照，须有足够累计空间；函数不重置count。
 * @param[in] function 稳定可读的fixture函数记录；所有非空逻辑池及存在的嵌套记录必须实际可读。
 *
 * ## Returns
 * 无C返回值；完成所有append后返回；任何快照断言失败由expect_true退出程序。
 *
 * ## Constraints and lifecycle
 * 调用方须保证count字节乘法可表示，且非零逻辑池有对应存储；本helper不认证任意IR或capacity。直接池包括bindingRows，GC三池、StateMap四池和Frame slots按逻辑count复制；记录中的地址/count/capacity也入快照，闲置capacity字节不复制。没有读取后克隆、对象保活或所有权转移。独立snapshot与source存储不重叠，整个遍历源稳定。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:22
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:24
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:25
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:44
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:46
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:56
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:68
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:87
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:216
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:236
 */

/**
 * @def ZR_CLONE_SNAPSHOT_ARRAY(field, countField)
 *
 * ## Summary
 * 在clone_snapshot_function的词法环境中将一个直接池的逻辑内容追加到当前快照，避免遗漏克隆失败后的池字节变化。
 *
 * ## Parameters
 * @param[in] field function中待读池成员的字段标识符，并非运行期表达式参数。
 * @param[in] countField 同一function中与该池对应的逻辑数量字段标识符。
 *
 * ## Constraints and lifecycle
 * 函数式预处理宏使用外部变量snapshot和function；25..27定义、28..44共17处展开、45处undef。按count而非capacity计算字节，不单独检查乘法或指针可读性，继承snapshot helper的非重叠/容量前提和exit失败协议。宏自身没有独立C返回值，不拥有原池。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:25
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:26
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:27
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:28
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:44
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:45
 */

/**
 * @fn static void clone_snapshot_module(SZrExecIrCloneSnapshot *snapshot, const SZrExecIrModule *module)
 *
 * ## Summary
 * 追加Module记录、三个模块级逻辑池和每个逻辑函数的完整fixture快照，观察失败CloneModule对旧图的影响。
 *
 * ## Parameters
 * @param[in,out] snapshot 合法可追加的独立快照，调用方预留累计空间。
 * @param[in] module 稳定可读的模块记录及其逻辑constants/layouts/sourceMaps/functions；每个函数满足function快照前提。
 *
 * ## Returns
 * 无C返回值；遍历完成后返回，快照断言失败退出程序。
 *
 * ## Constraints and lifecycle
 * 按functionCount遍历而非functionCapacity，不验证任意模块形状。模块级池字节乘法及真实存储可读性由固定fixture保证；counts不能伪造后直接进入此读取路径。记录内地址与元数据入快照，闲置容量内容不入；不分配、不接管模块或嵌套函数。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:76
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:79
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:80
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:82
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:84
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:86
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:87
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:256
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:277
 */

/**
 * @fn static void expect_clone_snapshot(const SZrExecIrCloneSnapshot *before, const SZrExecIrCloneSnapshot *after, const char *message)
 *
 * ## Summary
 * 比较同一旧fixture在操作前后快照的有效长度和原始字节，报告有限的未变性断言。
 *
 * ## Parameters
 * @param[in] before 操作前合法快照，count须在其内嵌容量内。
 * @param[in] after 操作后合法快照，记录顺序须与before一致。
 * @param[in] message 比较失败时交给expect_true输出的可读NUL结尾说明。
 *
 * ## Returns
 * 无C返回值；count相等且有效字节memcmp相等时正常返回，否则输出失败并exit(EXIT_FAILURE)。
 *
 * ## Constraints and lifecycle
 * 长度不等时短路，不执行memcmp。比较包含原记录中的指针和填充字节，只适用于本进程同fixture的前后状态；不验证闲置容量、分配来源、全堆释放或成功克隆与source的独立性。无所有权转移。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:90
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:93
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:94
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:238
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:239
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:279
 * - tests/parser/test_ssa_core_model.c:16
 * - tests/parser/test_ssa_core_model.c:19
 */

/**
 * @fn static void fill_clone_fault_function(SZrExecIrFunction *function, TZrMetadataToken token)
 *
 * ## Summary
 * 构造覆盖克隆所有持有池分配路径的固定函数fixture，包含16个直接池、单行typed binding、GC/StateMap/Frame记录及其池。
 *
 * ## Parameters
 * @param[out] function 非NULL独立函数记录，须为空或已Free；FunctionInit会覆盖原字段而不回收旧所有权。
 * @param[in] token 写入functionToken与StateMap身份的场景token，并参与signatureHash构造。
 *
 * ## Returns
 * 无C返回值；所有分配和binding安装断言成立时返回一个由调用者FreeFunction的完整持有图；失败经exit结束程序。
 *
 * ## Constraints and lifecycle
 * 本fixture为克隆形状/所有权和字节回滚检查而造，不保证CFG/SSA/effect/StateMap语义可执行或已通过完整Verify。所有直接及嵌套逻辑count/capacity设1，gcMapCount亦1，sealed置TRUE但不构成同步。当前调用在注入关闭时建fixture，随后Reset隔离Clone计数；自然OOM触发expect_true而非局部失败清理。不能把它视为通用合法程序builder或把目标clone capacity误写成与source精确相等；reserve可以取整。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:98
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:102
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:108
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:115
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:130
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:135
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:136
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:138
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:144
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:158
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:160
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:168
 * - tests/parser/test_ssa_core_model.c:201
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:47
 */

/**
 * @def ZR_CLONE_FIXTURE_ARRAY(field, countField, capacityField)
 *
 * ## Summary
 * 在固定函数fixture中为指定直接池分配一个零元素记录，并发布该池count和capacity为1。
 *
 * ## Parameters
 * @param[in] field 外部function指向的直接池字段标识符。
 * @param[in] countField 该池逻辑数量字段标识符。
 * @param[in] capacityField 该池容量字段标识符。
 *
 * ## Constraints and lifecycle
 * 对象function必须已Init且本池未持有旧分配；宏为do/while语句，不是可失败返回的C接口。calloc先写指针，再expect_true非NULL，然后发布1/1；失败直接exit，不负责释放先前fixture池。16处固定展开115..130，131撤销；不包括另行安装的bindingRows，也不保证所填IR语义合法。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:108
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:110
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:111
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:112
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:113
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:115
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:130
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:131
 */

/**
 * @def ZR_CLONE_FIXTURE_NESTED(object, field, countField, capacityField)
 *
 * ## Summary
 * 为已存在的GCMap、StateMap或FrameLayout记录分配一个嵌套池元素，补齐固定克隆图的分配路径。
 *
 * ## Parameters
 * @param[in] object function中的非NULL嵌套记录字段标识符。
 * @param[in] field 嵌套记录中的池字段标识符。
 * @param[in] countField 嵌套池逻辑数量字段标识符。
 * @param[in] capacityField 嵌套池容量字段标识符。
 *
 * ## Constraints and lifecycle
 * 使用外部function，不初始化外层记录；该池须尚未持有旧内存。calloc后检查，再设count/capacity1；失败exit，不自行释放已经构造的外层及早先池。八处展开151..158覆盖GC三池、StateMap四池和Frame一池，159处undef。这里只构造存储，未认证checkpoint语义。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:144
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:146
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:147
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:148
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:149
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:151
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:158
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:159
 */

/**
 * @fn static void fill_clone_fault_module(SZrExecIrModule *module, TZrMetadataToken firstToken, TZrUInt32 functionCount)
 *
 * ## Summary
 * 构造带三个模块级池、连续函数槽和逐函数完整克隆fixture的固定模块，用于覆盖模块分配前缀及函数回滚。
 *
 * ## Parameters
 * @param[out] module 独立模块记录，须为空或已Free；ModuleInit不回收旧池。
 * @param[in] firstToken 第一个函数的测试token，后续函数按index递增，当前小fixture避免token溢出。
 * @param[in] functionCount 本场景函数数量；现有调用仅使用1或2，不承诺任意大值或0的calloc结果。
 *
 * ## Returns
 * 无C返回值；正常返回时模块及各函数由调用方FreeModule；分配或子fixture断言失败退出程序。
 *
 * ## Constraints and lifecycle
 * 先分配constants/layouts/sourceMaps/functions并检查，再发布counts/capacities和逐槽构造函数。所有权独立且须最终逐函数释放；局部构造失败没有主动回收路径。仅为Clone结构分配fixture，非可执行模块或完整语义验证保证。调用时保持注入关闭，Reset后才统计Clone本身。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:171
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:174
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:178
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:181
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:182
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:188
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:190
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:191
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:253
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:254
 */

/**
 * @fn static void expect_clone_oom(const SZrExecIrDiagnostic *diagnostic, TZrMetadataToken token, const char *api, TZrUInt64 allocationIndex)
 *
 * ## Summary
 * 验证一次预期Clone分配失败的有限诊断字段，并在不匹配时打印API与ordinal后结束程序。
 *
 * ## Parameters
 * @param[in] diagnostic 非NULL且Clone已写入的稳定诊断记录。
 * @param[in] token 本ordinal期望的诊断functionToken。
 * @param[in] api 仅供失败消息使用的NUL结尾API名称。
 * @param[in] allocationIndex 仅供失败消息打印的ordinal，不参与诊断字段比较。
 *
 * ## Returns
 * 无C返回值；code为OUT_OF_MEMORY、token匹配且instructionId/blockId均0时返回，否则fprintf并exit(EXIT_FAILURE)。
 *
 * ## Constraints and lifecycle
 * 不检查sourceId、expectedVersion或actualVersion，不清诊断、不回收fixture或重置注入。模块用例按当前固定4+28+28分配布局选择token，不是跨版本稳定API次数。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:195
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:198
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:199
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:200
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:201
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:204
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:233
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:267
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:274
 */

/**
 * @fn static void test_function_clone_reports_allocation_failures_atomically(void)
 *
 * ## Summary
 * 先以完整函数fixture成功探测CloneFunction分配数，再逐ordinal拒绝一次分配，核查有限诊断、旧图字节保持及tracker余额。
 *
 * ## Returns
 * void用例没有C返回值；全部expect断言成立后正常返回，断言不成立退出整个测试程序，未执行后续本地Reset/Free。
 *
 * ## Constraints and lifecycle
 * 在注入关闭时构造source901/destination902并记录旧图；Reset后成功probe只断言TRUE和Count==28，随后Free probe保存live基线。1..count每轮FailAt并毒化diagnostic，要求Clone FALSE、Count恰停于index、OOM/token/instruction/block字段匹配、live等于基线，以及source/destination快照完全保持。该用例不比较成功probe所有池的独立性或内容；28为当前fixture及强制TU范围的期望，不是API承诺。每轮候选回滚由生产Clone完成，尾部Reset后Free旧target/source；未证明未拦截库或记账节点的全堆余额。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:208
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:213
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:218
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:219
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:221
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:222
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:223
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:224
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:227
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:229
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:231
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:233
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:234
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:238
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:239
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:243
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:244
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:245
 * - tests/parser/test_ssa_core_model.c:909
 * - tests/parser/test_ssa_core_model.c:947
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:487
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:488
 */

/**
 * @fn static void test_module_clone_reports_allocation_failures_atomically(void)
 *
 * ## Summary
 * 用两个完整source函数和一个已有destination函数，探测CloneModule的60次分配并遍历其失败回滚与诊断归属。
 *
 * ## Returns
 * void用例正常完成全部有限断言后返回；expect失败退出进程，跳过尾部Reset/Free。
 *
 * ## Constraints and lifecycle
 * source函数token911/912，旧destination913。成功probe仅要求Clone TRUE和Count==60，Free后记录余额。每轮1..60按前4次模块池期望token0、随后28次911、最后28次912，检查FALSE、尝试停止、四诊断字段、live基线及两个模块快照不变。固定数量来自4模块池+2×28函数分配，不能扩大为所有模块/源形状或容量恰等source。尾部Reset后Free旧图；早退不承诺资源清理。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:248
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:253
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:254
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:258
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:259
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:261
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:262
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:263
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:264
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:267
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:268
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:270
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:272
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:274
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:275
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:279
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:280
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:284
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:285
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:286
 * - tests/parser/test_ssa_core_model.c:913
 * - tests/parser/test_ssa_core_model.c:948
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1119
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1137
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1140
 */

/**
 * @fn static void test_function_clone_rejects_byte_overflow(void)
 *
 * ## Summary
 * 只在原生32位size_t时伪造values逻辑数量，使字节乘法超SIZE_MAX，核查CloneFunction在分配前拒绝并保留旧fixture。
 *
 * ## Returns
 * void用例：32位分支全部断言后正常返回；其他size_t构型仅打印需要原生32位的提示后返回，不执行容量溢出断言。断言失败exit进程。
 *
 * ## Constraints and lifecycle
 * 32位分支先生成正常source931/destination932快照，再令valueCount=valueCapacity=SIZE_MAX/sizeof(value)+1，Reset后要求FALSE、CAPACITY_OVERFLOW、source token931、Count0、旧destination token932及source记录memcmp保持。恢复count/capacity1后才重新快照、比较池内容并Free，避免按伪造count读取/释放。静态首拒绝链是shape count/capacity合格后字节尺寸检查；其他size_t构型仅打印前提提示，不执行容量溢出断言。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:289
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:290
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:300
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:303
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:304
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:305
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:308
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:311
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:313
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:316
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:317
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:318
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:322
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:426
 * - tests/parser/test_ssa_core_model.c:921
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:279
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:284
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:308
 */

/**
 * @fn static void test_module_clone_rejects_byte_overflow(void)
 *
 * ## Summary
 * 只在原生32位size_t时将constants逻辑数量改为无法表示字节大小的值，验证模块级尺寸拒绝及旧图保持。
 *
 * ## Returns
 * void用例：32位断言路径完成后返回；其余构型仅打印32位前提提示，不执行溢出断言。失败经expect_true退出程序。
 *
 * ## Constraints and lifecycle
 * source token933与旧destination934先按正常逻辑快照。伪造constantCount=constantCapacity=SIZE_MAX/sizeof(constant)+1后Reset，要求Clone FALSE、CAPACITY_OVERFLOW、诊断token0、Count0、旧destination934和source记录保持；恢复1/1后再读逻辑池快照与Free。它针对模块constants首个shape尺寸检查，不代表函数内部或所有数组溢出分支已覆盖。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:326
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:327
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:337
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:340
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:341
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:342
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:343
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:345
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:348
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:350
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:353
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:354
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:355
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:359
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:427
 * - tests/parser/test_ssa_core_model.c:925
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1085
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:284
 */

/**
 * @fn static void test_clone_distinguishes_invalid_input_from_capacity_overflow(void)
 *
 * ## Summary
 * 分别以count大于capacity和非空模块constants缺失存储构造畸形source，检查INVALID_ARGUMENT、无分配和旧图保持，再调用两个条件容量用例。
 *
 * ## Returns
 * void用例正常完成两轮畸形输入及后续条件容量helper后返回；失败exit，不能保证当前本地资源已释放。
 *
 * ## Constraints and lifecycle
 * 先构造四个独立正常图并快照。函数将valueCapacity0而valueCount1，要求FALSE/INVALID_ARGUMENT/token921、Count0、旧destination922、source记录不变；恢复capacity再核源/目标快照。模块临时置constantsNULL而count/cap1，要求FALSE/INVALID_ARGUMENT、Count0、旧destination首函数924、source记录不变；本模块断言未检查diagnostic.functionToken，不能借函数路径补信用。恢复真实指针后再快照和Free。最后Reset、Free四图，再调用32位条件helper；main默认与selector入口不同，selector返回前无总live==0 guard。这些用例与helper由普通C main直接调用；expect_true失败经exit(EXIT_FAILURE)结束整个程序，expect_clone_oom的失败亦直接exit，没有Unity的setUp/tearDown或longjmp。失败出口不会先执行当前函数尾部的Reset/Free。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:363
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:372
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:380
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:382
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:383
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:384
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:385
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:387
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:390
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:392
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:397
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:400
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:401
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:403
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:404
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:405
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:407
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:410
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:412
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:417
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:420
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:421
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:424
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:426
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:427
 * - tests/parser/test_ssa_core_model.c:917
 * - tests/parser/test_ssa_core_model.c:949
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:279
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1085
 */
