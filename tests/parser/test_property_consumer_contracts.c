#include "unity.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/module_fixture_support.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/writer.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;
static const ZrTestsFixtureSource *g_property_consumer_import_fixtures;
static TZrSize g_property_consumer_import_fixture_count;

#define PROPERTY_MIGRATION_CAPTURE_LIMIT 8U
#define PROPERTY_MIGRATION_TEXT_LIMIT 1024U

typedef struct SZrPropertyMigrationDiagnosticCapture {
    TZrUInt32 count;
    EZrStructuredDiagnosticSeverity severity[PROPERTY_MIGRATION_CAPTURE_LIMIT];
    SZrFileRange location[PROPERTY_MIGRATION_CAPTURE_LIMIT];
    TZrUInt32 relatedCount[PROPERTY_MIGRATION_CAPTURE_LIMIT];
    TZrUInt32 fixCount[PROPERTY_MIGRATION_CAPTURE_LIMIT];
    SZrFileRange editRange[PROPERTY_MIGRATION_CAPTURE_LIMIT];
    EZrDiagnosticFixApplicability applicability[PROPERTY_MIGRATION_CAPTURE_LIMIT];
    TZrChar editText[PROPERTY_MIGRATION_CAPTURE_LIMIT]
                     [PROPERTY_MIGRATION_TEXT_LIMIT];
} SZrPropertyMigrationDiagnosticCapture;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void property_consumer_release_compiler_function(
        SZrCompilerState *cs) {
    if (cs->topLevelFunction != ZR_NULL &&
        cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }
    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static void property_consumer_capture_migration_diagnostic(
        TZrPtr userData,
        const SZrStructuredDiagnostic *diagnostic,
        EZrToken token) {
    SZrPropertyMigrationDiagnosticCapture *capture =
            (SZrPropertyMigrationDiagnosticCapture *)userData;
    const TZrChar *code;
    TZrUInt32 index;

    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL || diagnostic == ZR_NULL ||
        diagnostic->code == ZR_NULL) {
        return;
    }
    code = ZrCore_String_GetNativeString(diagnostic->code);
    if (code == ZR_NULL || strcmp(code, "legacy_property_syntax") != 0 ||
        capture->count >= PROPERTY_MIGRATION_CAPTURE_LIMIT) {
        return;
    }

    index = capture->count++;
    capture->severity[index] = diagnostic->severity;
    capture->location[index] = diagnostic->location;
    capture->relatedCount[index] = (TZrUInt32)diagnostic->relatedInformation.length;
    capture->fixCount[index] = (TZrUInt32)diagnostic->fixes.length;
    if (diagnostic->fixes.length == 1U) {
        const SZrStructuredDiagnosticFix *fix =
                (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
                        (SZrArray *)&diagnostic->fixes,
                        0U);
        const TZrChar *editText =
                fix != ZR_NULL && fix->editText != ZR_NULL
                        ? ZrCore_String_GetNativeString(fix->editText)
                        : ZR_NULL;

        if (fix != ZR_NULL) {
            capture->editRange[index] = fix->editRange;
            capture->applicability[index] = fix->applicability;
        }
        if (editText != ZR_NULL) {
            snprintf(
                    capture->editText[index],
                    PROPERTY_MIGRATION_TEXT_LIMIT,
                    "%s",
                    editText);
        }
    }
}

static SZrAstNode *property_consumer_parse_with_migration_capture(
        const TZrChar *source,
        const TZrChar *sourceNameText,
        SZrPropertyMigrationDiagnosticCapture *capture,
        SZrParserState *outParserState) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_NOT_NULL(outParserState);
    memset(capture, 0, sizeof(*capture));
    sourceName = ZrCore_String_CreateFromNative(
            g_state,
            (TZrNativeString)sourceNameText);
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(
            outParserState,
            g_state,
            source,
            strlen(source),
            sourceName);
    outParserState->structuredErrorCallback =
            property_consumer_capture_migration_diagnostic;
    outParserState->errorUserData = capture;
    outParserState->suppressErrorOutput = ZR_TRUE;
    outParserState->enableLegacyMigrationParsing = ZR_TRUE;
    return ZrParser_ParseWithState(outParserState);
}

