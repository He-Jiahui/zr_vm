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
