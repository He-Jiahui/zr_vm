#ifndef ZR_VM_TEST_COMPILE_TIME_DECORATOR_SHAPE_RETENTION_CASES_H
#define ZR_VM_TEST_COMPILE_TIME_DECORATOR_SHAPE_RETENTION_CASES_H

static const SZrTypeValue *decorator_shape_object_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name) {
    SZrString *keyString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    keyString = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static const SZrTypeValue *decorator_shape_array_value(
        SZrState *state,
        SZrObject *array,
        TZrInt64 index) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(state, &key, index);
    return ZrCore_Object_GetValue(state, array, &key);
}

static void decorator_shape_assert_int_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        TZrInt64 expected) {
    const SZrTypeValue *value = decorator_shape_object_field(state, object, name);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    TEST_ASSERT_EQUAL_INT64(expected, value->value.nativeObject.nativeInt64);
}

static const SZrCompiledPrototypeInfo *decorator_shape_find_prototype(
        SZrState *state,
        const SZrFunction *function,
        const TZrChar *name) {
    const TZrByte *cursor;
    TZrSize remaining;

    if (state == ZR_NULL || function == ZR_NULL || name == ZR_NULL ||
        function->prototypeData == ZR_NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32)) {
        return ZR_NULL;
    }
    cursor = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 index = 0; index < function->prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *prototype;
        const SZrTypeValue *nameValue;
        TZrSize size;

        if (remaining < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_NULL;
        }
        prototype = (const SZrCompiledPrototypeInfo *)cursor;
        size = sizeof(*prototype) +
               prototype->inheritsCount * sizeof(TZrUInt32) +
               prototype->decoratorsCount * sizeof(TZrUInt32) +
               prototype->membersCount * sizeof(SZrCompiledMemberInfo);
        if (remaining < size || prototype->nameStringIndex >= function->constantValueLength) {
            return ZR_NULL;
        }
        nameValue = &function->constantValueList[prototype->nameStringIndex];
        if (nameValue->type == ZR_VALUE_TYPE_STRING && nameValue->value.object != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(
                           ZR_CAST_STRING(state, nameValue->value.object)),
                   name) == 0) {
            return prototype;
        }
        cursor += size;
        remaining -= size;
    }
    return ZR_NULL;
}

static const SZrCompiledMemberInfo *decorator_shape_find_member(
        SZrState *state,
        const SZrFunction *function,
        const SZrCompiledPrototypeInfo *prototype,
        const TZrChar *name) {
    const SZrCompiledMemberInfo *members;

    if (state == ZR_NULL || function == ZR_NULL || prototype == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    members = (const SZrCompiledMemberInfo *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));
    for (TZrUInt32 index = 0; index < prototype->membersCount; index++) {
        const SZrTypeValue *nameValue;
        if (members[index].nameStringIndex >= function->constantValueLength) {
            continue;
        }
        nameValue = &function->constantValueList[members[index].nameStringIndex];
        if (nameValue->type == ZR_VALUE_TYPE_STRING && nameValue->value.object != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(
                           ZR_CAST_STRING(state, nameValue->value.object)),
                   name) == 0) {
            return &members[index];
        }
    }
    return ZR_NULL;
}