static void property_consumer_assert_replacement_parses(
        const TZrChar *replacement,
        const TZrChar *containerPrefix,
        const TZrChar *containerSuffix) {
    TZrSize sourceLength;
    TZrChar *source;
    SZrString *sourceName;
    SZrParserState parserState;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(replacement);
    sourceLength = strlen(containerPrefix) + strlen(replacement) +
                   strlen(containerSuffix);
    source = (TZrChar *)malloc(sourceLength + 1U);
    TEST_ASSERT_NOT_NULL(source);
    snprintf(
            source,
            sourceLength + 1U,
            "%s%s%s",
            containerPrefix,
            replacement,
            containerSuffix);
    sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_migration_replacement.zr");
    ZrParser_State_Init(
            &parserState,
            g_state,
            source,
            sourceLength,
            sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(parserState.hasError, parserState.errorMessage);
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parserState);
    free(source);
}

static void property_consumer_reader_close_noop(
        SZrState *state,
        TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *property_consumer_load_binary_entry(
        const TZrChar *path) {
    TZrSize byteLength = 0u;
    TZrByte *bytes = ZrTests_Fixture_ReadFileBytes(path, &byteLength);
    ZrTestsFixtureReader reader;
    SZrIo *io;
    SZrIoSource *source;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)byteLength);
    reader.bytes = bytes;
    reader.length = byteLength;
    reader.consumed = ZR_FALSE;
    io = ZrCore_Io_New(g_state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(
            g_state,
            io,
            ZrTests_Fixture_ReaderRead,
            property_consumer_reader_close_noop,
            &reader);
    io->isBinary = ZR_TRUE;
    source = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(source);
    function = ZrCore_Io_LoadEntryFunctionToRuntime(g_state, source);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Io_Free(g_state->global, io);
    free(bytes);
    return function;
}

#include "test_property_consumer_runtime_bootstrap_cases.h"

static TZrBool property_consumer_import_source_loader(
        SZrState *state,
        TZrNativeString sourcePath,
        TZrNativeString md5,
        SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(
            state,
            sourcePath,
            md5,
            io,
            g_property_consumer_import_fixtures,
            g_property_consumer_import_fixture_count);
}

#include "test_property_consumer_stripping_cases.h"

static void test_property_query_public_shape_is_available(void) {
    SZrParserSemanticPropertyQuery query;

    memset(&query, 0x5a, sizeof(query));

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PropertyBySymbolId(
            ZR_NULL,
            ZR_SEMANTIC_ID_INVALID,
            &query));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.propertySymbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.propertyTypeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.getterSymbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.setterSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.initializerSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.setterValueSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.initializerValueSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.getterCallableTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.setterCallableTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            query.initializerCallableTypeId);
}

