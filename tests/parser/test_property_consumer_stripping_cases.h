#ifndef ZR_VM_TEST_PROPERTY_CONSUMER_STRIPPING_CASES_H
#define ZR_VM_TEST_PROPERTY_CONSUMER_STRIPPING_CASES_H

#define PROPERTY_CONSUMER_FUNCTION_GRAPH_LIMIT 512U

typedef struct SZrPropertyConsumerFunctionGraph {
    const SZrFunction *functions[PROPERTY_CONSUMER_FUNCTION_GRAPH_LIMIT];
    TZrUInt32 count;
} SZrPropertyConsumerFunctionGraph;

static void property_consumer_flatten_function_graph(
        const SZrFunction *function,
        SZrPropertyConsumerFunctionGraph *graph) {
    if (function == ZR_NULL || graph == ZR_NULL) {
        return;
    }
    for (TZrUInt32 index = 0U; index < graph->count; index++) {
        if (graph->functions[index] == function) {
            return;
        }
    }
    TEST_ASSERT_LESS_THAN_UINT32(
            PROPERTY_CONSUMER_FUNCTION_GRAPH_LIMIT,
            graph->count);
    graph->functions[graph->count++] = function;

    for (TZrUInt32 index = 0U;
         index < function->constantValueLength;
         index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];

        if (constant->type == ZR_VALUE_TYPE_FUNCTION &&
            constant->value.object != ZR_NULL &&
            !constant->isNative) {
            property_consumer_flatten_function_graph(
                    ZR_CAST_FUNCTION(g_state, constant->value.object),
                    graph);
        }
    }
    for (TZrUInt32 index = 0U;
         index < function->childFunctionLength;
         index++) {
        property_consumer_flatten_function_graph(
                &function->childFunctionList[index],
                graph);
    }
}

static TZrUInt32 property_consumer_function_flat_index(
        const SZrPropertyConsumerFunctionGraph *graph,
        const SZrFunction *function) {
    TEST_ASSERT_NOT_NULL(graph);
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0U; index < graph->count; index++) {
        if (graph->functions[index] == function) {
            return index;
        }
    }
    TEST_FAIL_MESSAGE("Compiled property function was absent from the AOT graph");
    return UINT32_MAX;
}

static void property_consumer_assert_body_marker(
        const TZrChar *generatedText,
        TZrUInt32 functionIndex,
        TZrBool expected) {
    TZrChar marker[128];

    snprintf(
            marker,
            sizeof(marker),
            "/* code_stripping.functionBodyBytes[%u] = ",
            (unsigned)functionIndex);
    if (expected) {
        TEST_ASSERT_NOT_NULL(strstr(generatedText, marker));
    } else {
        TEST_ASSERT_NULL(strstr(generatedText, marker));
    }
}

static void property_consumer_assert_property_root_marker(
        const TZrChar *generatedText,
        TZrUInt32 functionIndex) {
    TZrChar marker[160];

    snprintf(
            marker,
            sizeof(marker),
            "/* reachability.functionManifest.node[%u] = reason=root.property_accessor predecessor=none */",
            (unsigned)functionIndex);
    TEST_ASSERT_NOT_NULL(strstr(generatedText, marker));
}

static TZrUInt32 property_consumer_read_u32_marker(
        const TZrChar *generatedText,
        const TZrChar *name) {
    TZrChar marker[128];
    const TZrChar *valueStart;
    TZrChar *valueEnd;
    unsigned long value;

    snprintf(marker, sizeof(marker), "/* %s = ", name);
    valueStart = strstr(generatedText, marker);
    TEST_ASSERT_NOT_NULL(valueStart);
    valueStart += strlen(marker);
    value = strtoul(valueStart, &valueEnd, 10);
    TEST_ASSERT_TRUE(valueEnd != valueStart);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(UINT32_MAX, value);
    return (TZrUInt32)value;
}

static void test_aot_stripping_preserves_structured_property_dispatch_roots(void) {
    static const TZrChar generatedPath[] =
            "property_consumer_stripping.c";
    const TZrChar *source =
            "class Meter {\n"
            "  pri var stored: int = 1;\n"
            "  pub @constructor() { this.stored = 1; }\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "  pub fn unused(): int { return 99; }\n"
            "}\n"
            "var meter = new Meter();\n"
            "meter.value = 7;\n"
            "return meter.value;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_stripping.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state,
            source,
            strlen(source),
            sourceName);
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;
    const SZrFunction *getterFunction = ZR_NULL;
    const SZrFunction *setterFunction = ZR_NULL;
    const SZrFunction *unusedFunction = ZR_NULL;
    SZrPropertyConsumerFunctionGraph graph = {0};
    SZrAotWriterOptions options;
    TZrSize generatedLength = 0U;
    TZrChar *generatedText;
    TZrUInt32 getterIndex;
    TZrUInt32 setterIndex;
    TZrUInt32 unusedIndex;
    TZrUInt32 functionsBefore;
    TZrUInt32 functionsAfter;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);
    prototype = (const SZrCompiledPrototypeInfo *)(
            function->prototypeData + sizeof(TZrUInt32));
    members = (const SZrCompiledMemberInfo *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));
    for (TZrUInt32 index = 0U; index < prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &members[index];
        const SZrTypeValue *constant;

        if (member->functionConstantIndex >= function->constantValueLength) {
            continue;
        }
        constant = &function->constantValueList[member->functionConstantIndex];
        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL ||
            constant->isNative) {
            continue;
        }
        if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) {
            getterFunction = ZR_CAST_FUNCTION(g_state, constant->value.object);
        } else if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_SET) {
            setterFunction = ZR_CAST_FUNCTION(g_state, constant->value.object);
        } else if (member->memberType == ZR_AST_CLASS_METHOD &&
                   member->propertyIdentity == UINT32_MAX) {
            unusedFunction = ZR_CAST_FUNCTION(g_state, constant->value.object);
        }
    }
    TEST_ASSERT_NOT_NULL(getterFunction);
    TEST_ASSERT_NOT_NULL(setterFunction);
    TEST_ASSERT_NOT_NULL(unusedFunction);
    property_consumer_flatten_function_graph(function, &graph);
    getterIndex = property_consumer_function_flat_index(&graph, getterFunction);
    setterIndex = property_consumer_function_flat_index(&graph, setterFunction);
    unusedIndex = property_consumer_function_flat_index(&graph, unusedFunction);

    memset(&options, 0, sizeof(options));
    options.moduleName = "property_consumer_stripping";
    options.sourceHash = "property-consumer-stripping";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "property-consumer-stripping";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            g_state,
            function,
            generatedPath,
            &options));
    generatedText = ZrTests_ReadTextFile(generatedPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedText);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, generatedLength);
    functionsBefore = property_consumer_read_u32_marker(
            generatedText,
            "code_stripping.functionsBefore");
    functionsAfter = property_consumer_read_u32_marker(
            generatedText,
            "code_stripping.functionsAfter");
    TEST_ASSERT_GREATER_THAN_UINT32(functionsAfter, functionsBefore);
    property_consumer_assert_body_marker(
            generatedText,
            getterIndex,
            ZR_TRUE);
    property_consumer_assert_property_root_marker(
            generatedText,
            getterIndex);
    property_consumer_assert_body_marker(
            generatedText,
            setterIndex,
            ZR_TRUE);
    property_consumer_assert_property_root_marker(
            generatedText,
            setterIndex);
    property_consumer_assert_body_marker(
            generatedText,
            unusedIndex,
            ZR_FALSE);

    free(generatedText);
    remove(generatedPath);
    ZrCore_Function_Free(g_state, function);
}

#endif