static SZrObject *decorator_shape_metadata_object(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 constantIndex) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || function == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }
    value = &function->constantValueList[constantIndex];
    if (value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

static void decorator_shape_assert_generated_field_reflection(
        SZrState *state,
        SZrFunction *function) {
    SZrObjectModule *module;
    SZrString *moduleName;
    SZrString *fieldName;
    const SZrTypeValue *prototypeExport;
    SZrTypeValue prototypeValue;
    SZrTypeValue typeDescriptorValue;
    SZrObject *typeDescriptor;
    SZrObject *fieldDescriptor = ZR_NULL;
    const SZrTypeValue *metadataValue;
    SZrObject *metadata;
    SZrReflectionMemberQuery query;
    EZrReflectionQueryStatus status;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);
    moduleName = ZrCore_String_CreateFromNative(
            state, "generated_artifact_reflection");
    TEST_ASSERT_NOT_NULL(moduleName);
    ZrCore_Module_SetInfo(
            state,
            module,
            moduleName,
            ZrCore_Module_CalculatePathHash(state, moduleName),
            moduleName);
    TEST_ASSERT_EQUAL_UINT64(
            1U,
            ZrCore_Module_CreatePrototypesFromData(state, module, function));

    prototypeExport = ZrCore_Module_GetPubExport(
            state,
            module,
            ZrCore_String_CreateFromNative(state, "Meter"));
    TEST_ASSERT_NOT_NULL(prototypeExport);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, prototypeExport->type);
    ZrCore_Value_ResetAsNull(&prototypeValue);
    ZrCore_Value_Copy(state, &prototypeValue, prototypeExport);
    ZrCore_Value_ResetAsNull(&typeDescriptorValue);
    TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
            state, &prototypeValue, &typeDescriptorValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, typeDescriptorValue.type);
    typeDescriptor = ZR_CAST_OBJECT(state, typeDescriptorValue.value.object);
    TEST_ASSERT_NOT_NULL(typeDescriptor);

    fieldName = ZrCore_String_CreateFromNative(state, "generated");
    ZrCore_Reflection_MemberQueryInitDefault(&query);
    TEST_ASSERT_TRUE(ZrCore_Reflection_GetMember(
            state,
            typeDescriptor,
            fieldName,
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            ZR_NULL,
            0U,
            &query,
            &fieldDescriptor,
            &status));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_OK, status);
    TEST_ASSERT_NOT_NULL(fieldDescriptor);
    metadataValue = decorator_shape_object_field(
            state, fieldDescriptor, "metadata");
    TEST_ASSERT_NOT_NULL(metadataValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, metadataValue->type);
    metadata = ZR_CAST_OBJECT(state, metadataValue->value.object);
    TEST_ASSERT_NOT_NULL(metadata);
    decorator_shape_assert_int_field(state, metadata, "generated", 1);
    decorator_shape_assert_int_field(state, metadata, "sourceLineStart", 11);
    decorator_shape_assert_int_field(state, metadata, "sourceLineEnd", 11);
}