static void test_property_query_roundtrips_compiler_property_identity(void) {
    const TZrChar *source =
            "class Meter {\n"
            "  pub property value: int {\n"
            "    get { return 7; }\n"
            "    pri set { }\n"
            "  }\n"
            "  pub static property shared: int {\n"
            "    get { return 9; }\n"
            "  }\n"
            "}\n"
            "fn read(meter: Meter): int { return meter.value; }\n"
            "fn readStatic(): int { return Meter.shared; }\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_query.zr");
    SZrAstNode *ast = ZrParser_Parse(
            g_state,
            source,
            strlen(source),
            sourceName);
    SZrAstNode *classNode;
    SZrAstNode *propertyNode;
    SZrCompilerState cs;
    SZrParserSemanticPropertyQuery atQuery;
    SZrParserSemanticPropertyQuery idQuery;
    SZrParserSemanticPropertyQuery usageQuery;
    SZrParserSemanticPropertyQuery staticUsageQuery;
    const TZrChar *usageText;
    TZrSize usageOffset;
    SZrFilePosition usagePosition;
    SZrFileRange usageRange;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            (TZrUInt32)ast->data.script.statements->count);
    classNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(classNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            2U,
            (TZrUInt32)classNode->data.classDeclaration.members->count);
    propertyNode = classNode->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(propertyNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_DECLARATION, propertyNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyAt(
            cs.semanticContext,
            propertyNode->data.propertyDeclaration.nameLocation,
            ZR_NULL,
            &atQuery));
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.propertySymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.propertyTypeId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.getterSymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.setterSymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.setterValueSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.initializerValueSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.initializerSymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.getterCallableTypeId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            atQuery.setterCallableTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, atQuery.access);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, atQuery.getterAccess);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PRIVATE, atQuery.setterAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY,
            atQuery.receiverEffect);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_NONE,
            atQuery.referenceAccess);
    TEST_ASSERT_FALSE(atQuery.exportsWritableRef);
    TEST_ASSERT_FALSE(atQuery.isStatic);
    TEST_ASSERT_EQUAL_UINT64(
            propertyNode->location.start.offset,
            atQuery.declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            propertyNode->data.propertyDeclaration.nameLocation.start.offset,
            atQuery.selectionRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyBySymbolId(
            cs.semanticContext,
            atQuery.getterSymbolId,
            &idQuery));
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.propertySymbolId,
            idQuery.propertySymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.propertyTypeId,
            idQuery.propertyTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.setterCallableTypeId,
            idQuery.setterCallableTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyBySymbolId(
            cs.semanticContext,
            atQuery.setterValueSymbolId,
            &idQuery));
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.propertySymbolId,
            idQuery.propertySymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.propertyTypeId,
            idQuery.propertyTypeId);

    usageText = strstr(source, "meter.value");
    TEST_ASSERT_NOT_NULL(usageText);
    usageOffset = (TZrSize)(usageText - source) + strlen("meter.");
    usagePosition = ZrParser_FilePosition_Create(usageOffset, 0, 0);
    usageRange = ZrParser_FileRange_Create(
            usagePosition,
            usagePosition,
            sourceName);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyAt(
            cs.semanticContext,
            usageRange,
            ZR_NULL,
            &usageQuery));
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.propertySymbolId,
            usageQuery.propertySymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            atQuery.propertyTypeId,
            usageQuery.propertyTypeId);

    usageText = strstr(source, "Meter.shared");
    TEST_ASSERT_NOT_NULL(usageText);
    usageOffset = (TZrSize)(usageText - source) + strlen("Meter.");
    usagePosition = ZrParser_FilePosition_Create(usageOffset, 0, 0);
    usageRange = ZrParser_FileRange_Create(
            usagePosition,
            usagePosition,
            sourceName);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyAt(
            cs.semanticContext,
            usageRange,
            ZR_NULL,
            &staticUsageQuery));
    TEST_ASSERT_NOT_EQUAL(
            atQuery.propertySymbolId,
            staticUsageQuery.propertySymbolId);
    TEST_ASSERT_TRUE(staticUsageQuery.isStatic);

    ZrParser_SemanticContext_Reset(cs.semanticContext);
    memset(&idQuery, 0x5a, sizeof(idQuery));
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PropertyBySymbolId(
            cs.semanticContext,
            atQuery.propertySymbolId,
            &idQuery));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            idQuery.propertySymbolId);

    property_consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_many_properties_publish_unique_canonical_contracts(void) {
    enum { PROPERTY_COUNT = 128, SOURCE_CAPACITY = 32768 };
    TZrChar *source = (TZrChar *)malloc(SOURCE_CAPACITY);
    TZrSize sourceLength = 0U;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    TEST_ASSERT_NOT_NULL(source);
    sourceLength += (TZrSize)snprintf(
            source + sourceLength,
            SOURCE_CAPACITY - sourceLength,
            "class PropertyStress {\n");
    for (TZrUInt32 index = 0U; index < PROPERTY_COUNT; index++) {
        TZrInt32 written = snprintf(
                source + sourceLength,
                SOURCE_CAPACITY - sourceLength,
                "  pub property p%03u: int { get { return %u; } set { } }\n",
                index,
                index);
        TEST_ASSERT_GREATER_THAN_INT(0, written);
        TEST_ASSERT_LESS_THAN_UINT64(
                SOURCE_CAPACITY - sourceLength,
                (TZrSize)written);
        sourceLength += (TZrSize)written;
    }
    sourceLength += (TZrSize)snprintf(
            source + sourceLength,
            SOURCE_CAPACITY - sourceLength,
            "}\nreturn 0;\n");
    TEST_ASSERT_LESS_THAN_UINT64(SOURCE_CAPACITY, sourceLength);

    sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_stress.zr");
    ast = ZrParser_Parse(
            g_state,
            source,
            sourceLength,
            sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_UINT32(
            PROPERTY_COUNT,
            (TZrUInt32)cs.semanticContext->propertyContracts.length);
    for (TZrSize first = 0U;
         first < cs.semanticContext->propertyContracts.length;
         first++) {
        const SZrSemanticPropertyContract *firstContract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        &cs.semanticContext->propertyContracts,
                        first);
        TEST_ASSERT_NOT_NULL(firstContract);
        TEST_ASSERT_NOT_EQUAL(
                ZR_SEMANTIC_ID_INVALID,
                firstContract->propertySymbolId);
        TEST_ASSERT_NOT_EQUAL(
                ZR_SEMANTIC_ID_INVALID,
                firstContract->getterSymbolId);
        TEST_ASSERT_NOT_EQUAL(
                ZR_SEMANTIC_ID_INVALID,
                firstContract->setterSymbolId);
        for (TZrSize second = first + 1U;
             second < cs.semanticContext->propertyContracts.length;
             second++) {
            const SZrSemanticPropertyContract *secondContract =
                    (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                            &cs.semanticContext->propertyContracts,
                            second);
            TEST_ASSERT_NOT_NULL(secondContract);
            TEST_ASSERT_NOT_EQUAL(
                    firstContract->propertySymbolId,
                    secondContract->propertySymbolId);
        }
    }

    property_consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
    free(source);
}

static void test_property_def_row_preserves_initializer_and_visible_name(void) {
    SZrArtifactPropertyDefRow row;

    memset(&row, 0, sizeof(row));
    row.initializerToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_MEMBER_DEF,
            7U);
    row.nameStringOffset = 19U;

    TEST_ASSERT_EQUAL_UINT32(
            ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE,
            sizeof(row));
    TEST_ASSERT_EQUAL_UINT32(40U, offsetof(SZrArtifactPropertyDefRow, initializerToken));
    TEST_ASSERT_EQUAL_UINT32(44U, offsetof(SZrArtifactPropertyDefRow, nameStringOffset));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7U),
            row.initializerToken);
    TEST_ASSERT_EQUAL_UINT32(19U, row.nameStringOffset);
}

static void property_consumer_assert_compiled_property_carrier(
        const SZrFunction *function) {
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;
    const SZrCompiledMemberInfo *visible = ZR_NULL;
    const SZrCompiledMemberInfo *getter = ZR_NULL;
    const SZrCompiledMemberInfo *setter = ZR_NULL;
    TZrUInt32 ordinaryMethodCount = 0U;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo),
            function->prototypeDataLength);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            *(const TZrUInt32 *)function->prototypeData);
    prototype = (const SZrCompiledPrototypeInfo *)(
            function->prototypeData + sizeof(TZrUInt32));
    members = (const SZrCompiledMemberInfo *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));

    for (TZrUInt32 index = 0U; index < prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &members[index];

        if (member->memberType == ZR_AST_PROPERTY_DECLARATION) {
            TEST_ASSERT_NULL(visible);
            visible = member;
        } else if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) {
            TEST_ASSERT_NULL(getter);
            getter = member;
        } else if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_SET) {
            TEST_ASSERT_NULL(setter);
            setter = member;
        } else if (member->memberType == ZR_AST_CLASS_METHOD &&
                   member->propertyIdentity == UINT32_MAX) {
            ordinaryMethodCount++;
        }
    }

    TEST_ASSERT_NOT_NULL(visible);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_NOT_NULL(setter);
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, visible->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(
            visible->propertyIdentity,
            getter->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(
            visible->propertyIdentity,
            setter->propertyIdentity);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            visible->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_REFERENCE_ACCESS_NONE,
            visible->metaType);
    TEST_ASSERT_FALSE(visible->isMetaMethod);
    TEST_ASSERT_LESS_THAN_UINT32(
            function->constantValueLength,
            getter->functionConstantIndex);
    TEST_ASSERT_LESS_THAN_UINT32(
            function->constantValueLength,
            setter->functionConstantIndex);
    TEST_ASSERT_NOT_EQUAL(
            getter->functionConstantIndex,
            setter->functionConstantIndex);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, ordinaryMethodCount);
}