static void test_union_interface_parameter_decorator_metadata_is_retained(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.parameter, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct ParamLabel { pub let value: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn markType(target: declaration.TypeView): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn markField(target: declaration.Field): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn markParameter(target: declaration.Parameter): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#markType#\n"
            "union Choice {\n"
            "    #markField# Some(#markParameter# #ParamLabel(value: 5)# value: int);\n"
            "}\n"
            "interface Service {\n"
            "    fn run(#markParameter# #ParamLabel(value: 7)# value: int): int;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;
    const SZrCompiledPrototypeInfo *choice;
    const SZrCompiledPrototypeInfo *service;
    const SZrCompiledMemberInfo *some;
    const SZrCompiledMemberInfo *run;
    SZrObject *someMetadata;
    SZrObject *runMetadata;
    const SZrTypeValue *payloadFieldsValue;
    const SZrTypeValue *parametersValue;
    const SZrTypeValue *parameterValue;
    SZrObject *parameterMetadata;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "decorator_shape_retention.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    choice = decorator_shape_find_prototype(state, function, "Choice");
    service = decorator_shape_find_prototype(state, function, "Service");
    TEST_ASSERT_NOT_NULL(choice);
    TEST_ASSERT_NOT_NULL(service);
    TEST_ASSERT_EQUAL_UINT32(1, choice->decoratorsCount);

    some = decorator_shape_find_member(state, function, choice, "Some");
    run = decorator_shape_find_member(state, function, service, "run");
    TEST_ASSERT_NOT_NULL(some);
    TEST_ASSERT_NOT_NULL(run);
    TEST_ASSERT_TRUE(some->hasDecoratorMetadata);
    TEST_ASSERT_TRUE(run->hasDecoratorMetadata);
    someMetadata = decorator_shape_metadata_object(
            state, function, some->decoratorMetadataConstantIndex);
    runMetadata = decorator_shape_metadata_object(
            state, function, run->decoratorMetadataConstantIndex);
    TEST_ASSERT_NOT_NULL(someMetadata);
    TEST_ASSERT_NOT_NULL(runMetadata);
    TEST_ASSERT_TRUE(some->hasDecoratorNames);

    payloadFieldsValue = decorator_shape_object_field(state, someMetadata, "payloadFields");
    TEST_ASSERT_NOT_NULL(payloadFieldsValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, payloadFieldsValue->type);
    parameterValue = decorator_shape_array_value(
            state, ZR_CAST_OBJECT(state, payloadFieldsValue->value.object), 0);
    TEST_ASSERT_NOT_NULL(parameterValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, parameterValue->type);
    parameterMetadata = ZR_CAST_OBJECT(state, parameterValue->value.object);
    decorator_shape_assert_int_field(state, parameterMetadata, "decoratorCount", 2);
    parameterValue = decorator_shape_object_field(state, parameterMetadata, "metadata");
    TEST_ASSERT_NOT_NULL(parameterValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, parameterValue->type);

    parametersValue = decorator_shape_object_field(state, runMetadata, "parameters");
    TEST_ASSERT_NOT_NULL(parametersValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, parametersValue->type);
    parameterValue = decorator_shape_array_value(
            state, ZR_CAST_OBJECT(state, parametersValue->value.object), 0);
    TEST_ASSERT_NOT_NULL(parameterValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, parameterValue->type);
    parameterMetadata = ZR_CAST_OBJECT(state, parameterValue->value.object);
    decorator_shape_assert_int_field(state, parameterMetadata, "decoratorCount", 2);
    parameterValue = decorator_shape_object_field(state, parameterMetadata, "metadata");
    TEST_ASSERT_NOT_NULL(parameterValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, parameterValue->type);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_ordinary_enum_static_and_dynamic_decorators_compose(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.field, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct EnumLabel { pub let value: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn markField(target: declaration.Field): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "enum Mode: i32 {\n"
            "    #markField# #EnumLabel(value: 9)# Active = 7;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;
    const SZrCompiledPrototypeInfo *mode;
    const SZrCompiledMemberInfo *active;
    SZrObject *metadata;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "enum_decorator_composition.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    mode = decorator_shape_find_prototype(state, function, "Mode");
    TEST_ASSERT_NOT_NULL(mode);
    active = decorator_shape_find_member(state, function, mode, "Active");
    TEST_ASSERT_NOT_NULL(active);
    TEST_ASSERT_TRUE(active->hasDecoratorMetadata);
    TEST_ASSERT_TRUE(active->hasDecoratorNames);
    metadata = decorator_shape_metadata_object(
            state, function, active->decoratorMetadataConstantIndex);
    TEST_ASSERT_NOT_NULL(metadata);
    TEST_ASSERT_TRUE(active->hasDecoratorNames);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_generated_field_retains_transform_source_provenance(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn derive(target: declaration.Struct): declaration.Patch {\n"
            "    let field = init declaration.GeneratedField(\n"
            "        name: \"generated\", type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId, additions: [field]);\n"
            "}\n"
            "#derive#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;
    const SZrCompiledPrototypeInfo *meter;
    const SZrCompiledMemberInfo *generated;
    SZrObject *metadata;
    const SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "generated_field_provenance.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    meter = decorator_shape_find_prototype(state, function, "Meter");
    TEST_ASSERT_NOT_NULL(meter);
    generated = decorator_shape_find_member(state, function, meter, "generated");
    TEST_ASSERT_NOT_NULL(generated);
    TEST_ASSERT_TRUE(generated->hasDecoratorMetadata);
    metadata = decorator_shape_metadata_object(
            state, function, generated->decoratorMetadataConstantIndex);
    TEST_ASSERT_NOT_NULL(metadata);
    decorator_shape_assert_int_field(state, metadata, "generated", 1);
    decorator_shape_assert_int_field(state, metadata, "sourceLineStart", 11);
    decorator_shape_assert_int_field(state, metadata, "sourceLineEnd", 11);
    value = decorator_shape_object_field(state, metadata, "originTargetSymbolId");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    TEST_ASSERT_NOT_EQUAL(0, value->value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_generated_field_metadata_roundtrips_to_artifact_and_reflection(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn derive(target: declaration.Struct): declaration.Patch {\n"
            "    let field = init declaration.GeneratedField(\n"
            "        name: \"generated\", type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId, additions: [field]);\n"
            "}\n"
            "#derive#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    const TZrChar *binaryPath = "generated_field_metadata_roundtrip.zro";
    const TZrChar *intermediatePath = "generated_field_metadata_roundtrip.zri";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *sourceFunction;
    SZrFunction *runtimeFunction;
    const SZrCompiledPrototypeInfo *meter;
    const SZrCompiledMemberInfo *generated;
    SZrObject *metadata;
    TZrByte *binaryBytes;
    TZrSize binaryLength = 0U;
    TZrByte *intermediateBytes;
    TZrSize intermediateLength = 0U;
    TZrChar *intermediateText;
    SZrCompileTimeImportReader *reader;
    SZrIo binaryIo;
    SZrIoSource *binarySource;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "generated_field_metadata_roundtrip.zr");
    sourceFunction = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    decorator_shape_assert_generated_field_reflection(state, sourceFunction);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            state, sourceFunction, binaryPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(
            state, sourceFunction, intermediatePath));

    intermediateBytes = ZrTests_Fixture_ReadFileBytes(
            intermediatePath, &intermediateLength);
    TEST_ASSERT_NOT_NULL(intermediateBytes);
    TEST_ASSERT_TRUE(intermediateLength > 0U);
    intermediateText = (TZrChar *)malloc(intermediateLength + 1U);
    TEST_ASSERT_NOT_NULL(intermediateText);
    memcpy(intermediateText, intermediateBytes, intermediateLength);
    intermediateText[intermediateLength] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GENERATED_SOURCE_MAPS (1):"));
    TEST_ASSERT_NOT_NULL(strstr(
            intermediateText,
            "type=Meter member=generated originTargetSymbolId="));
    TEST_ASSERT_NOT_NULL(strstr(intermediateText, "sourceLineStart=11 sourceLineEnd=11"));
    free(intermediateText);
    free(intermediateBytes);

    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0U);
    reader = (SZrCompileTimeImportReader *)malloc(sizeof(*reader));
    TEST_ASSERT_NOT_NULL(reader);
    reader->bytes = binaryBytes;
    reader->length = binaryLength;
    reader->consumed = ZR_FALSE;
    ZrCore_Io_Init(
            state,
            &binaryIo,
            compile_time_import_reader_read,
            compile_time_import_reader_close,
            reader);
    binaryIo.isBinary = ZR_TRUE;
    binarySource = ZrCore_Io_ReadSourceNew(&binaryIo);
    if (binaryIo.close != ZR_NULL) {
        binaryIo.close(state, binaryIo.customData);
    }
    TEST_ASSERT_NOT_NULL(binarySource);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, binarySource);
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    meter = decorator_shape_find_prototype(state, runtimeFunction, "Meter");
    TEST_ASSERT_NOT_NULL(meter);
    generated = decorator_shape_find_member(
            state, runtimeFunction, meter, "generated");
    TEST_ASSERT_NOT_NULL(generated);
    TEST_ASSERT_TRUE(generated->hasDecoratorMetadata);
    metadata = decorator_shape_metadata_object(
            state,
            runtimeFunction,
            generated->decoratorMetadataConstantIndex);
    TEST_ASSERT_NOT_NULL(metadata);
    decorator_shape_assert_int_field(state, metadata, "generated", 1);
    decorator_shape_assert_int_field(state, metadata, "sourceLineStart", 11);
    decorator_shape_assert_int_field(state, metadata, "sourceLineEnd", 11);
    TEST_ASSERT_NOT_NULL(decorator_shape_object_field(
            state, metadata, "originTargetSymbolId"));
    decorator_shape_assert_generated_field_reflection(state, runtimeFunction);

    ZrCore_Function_Free(state, runtimeFunction);
    ZrCore_Function_Free(state, sourceFunction);
    free(binaryBytes);
    remove(binaryPath);
    remove(intermediatePath);
    destroy_test_state(state);
}

static TZrChar *decorator_shape_read_text_file(
        const TZrChar *path,
        TZrSize *outLength) {
    TZrByte *bytes;
    TZrSize length = 0U;
    TZrChar *text;

    bytes = ZrTests_Fixture_ReadFileBytes(path, &length);
    if (bytes == ZR_NULL) {
        return ZR_NULL;
    }
    text = (TZrChar *)malloc(length + 1U);
    if (text == ZR_NULL) {
        free(bytes);
        return ZR_NULL;
    }
    memcpy(text, bytes, length);
    text[length] = '\0';
    free(bytes);
    if (outLength != ZR_NULL) {
        *outLength = length;
    }
    return text;
}

static TZrBool decorator_shape_file_exists(const TZrChar *path) {
    FILE *file = path != ZR_NULL ? fopen(path, "rb") : ZR_NULL;

    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    fclose(file);
    return ZR_TRUE;
}

static void test_intermediate_omits_empty_generated_source_map_section(void) {
    static const TZrChar *source =
            "pub struct Plain { pub let value: int; }\n"
            "return 0;\n";
    const TZrChar *path = "ordinary_without_generated_source_map.zri";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;
    TZrChar *text;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "ordinary_without_generated_source_map.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(
            state, function, path));
    text = decorator_shape_read_text_file(path, ZR_NULL);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "GENERATED_SOURCE_MAPS"));

    free(text);
    remove(path);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_generated_source_maps_are_ordered_and_byte_stable(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn derive(target: declaration.Struct): declaration.Patch {\n"
            "    let first = init declaration.GeneratedField(\n"
            "        name: \"first\", type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    let second = init declaration.GeneratedField(\n"
            "        name: \"second\", type: typeid(int),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId, additions: [first, second]);\n"
            "}\n"
            "#derive#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    const TZrChar *firstPath = "generated_source_maps_first.zri";
    const TZrChar *secondPath = "generated_source_maps_second.zri";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;
    TZrChar *firstText;
    TZrChar *secondText;
    TZrSize firstLength = 0U;
    TZrSize secondLength = 0U;
    const TZrChar *firstRow;
    const TZrChar *secondRow;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "generated_source_maps_ordered.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(
            state, function, firstPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(
            state, function, secondPath));
    firstText = decorator_shape_read_text_file(firstPath, &firstLength);
    secondText = decorator_shape_read_text_file(secondPath, &secondLength);
    TEST_ASSERT_NOT_NULL(firstText);
    TEST_ASSERT_NOT_NULL(secondText);
    TEST_ASSERT_EQUAL_UINT64(firstLength, secondLength);
    TEST_ASSERT_EQUAL_MEMORY(firstText, secondText, firstLength);
    TEST_ASSERT_NOT_NULL(strstr(firstText, "GENERATED_SOURCE_MAPS (2):"));
    firstRow = strstr(firstText, "type=Meter member=first ");
    secondRow = strstr(firstText, "type=Meter member=second ");
    TEST_ASSERT_NOT_NULL(firstRow);
    TEST_ASSERT_NOT_NULL(secondRow);
    TEST_ASSERT_TRUE(firstRow < secondRow);

    free(secondText);
    free(firstText);
    remove(secondPath);
    remove(firstPath);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_intermediate_rejects_malformed_prototype_data_before_write(void) {
    static const TZrChar *source =
            "pub struct Safe { pub let value: int; }\n"
            "return 0;\n";
    const TZrChar *path = "malformed_prototype_data.zri";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;
    TZrByte *originalData;
    TZrUInt32 originalLength;
    TZrUInt32 originalCount;
    TZrByte *overflowData;
    TZrByte *truncatedData;
    SZrCompiledPrototypeInfo *overflowPrototype;
    TZrUInt32 encodedCount = 1U;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "malformed_prototype_data.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    originalData = function->prototypeData;
    originalLength = function->prototypeDataLength;
    originalCount = function->prototypeCount;

    overflowData = (TZrByte *)calloc(
            1U, sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo));
    TEST_ASSERT_NOT_NULL(overflowData);
    memcpy(overflowData, &encodedCount, sizeof(encodedCount));
    overflowPrototype = (SZrCompiledPrototypeInfo *)(
            overflowData + sizeof(TZrUInt32));
    overflowPrototype->inheritsCount = UINT32_MAX;
    overflowPrototype->decoratorsCount = UINT32_MAX;
    overflowPrototype->membersCount = UINT32_MAX;
    function->prototypeData = overflowData;
    function->prototypeDataLength =
            sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo);
    function->prototypeCount = 1U;
    remove(path);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteIntermediateFile(
            state, function, path));
    TEST_ASSERT_FALSE(decorator_shape_file_exists(path));
    free(overflowData);

    truncatedData = (TZrByte *)malloc(3U);
    TEST_ASSERT_NOT_NULL(truncatedData);
    memset(truncatedData, 0, 3U);
    function->prototypeData = truncatedData;
    function->prototypeDataLength = 3U;
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteIntermediateFile(
            state, function, path));
    TEST_ASSERT_FALSE(decorator_shape_file_exists(path));
    free(truncatedData);

    function->prototypeData = originalData;
    function->prototypeDataLength = originalLength;
    function->prototypeCount = originalCount;
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

#endif