static void test_current_zro_property_carrier_roundtrips_exact_rows(void) {
    static const TZrChar binaryPath[] =
            "property_consumer_carrier_roundtrip.zro";
    const TZrChar *source =
            "class Meter {\n"
            "  pri var stored: int = 7;\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "  pub fn __get_fake(): int { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_carrier.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state,
            source,
            strlen(source),
            sourceName);
    SZrFunction *loadedFunction;

    TEST_ASSERT_NOT_NULL(function);
    property_consumer_assert_compiled_property_carrier(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state,
            function,
            binaryPath));
    loadedFunction = property_consumer_load_binary_entry(binaryPath);
    property_consumer_assert_compiled_property_carrier(loadedFunction);
    TEST_ASSERT_EQUAL_UINT32(
            function->prototypeDataLength,
            loadedFunction->prototypeDataLength);
    TEST_ASSERT_EQUAL_MEMORY(
            function->prototypeData,
            loadedFunction->prototypeData,
            function->prototypeDataLength);

    remove(binaryPath);
    ZrCore_Function_Free(g_state, loadedFunction);
    ZrCore_Function_Free(g_state, function);
}

static void test_binary_import_merges_property_contract_into_placeholder(void) {
    static const TZrChar binaryPath[] =
            "property_consumer_binary_import.zro";
    static const TZrChar providerSource[] =
            "pub class Meter {\n"
            "  pub static property shared: int {\n"
            "    get { return 7; }\n"
            "  }\n"
            "}\n";
    static const TZrChar consumerSource[] =
            "var binaryStage = import(\"graph_binary_stage\");\n"
            "var answer = binaryStage.Meter.shared;\n"
            "return answer;\n";
    SZrFunction *provider = ZR_NULL;
    SZrString *providerName = ZR_NULL;
    SZrString *consumerName = ZR_NULL;
    SZrAstNode *consumerAst = ZR_NULL;
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0U;
    ZrTestsFixtureSource fixture;
    TZrBool (*previousSourceLoader)(
            SZrState *, TZrNativeString, TZrNativeString, SZrIo *) = ZR_NULL;
    SZrCompilerState cs;
    SZrTypeMemberInfo *visibleProperty = ZR_NULL;
    SZrTypeMemberInfo *getter = ZR_NULL;
    SZrParserSemanticPropertyQuery query;

    providerName = ZrCore_String_CreateFromNative(
            g_state,
            "graph_binary_stage.zr");
    TEST_ASSERT_NOT_NULL(providerName);
    provider = ZrParser_Source_Compile(
            g_state,
            providerSource,
            strlen(providerSource),
            providerName);
    TEST_ASSERT_NOT_NULL(provider);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state,
            provider,
            binaryPath));
    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, (TZrUInt32)binaryLength);

    memset(&fixture, 0, sizeof(fixture));
    fixture.path = "graph_binary_stage";
    fixture.bytes = binaryBytes;
    fixture.length = binaryLength;
    fixture.isBinary = ZR_TRUE;
    previousSourceLoader = g_state->global->sourceLoader;
    g_property_consumer_import_fixtures = &fixture;
    g_property_consumer_import_fixture_count = 1U;
    g_state->global->sourceLoader = property_consumer_import_source_loader;

    consumerName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_binary_import.zr");
    consumerAst = ZrParser_Parse(
            g_state,
            consumerSource,
            strlen(consumerSource),
            consumerName);
    TEST_ASSERT_NOT_NULL(consumerAst);
    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, consumerAst);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    for (TZrSize prototypeIndex = 0U;
         prototypeIndex < cs.typePrototypes.length;
         prototypeIndex++) {
        SZrTypePrototypeInfo *prototype =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &cs.typePrototypes,
                        prototypeIndex);

        if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
            strcmp(
                    ZrCore_String_GetNativeString(prototype->name),
                    "Meter") != 0) {
            continue;
        }
        TEST_ASSERT_NOT_NULL(prototype->importModuleName);
        TEST_ASSERT_EQUAL_STRING(
                "graph_binary_stage",
                ZrCore_String_GetNativeString(prototype->importModuleName));
        for (TZrSize memberIndex = 0U;
             memberIndex < prototype->members.length;
             memberIndex++) {
            SZrTypeMemberInfo *member =
                    (SZrTypeMemberInfo *)ZrCore_Array_Get(
                            &prototype->members,
                            memberIndex);

            if (member == ZR_NULL ||
                member->propertyIdentity == UINT32_MAX) {
                continue;
            }
            if (member->memberType == ZR_AST_PROPERTY_DECLARATION &&
                member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
                visibleProperty = member;
            } else if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) {
                getter = member;
            }
        }
        break;
    }

    TEST_ASSERT_NOT_NULL(visibleProperty);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_EQUAL_UINT32(
            visibleProperty->propertyIdentity,
            getter->propertyIdentity);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            visibleProperty->propertySymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            visibleProperty->propertyValueTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyBySymbolId(
            cs.semanticContext,
            visibleProperty->propertySymbolId,
            &query));
    TEST_ASSERT_EQUAL_UINT32(
            visibleProperty->propertyValueTypeId,
            query.propertyTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, query.getterSymbolId);

    property_consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, consumerAst);
    g_state->global->sourceLoader = previousSourceLoader;
    g_property_consumer_import_fixtures = ZR_NULL;
    g_property_consumer_import_fixture_count = 0U;
    free(binaryBytes);
    remove(binaryPath);
    ZrCore_Function_Free(g_state, provider);
}

static void test_source_reflection_exposes_linked_property_accessors(void) {
    static const TZrChar binaryPath[] =
            "property_consumer_reflection_roundtrip.zro";
    const TZrChar *source =
            "class Meter {\n"
            "  pri var stored: int = 7;\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "  pub fn __get_fake(): int { return 1; }\n"
            "}\n"
            "var reflected = typeof(Meter).members.value[0];\n"
            "var decoy = typeof(Meter).members.__get_fake[0];\n"
            "return reflected.kind == \"property\" &&\n"
            "       reflected.getter != null &&\n"
            "       reflected.setter != null &&\n"
            "       reflected.propertyIdentity >= 0 &&\n"
            "       reflected.propertyTypeId > 0 &&\n"
            "       reflected.getter.name == \"get\" &&\n"
            "       reflected.setter.name == \"set\" &&\n"
            "       reflected.getterAccess == reflected.getter.access &&\n"
            "       reflected.setterAccess == reflected.setter.access &&\n"
            "       reflected.receiverEffect == reflected.getter.receiverEffect &&\n"
            "       reflected.referenceAccess == reflected.getter.referenceAccess &&\n"
            "       reflected.exportsWritableRef == reflected.getter.exportsWritableRef &&\n"
            "       decoy.kind == \"method\" ? 1 : 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_reflection.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state,
            source,
            strlen(source),
            sourceName);
    SZrFunction *loadedFunction;
    TZrInt64 result = 0;
    TZrInt64 loadedResult = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state,
            function,
            &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state,
            function,
            binaryPath));
    loadedFunction = property_consumer_load_binary_entry(binaryPath);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state,
            loadedFunction,
            &loadedResult));
    TEST_ASSERT_EQUAL_INT64(1, loadedResult);
    remove(binaryPath);
    ZrCore_Function_Free(g_state, loadedFunction);
    ZrCore_Function_Free(g_state, function);
}

static void test_legacy_property_migrations_publish_exact_structured_edits(void) {
    const TZrChar *pairSource =
            "class Meter {\n"
            "  pub get value: int { return this.stored; }\n"
            "  pub set value(input: int) { this.stored = input; }\n"
            "}\n";
    const TZrChar *singleSource =
            "class Meter {\n"
            "  pub static get shared: int { return 7; }\n"
            "}\n";
    const TZrChar *interfaceSource =
            "interface Metric {\n"
            "  pub get set value: int;\n"
            "}\n";
    SZrPropertyMigrationDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *ast;
    const TZrChar *pairStart;
    const TZrChar *pairEnd;

    ast = property_consumer_parse_with_migration_capture(
            pairSource,
            "property_migration_pair.zr",
            &capture,
            &parserState);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(parserState.hasError);
    TEST_ASSERT_EQUAL_UINT32(1U, capture.count);
    TEST_ASSERT_EQUAL_INT(
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            capture.severity[0]);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3U, capture.relatedCount[0]);
    TEST_ASSERT_EQUAL_UINT32(1U, capture.fixCount[0]);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE,
            capture.applicability[0]);
    pairStart = strstr(pairSource, "pub get value");
    pairEnd = strstr(pairSource, "{ this.stored = input; }");
    TEST_ASSERT_NOT_NULL(pairStart);
    TEST_ASSERT_NOT_NULL(pairEnd);
    pairEnd += strlen("{ this.stored = input; }");
    TEST_ASSERT_EQUAL_UINT64(
            (TZrSize)(pairStart - pairSource),
            capture.editRange[0].start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrSize)(pairEnd - pairSource),
            capture.editRange[0].end.offset);
    TEST_ASSERT_NOT_NULL(strstr(
            capture.editText[0],
            "pub property value: int"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "get { return this.stored; }"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "set {"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "var input = value;"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "this.stored = input;"));
    TEST_ASSERT_NULL(strstr(capture.editText[0], "__get_"));
    TEST_ASSERT_NULL(strstr(capture.editText[0], "__set_"));
    property_consumer_assert_replacement_parses(
            capture.editText[0],
            "class Meter {\n  pri var stored: int = 0;\n  ",
            "\n}\n");
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parserState);

    ast = property_consumer_parse_with_migration_capture(
            singleSource,
            "property_migration_single.zr",
            &capture,
            &parserState);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT32(1U, capture.count);
    TEST_ASSERT_EQUAL_UINT32(1U, capture.fixCount[0]);
    TEST_ASSERT_NOT_NULL(strstr(
            capture.editText[0],
            "pub static property shared: int"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "get { return 7; }"));
    property_consumer_assert_replacement_parses(
            capture.editText[0],
            "class Meter {\n  ",
            "\n}\n");
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parserState);

    ast = property_consumer_parse_with_migration_capture(
            interfaceSource,
            "property_migration_interface.zr",
            &capture,
            &parserState);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT32(1U, capture.count);
    TEST_ASSERT_EQUAL_UINT32(1U, capture.fixCount[0]);
    TEST_ASSERT_NOT_NULL(strstr(
            capture.editText[0],
            "pub property value: int"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "get;"));
    TEST_ASSERT_NOT_NULL(strstr(capture.editText[0], "set;"));
    property_consumer_assert_replacement_parses(
            capture.editText[0],
            "interface Metric {\n  ",
            "\n}\n");
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parserState);
}

static void test_legacy_property_migration_rejects_ambiguous_pairs(void) {
    const TZrChar *sources[] = {
        "class Meter {\n"
        "  pub get value: int { return 1; }\n"
        "  pub set other(value: int) { }\n"
        "}\n",
        "class Meter {\n"
        "  pub get value: int { return 1; }\n"
        "  pub set value(value: string) { }\n"
        "}\n",
        "class Meter {\n"
        "  pub get value: int { return 1; }\n"
        "  pub fn touch(): void { }\n"
        "  pub set value(value: int) { }\n"
        "}\n"
    };

    for (TZrSize sourceIndex = 0U;
         sourceIndex < sizeof(sources) / sizeof(sources[0]);
         sourceIndex++) {
        SZrPropertyMigrationDiagnosticCapture capture;
        SZrParserState parserState;
        SZrAstNode *ast = property_consumer_parse_with_migration_capture(
                sources[sourceIndex],
                "property_migration_negative.zr",
                &capture,
                &parserState);

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(parserState.hasError);
        TEST_ASSERT_EQUAL_UINT32(2U, capture.count);
        TEST_ASSERT_EQUAL_UINT32(0U, capture.fixCount[0]);
        TEST_ASSERT_EQUAL_UINT32(0U, capture.fixCount[1]);
        ZrParser_Ast_Free(g_state, ast);
        ZrParser_State_Free(&parserState);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_property_query_public_shape_is_available);
    RUN_TEST(test_property_query_roundtrips_compiler_property_identity);
    RUN_TEST(test_many_properties_publish_unique_canonical_contracts);
    RUN_TEST(test_property_def_row_preserves_initializer_and_visible_name);
    RUN_TEST(test_current_zro_property_carrier_roundtrips_exact_rows);
    RUN_TEST(
            test_public_runtime_prototype_bootstrap_preserves_property_contract);
    RUN_TEST(test_binary_import_merges_property_contract_into_placeholder);
    RUN_TEST(test_source_reflection_exposes_linked_property_accessors);
    RUN_TEST(test_aot_stripping_preserves_structured_property_dispatch_roots);
    RUN_TEST(test_legacy_property_migrations_publish_exact_structured_edits);
    RUN_TEST(test_legacy_property_migration_rejects_ambiguous_pairs);
    return UNITY_END();
}
