#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "../../zr_vm_core/src/zr_vm_core/object/object_internal.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

#pragma pack(push, 1)
typedef struct SExplicitFieldCompiledPrototypeView {
    TZrUInt32 nameStringIndex;
    TZrUInt32 type;
    TZrUInt32 accessModifier;
    TZrUInt32 inheritsCount;
    TZrUInt32 membersCount;
    TZrUInt64 protocolMask;
    TZrUInt32 hasDecoratorMetadata;
    TZrUInt32 decoratorMetadataConstantIndex;
    TZrUInt32 decoratorsCount;
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutByteAlign;
} SExplicitFieldCompiledPrototypeView;

typedef struct SExplicitFieldCompiledMemberView {
    TZrUInt32 memberType;
    TZrUInt32 nameStringIndex;
    TZrUInt32 accessModifier;
    TZrUInt32 isStatic;
    TZrUInt32 isConst;
    TZrUInt32 fieldTypeNameStringIndex;
    TZrUInt32 fieldOffset;
    TZrUInt32 fieldSize;
    TZrUInt32 isMetaMethod;
    TZrUInt32 metaType;
    TZrUInt32 functionConstantIndex;
    TZrUInt32 parameterCount;
    TZrUInt32 returnTypeNameStringIndex;
    TZrUInt32 reservedRemovedUsingManaged;
    TZrUInt32 ownershipQualifier;
    TZrUInt32 callsClose;
    TZrUInt32 callsDestructor;
    TZrUInt32 declarationOrder;
    TZrUInt32 contractRole;
    TZrUInt32 hasDecoratorMetadata;
    TZrUInt32 decoratorMetadataConstantIndex;
    TZrUInt32 hasDecoratorNames;
    TZrUInt32 decoratorNamesConstantIndex;
    TZrUInt32 modifierFlags;
    TZrUInt32 ownerTypeNameStringIndex;
    TZrUInt32 baseDefinitionOwnerTypeNameStringIndex;
    TZrUInt32 baseDefinitionNameStringIndex;
    TZrUInt32 virtualSlotIndex;
    TZrUInt32 interfaceContractSlot;
    TZrUInt32 propertyIdentity;
    TZrUInt32 accessorRole;
} SExplicitFieldCompiledMemberView;
#pragma pack(pop)

static SZrState *g_state;

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

static SZrAstNode *parse_source(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_explicit_field_init.zr");
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *first_type_declaration(SZrAstNode *script) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u, (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[0];
}

static SZrAstNode *class_member(SZrAstNode *classNode, TZrSize index) {
    TEST_ASSERT_NOT_NULL(classNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)classNode->data.classDeclaration.members->count);
    return classNode->data.classDeclaration.members->nodes[index];
}

static SZrFunction *compile_source(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_explicit_field_init.zr");
    return ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
}

static void assert_class_compile_error_range(
        const char *source,
        const char *expectedMessage,
        const char *expectedRangeText) {
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const char *expectedStart = strstr(source, expectedRangeText);

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(expectedStart);
    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (TZrSize index = 0u;
         index < script->data.script.statements->count && !compiler.hasError;
         index++) {
        SZrAstNode *declaration = script->data.script.statements->nodes[index];
        if (declaration->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, declaration);
        }
    }

    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(compiler.errorMessage, expectedMessage),
            compiler.errorMessage);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(expectedStart - source),
            compiler.errorLocation.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(expectedStart - source + strlen(expectedRangeText)),
            compiler.errorLocation.end.offset);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static SZrString *string_constant_at(
        const SZrFunction *function,
        TZrUInt32 index) {
    const SZrTypeValue *constant;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }
    constant = &function->constantValueList[index];
    if (constant->type != ZR_VALUE_TYPE_STRING ||
        constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_STRING(g_state, constant->value.object);
}

static const SExplicitFieldCompiledPrototypeView *compiled_prototype_by_name(
        const SZrFunction *function,
        const char *name) {
    const TZrByte *cursor;
    TZrSize remaining;

    if (function == ZR_NULL || function->prototypeData == ZR_NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32)) {
        return ZR_NULL;
    }
    cursor = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    while (remaining >= sizeof(SExplicitFieldCompiledPrototypeView)) {
        const SExplicitFieldCompiledPrototypeView *prototype =
                (const SExplicitFieldCompiledPrototypeView *)cursor;
        TZrSize byteLength =
                sizeof(*prototype) +
                (TZrSize)prototype->inheritsCount * sizeof(TZrUInt32) +
                (TZrSize)prototype->decoratorsCount * sizeof(TZrUInt32) +
                (TZrSize)prototype->membersCount *
                        sizeof(SExplicitFieldCompiledMemberView);
        SZrString *actualName;

        if (remaining < byteLength) {
            return ZR_NULL;
        }
        actualName = string_constant_at(function, prototype->nameStringIndex);
        if (actualName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(actualName), name) == 0) {
            return prototype;
        }
        cursor += byteLength;
        remaining -= byteLength;
    }
    return ZR_NULL;
}

static const SExplicitFieldCompiledMemberView *compiled_prototype_members(
        const SExplicitFieldCompiledPrototypeView *prototype) {
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    return (const SExplicitFieldCompiledMemberView *)(
            (const TZrByte *)prototype + sizeof(*prototype) +
            (TZrSize)prototype->inheritsCount * sizeof(TZrUInt32) +
            (TZrSize)prototype->decoratorsCount * sizeof(TZrUInt32));
}

static SZrFunction *function_constant_at(
        const SZrFunction *function,
        TZrUInt32 index) {
    const SZrTypeValue *constant;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }
    constant = &function->constantValueList[index];
    if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
        constant->value.object == ZR_NULL || constant->isNative) {
        return ZR_NULL;
    }
    return ZR_CAST_FUNCTION(g_state, constant->value.object);
}

static void fixture_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *load_binary_entry(const char *path) {
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
            fixture_reader_close_noop,
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

static const SZrTypePrototypeInfo *find_type_prototype(
        const SZrCompilerState *compiler,
        const char *name) {
    for (TZrSize index = 0u; index < compiler->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        (SZrArray *)&compiler->typePrototypes, index);
        if (prototype != ZR_NULL && prototype->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(prototype->name), name) == 0) {
            return prototype;
        }
    }
    return ZR_NULL;
}

static void test_let_and_var_fields_have_distinct_mutability(void) {
    static const char source[] =
            "class Box {\n"
            "  pri let frozen: int = 1;\n"
            "  pri var mutable: int = 2;\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *classNode = first_type_declaration(script);
    SZrAstNode *immutableField = class_member(classNode, 0u);
    SZrAstNode *mutableField = class_member(classNode, 1u);

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, immutableField->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, mutableField->type);
    TEST_ASSERT_TRUE(immutableField->data.classField.isConst);
    TEST_ASSERT_FALSE(mutableField->data.classField.isConst);
    TEST_ASSERT_EQUAL_STRING(
            "frozen",
            ZrCore_String_GetNativeString(
                    immutableField->data.classField.name->name));
    TEST_ASSERT_EQUAL_STRING(
            "mutable",
            ZrCore_String_GetNativeString(
                    mutableField->data.classField.name->name));

    ZrParser_Ast_Free(g_state, script);
}

static void test_let_field_is_initialized_once_in_constructor(void) {
    static const char validSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor(value: int) { this.id = value; }\n"
            "}\n";
    static const char invalidSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor(value: int) { this.id = value; this.id = value; }\n"
            "}\n";
    static const char declarationInitializerRepeatSource[] =
            "class Ticket {\n"
            "  pri let id: int = 1;\n"
            "  pub @constructor() { this.id = 2; }\n"
            "}\n";
    static const char compoundSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor() { this.id += 1; }\n"
            "}\n";
    SZrFunction *validFunction = compile_source(validSource);
    SZrFunction *invalidFunction;

    TEST_ASSERT_NOT_NULL(validFunction);
    ZrCore_Function_Free(g_state, validFunction);

    invalidFunction = compile_source(invalidSource);
    TEST_ASSERT_NULL(invalidFunction);
    TEST_ASSERT_NULL(compile_source(declarationInitializerRepeatSource));
    TEST_ASSERT_NULL(compile_source(compoundSource));
}

static void test_constructor_paths_require_complete_immutable_field_initialization(void) {
    static const char allPathsSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor(value: int) {\n"
            "    if (value > 0) { this.id = value; } else { this.id = 0; }\n"
            "  }\n"
            "}\n";
    static const char missingBranchSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor(value: int) {\n"
            "    if (value > 0) { this.id = value; }\n"
            "  }\n"
            "}\n";
    static const char earlyReturnSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor(value: int) {\n"
            "    if (value < 0) { return; }\n"
            "    this.id = value;\n"
            "  }\n"
            "}\n";
    static const char throwPathSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub @constructor(value: int) {\n"
            "    if (value < 0) { throw \"invalid\"; }\n"
            "    this.id = value;\n"
            "  }\n"
            "}\n";
    SZrFunction *function = compile_source(allPathsSource);

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(compile_source(missingBranchSource));
    TEST_ASSERT_NULL(compile_source(earlyReturnSource));
    function = compile_source(throwPathSource);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
}

static void test_let_locals_and_foreach_bindings_are_immutable(void) {
    static const char validSource[] =
            "fn update() { var value: int = 1; value = 2; }\n";
    static const char invalidLocalSource[] =
            "fn update() { let value: int = 1; value = 2; }\n";
    static const char invalidForeachSource[] =
            "fn update(values) { for (let value in values) { value = 2; } }\n";
    static const char invalidObjectDestructuringSource[] =
            "fn update() { var source = {value: 1}; let {value} = source; value = 2; }\n";
    static const char invalidArrayDestructuringSource[] =
            "fn update() { var source = [1]; let [value] = source; value = 2; }\n";
    static const char invalidForeachDestructuringSource[] =
            "fn update(values) { for (let {value} in values) { value = 2; } }\n";
    SZrFunction *function = compile_source(validSource);

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(compile_source(invalidLocalSource));
    TEST_ASSERT_NULL(compile_source(invalidForeachSource));
    TEST_ASSERT_NULL(compile_source(invalidObjectDestructuringSource));
    TEST_ASSERT_NULL(compile_source(invalidArrayDestructuringSource));
    TEST_ASSERT_NULL(compile_source(invalidForeachDestructuringSource));
}

static void test_binding_ranges_start_at_exact_let_or_var_keyword(void) {
    static const char source[] =
            "fn locals() { let localValue: int = 1; }\n"
            "class Box { pub let classValue: int = 1; }\n"
            "struct Pair { pri var structValue: int = 2; }\n"
            "interface Shape { pub let interfaceValue: int; }\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *functionNode;
    SZrAstNode *localNode;
    SZrAstNode *classNode;
    SZrAstNode *structNode;
    SZrAstNode *interfaceNode;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_UINT32(4u, (TZrUInt32)script->data.script.statements->count);
    functionNode = script->data.script.statements->nodes[0];
    localNode = functionNode->data.functionDeclaration.body->data.block.body->nodes[0];
    classNode = script->data.script.statements->nodes[1];
    structNode = script->data.script.statements->nodes[2];
    interfaceNode = script->data.script.statements->nodes[3];

    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(strstr(source, "let localValue") - source),
            localNode->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(strstr(source, "let classValue") - source),
            classNode->data.classDeclaration.members->nodes[0]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(strstr(source, "var structValue") - source),
            structNode->data.structDeclaration.members->nodes[0]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(strstr(source, "let interfaceValue") - source),
            interfaceNode->data.interfaceDeclaration.members->nodes[0]->location.start.offset);

    ZrParser_Ast_Free(g_state, script);
}

static void test_syntax_writer_prints_explicit_field_names(void) {
    static const char source[] =
            "class Box { pub let classValue: int = 1; }\n"
            "struct Pair { pri var structValue: int = 2; }\n"
            "interface Shape { pub let interfaceValue: int; }\n";
    static const char outputPath[] = "property_explicit_field_init_syntax_tree.zrs";
    SZrAstNode *script = parse_source(source);
    FILE *file;
    char output[8192];
    size_t byteLength;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteSyntaxTreeFile(g_state, script, outputPath));
    file = fopen(outputPath, "rb");
    TEST_ASSERT_NOT_NULL(file);
    byteLength = fread(output, 1u, sizeof(output) - 1u, file);
    output[byteLength] = '\0';
    fclose(file);
    remove(outputPath);

    TEST_ASSERT_NOT_NULL(strstr(output, "binding: let"));
    TEST_ASSERT_NOT_NULL(strstr(output, "binding: var"));
    TEST_ASSERT_NOT_NULL(strstr(output, "name: \"classValue\""));
    TEST_ASSERT_NOT_NULL(strstr(output, "name: \"structValue\""));
    TEST_ASSERT_NOT_NULL(strstr(output, "name: \"interfaceValue\""));
    ZrParser_Ast_Free(g_state, script);
}

static void test_let_field_is_shallow_for_handles_but_not_inline_structs(void) {
    static const char shallowHandleSource[] =
            "class Payload { pub var value: int = 0; }\n"
            "class Holder {\n"
            "  pri let payload: Payload;\n"
            "  pub @constructor(value: Payload) { this.payload = value; }\n"
            "  pub fn mutate() { this.payload.value = 1; }\n"
            "}\n";
    static const char inlineStructSource[] =
            "struct Point { pub var x: int = 0; }\n"
            "class Holder {\n"
            "  pri let point: Point;\n"
            "  pub @constructor(value: Point) { this.point = value; }\n"
            "  pub fn mutate() { this.point.x = 1; }\n"
            "}\n";
    SZrFunction *function = compile_source(shallowHandleSource);

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(compile_source(inlineStructSource));
}

static void test_let_surface_covers_locals_struct_interface_and_foreach(void) {
    static const char source[] =
            "fn localBindings() { let frozen: int = 1; var mutable: int = 2; }\n"
            "struct Pair { pri let first: int = 1; pri var second: int = 2; }\n"
            "interface Shape { pub let dimensions: int; pub var label: string; }\n"
            "fn iterate(values) { for (let value in values) { return; } }\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *functionNode;
    SZrAstNode *structNode;
    SZrAstNode *interfaceNode;
    SZrAstNode *iterateNode;
    SZrAstNodeArray *body;
    SZrAstNode *foreachNode;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(
            4u, (TZrUInt32)script->data.script.statements->count);

    functionNode = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    body = functionNode->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)body->count);
    TEST_ASSERT_TRUE(body->nodes[0]->data.variableDeclaration.isConst);
    TEST_ASSERT_FALSE(body->nodes[1]->data.variableDeclaration.isConst);

    structNode = script->data.script.statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structNode->type);
    TEST_ASSERT_TRUE(structNode->data.structDeclaration.members->nodes[0]
                             ->data.structField.isConst);
    TEST_ASSERT_FALSE(structNode->data.structDeclaration.members->nodes[1]
                              ->data.structField.isConst);

    interfaceNode = script->data.script.statements->nodes[2];
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, interfaceNode->type);
    TEST_ASSERT_TRUE(interfaceNode->data.interfaceDeclaration.members->nodes[0]
                             ->data.interfaceFieldDeclaration.isConst);
    TEST_ASSERT_FALSE(interfaceNode->data.interfaceDeclaration.members->nodes[1]
                              ->data.interfaceFieldDeclaration.isConst);

    iterateNode = script->data.script.statements->nodes[3];
    body = iterateNode->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)body->count);
    foreachNode = body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOREACH_LOOP, foreachNode->type);
    TEST_ASSERT_TRUE(foreachNode->data.foreachLoop.isConst);

    ZrParser_Ast_Free(g_state, script);
}

static void test_explicit_fields_are_separate_from_property_contract(void) {
    static const char source[] =
            "struct Storage {\n"
            "  pri let frozen: int = 1;\n"
            "  pri var mutable: int = 2;\n"
            "  pub property value: int {\n"
            "    get { return this.mutable; }\n"
            "    set { this.mutable = value; }\n"
            "  }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypePrototypeInfo *prototype;
    TZrUInt32 fieldCount = 0u;
    TZrUInt32 propertyCount = 0u;
    TZrUInt32 accessorCount = 0u;
    TZrBool sawImmutable = ZR_FALSE;
    TZrBool sawMutable = ZR_FALSE;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_CompileStructDeclaration(
            &compiler, first_type_declaration(script));
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);

    prototype = find_type_prototype(&compiler, "Storage");
    TEST_ASSERT_NOT_NULL(prototype);
    for (TZrSize index = 0u; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        TEST_ASSERT_NOT_NULL(member);
        if (member->memberType == ZR_AST_STRUCT_FIELD) {
            fieldCount++;
            TEST_ASSERT_GREATER_THAN_UINT32(0u, member->fieldSize);
            if (member->isConst) {
                sawImmutable = ZR_TRUE;
            } else {
                sawMutable = ZR_TRUE;
            }
        } else if (member->memberType == ZR_AST_PROPERTY_DECLARATION &&
                   member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            propertyCount++;
            TEST_ASSERT_EQUAL_UINT32(0u, member->fieldSize);
        } else if (member->accessorRole != ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            accessorCount++;
        }
    }

    TEST_ASSERT_EQUAL_UINT32(2u, fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, propertyCount);
    TEST_ASSERT_EQUAL_UINT32(2u, accessorCount);
    TEST_ASSERT_TRUE(sawImmutable);
    TEST_ASSERT_TRUE(sawMutable);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, prototype->layoutByteSize);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void assert_compiled_storage_contract(const SZrFunction *function) {
    const SExplicitFieldCompiledPrototypeView *prototype =
            compiled_prototype_by_name(function, "Storage");
    const SExplicitFieldCompiledMemberView *members =
            compiled_prototype_members(prototype);
    TZrUInt32 fieldCount = 0u;
    TZrUInt32 propertyCount = 0u;
    TZrUInt32 accessorCount = 0u;
    TZrBool sawLet = ZR_FALSE;
    TZrBool sawVar = ZR_FALSE;
    const SExplicitFieldCompiledMemberView *constructorMember = ZR_NULL;
    SZrFunction *constructorFunction;
    const SZrFunctionFrameSlotLayout *receiverLayout;
    const SZrTypeLayout *receiverTypeLayout;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, prototype->layoutByteSize);
    for (TZrUInt32 index = 0u; index < prototype->membersCount; index++) {
        const SExplicitFieldCompiledMemberView *member = &members[index];
        if (member->memberType == (TZrUInt32)ZR_AST_STRUCT_FIELD) {
            fieldCount++;
            TEST_ASSERT_GREATER_THAN_UINT32(0u, member->fieldSize);
            if (member->isConst != 0u) {
                sawLet = ZR_TRUE;
            } else {
                sawVar = ZR_TRUE;
            }
        } else if (member->memberType ==
                           (TZrUInt32)ZR_AST_PROPERTY_DECLARATION &&
                   member->accessorRole ==
                           (TZrUInt32)ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            propertyCount++;
            TEST_ASSERT_EQUAL_UINT32(0u, member->fieldSize);
        } else if (member->accessorRole !=
                   (TZrUInt32)ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            accessorCount++;
        }
        if (member->isMetaMethod != 0u &&
            member->metaType == (TZrUInt32)ZR_META_CONSTRUCTOR) {
            constructorMember = member;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2u, fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, propertyCount);
    TEST_ASSERT_EQUAL_UINT32(2u, accessorCount);
    TEST_ASSERT_TRUE(sawLet);
    TEST_ASSERT_TRUE(sawVar);
    TEST_ASSERT_NOT_NULL(constructorMember);
    constructorFunction = function_constant_at(
            function, constructorMember->functionConstantIndex);
    TEST_ASSERT_NOT_NULL(constructorFunction);
    receiverLayout = ZrCore_Function_FindFrameSlotLayout(
            constructorFunction, 0u);
    TEST_ASSERT_NOT_NULL(receiverLayout);
    TEST_ASSERT_BITS_HIGH(
            ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP,
            receiverLayout->reserved0);
    receiverTypeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
            constructorFunction,
            receiverLayout->typeLayoutId,
            g_state);
    TEST_ASSERT_NOT_NULL(receiverTypeLayout);
    TEST_ASSERT_EQUAL_UINT32(2u, receiverTypeLayout->fieldCount);
}

static void test_explicit_field_property_artifact_and_reflection_roundtrip(void) {
    static const char source[] =
            "module property_explicit_field_init;\n"
            "pub struct Storage {\n"
            "  pub let frozen: int;\n"
            "  pub var mutable: int;\n"
            "  pub property value: int {\n"
            "    get { return this.mutable; }\n"
            "    set { this.mutable = value; }\n"
            "  }\n"
            "  pub @constructor(frozen: int, mutable: int) {\n"
            "    this.frozen = frozen;\n"
            "    this.mutable = mutable;\n"
            "  }\n"
            "}\n";
    static const char binaryPath[] = "property_explicit_field_init_roundtrip.zro";
    SZrFunction *sourceFunction = compile_source(source);
    SZrFunction *runtimeFunction;
    SZrObjectModule *module;
    SZrString *moduleName;
    SZrString *sourceName;
    SZrString *typeName;
    const SZrTypeValue *typeValue;
    SZrObjectPrototype *prototype;
    TZrUInt32 fieldCount = 0u;
    TZrUInt32 propertyCount = 0u;
    TZrBool sawReadonlyField = ZR_FALSE;
    TZrBool sawWritableField = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(sourceFunction);
    assert_compiled_storage_contract(sourceFunction);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state, sourceFunction, binaryPath));
    runtimeFunction = load_binary_entry(binaryPath);
    assert_compiled_storage_contract(runtimeFunction);
    TEST_ASSERT_EQUAL_UINT32(sourceFunction->prototypeDataLength,
                             runtimeFunction->prototypeDataLength);
    TEST_ASSERT_EQUAL_MEMORY(sourceFunction->prototypeData,
                             runtimeFunction->prototypeData,
                             sourceFunction->prototypeDataLength);

    module = ZrCore_Module_Create(g_state);
    TEST_ASSERT_NOT_NULL(module);
    moduleName = ZrCore_String_CreateFromNative(
            g_state, "property_explicit_field_init");
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_explicit_field_init.zr");
    ZrCore_Module_SetInfo(
            g_state,
            module,
            moduleName,
            ZrCore_Module_CalculatePathHash(g_state, sourceName),
            sourceName);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            (TZrUInt32)ZrCore_Module_CreatePrototypesFromData(
                    g_state, module, runtimeFunction));
    typeName = ZrCore_String_CreateFromNative(g_state, "Storage");
    typeValue = ZrCore_Module_GetPubExport(g_state, module, typeName);
    TEST_ASSERT_NOT_NULL(typeValue);
    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(
            g_state, typeValue->value.object);
    TEST_ASSERT_NOT_NULL(prototype);
    for (TZrUInt32 index = 0u;
         index < prototype->memberDescriptorCount;
         index++) {
        const SZrMemberDescriptor *descriptor =
                &prototype->memberDescriptors[index];
        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD) {
            fieldCount++;
            if (descriptor->isWritable) {
                sawWritableField = ZR_TRUE;
            } else {
                sawReadonlyField = ZR_TRUE;
            }
        } else if (descriptor->kind ==
                   ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY) {
            propertyCount++;
            TEST_ASSERT_NOT_NULL(descriptor->getterFunction);
            TEST_ASSERT_NOT_NULL(descriptor->setterFunction);
            TEST_ASSERT_NULL(descriptor->initializerFunction);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2u, fieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, propertyCount);
    TEST_ASSERT_TRUE(sawReadonlyField);
    TEST_ASSERT_TRUE(sawWritableField);

    remove(binaryPath);
    ZrCore_Function_Free(g_state, runtimeFunction);
    ZrCore_Function_Free(g_state, sourceFunction);
}

static void test_init_property_is_constructor_only_and_uses_explicit_field(void) {
    static const char validSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int {\n"
            "    get { return this.stored; }\n"
            "    init { this.stored = value; }\n"
            "  }\n"
            "  pub @constructor(value: int) { this.id = value; }\n"
            "}\n";
    static const char invalidSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int {\n"
            "    get { return this.stored; }\n"
            "    init { this.stored = value; }\n"
            "  }\n"
            "  pub @constructor(value: int) { this.id = value; }\n"
            "  pub fn replace(value: int) { this.id = value; }\n"
            "}\n";
    SZrFunction *validFunction = compile_source(validSource);
    SZrFunction *invalidFunction;

    TEST_ASSERT_NOT_NULL(validFunction);
    ZrCore_Function_Free(g_state, validFunction);

    invalidFunction = compile_source(invalidSource);
    TEST_ASSERT_NULL(invalidFunction);
}

static void test_set_property_remains_writable_during_and_after_construction(void) {
    static const char source[] =
            "class MutableTicket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "  pub @constructor(value: int) { this.id = value; }\n"
            "  pub fn replace(value: int) { this.id = value; }\n"
            "}\n";
    SZrFunction *function = compile_source(source);

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
}

static void test_init_property_rejects_foreign_and_static_receivers(void) {
    static const char foreignSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int { init { this.stored = value; } }\n"
            "  pub @constructor(value: int) { var other = this; other.id = value; }\n"
            "}\n";
    static const char staticSource[] =
            "class Ticket {\n"
            "  pub static property id: int { init { return; } }\n"
            "}\n";

    assert_class_compile_error_range(
            foreignSource,
            "Property does not declare an accessible setter",
            "other.id = value");
    assert_class_compile_error_range(
            staticSource,
            "static property cannot declare an init accessor",
            "init");
}

static void test_nested_callable_does_not_inherit_init_phase(void) {
    static const char source[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property first: int {\n"
            "    init { this.stored = value; }\n"
            "  }\n"
            "  pub property second: int {\n"
            "    init { var deferred = () => { this.first = value; }; }\n"
            "  }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_hidden_init_accessor_cannot_be_called_directly(void) {
    static const char directCallSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int { init { this.stored = value; } }\n"
            "  pub fn replace(value: int) { this.__set_id(value); }\n"
            "}\n";
    static const char bareReferenceSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int { init { this.stored = value; } }\n"
            "  pub fn leak() { var leaked = this.__set_id; }\n"
            "}\n";
    static const char aliasCallSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 0;\n"
            "  pub property id: int { init { this.stored = value; } }\n"
            "  pub fn replace(value: int) {\n"
            "    var leaked = this.__set_id;\n"
            "    leaked(this, value);\n"
            "  }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(directCallSource));
    TEST_ASSERT_NULL(compile_source(bareReferenceSource));
    TEST_ASSERT_NULL(compile_source(aliasCallSource));
}

static void test_init_accessor_rejects_repeated_immutable_field_write(void) {
    static const char source[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pub property initial: int {\n"
            "    init { this.id = value; this.id = value; }\n"
            "  }\n"
            "  pub @constructor(value: int) { this.id = value; this.initial = value; }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_property_accessor_resolves_preceding_method_contract(void) {
    static const char validSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 1;\n"
            "  pri const fn normalize(value: int): int { return value; }\n"
            "  pub property id: int { get { return this.normalize(this.stored); } }\n"
            "}\n";
    static const char invalidSource[] =
            "class Ticket {\n"
            "  pri var stored: int = 1;\n"
            "  pri const fn normalize(value: int): int { return value; }\n"
            "  pub property id: int { get { return this.normalize(\"wrong\"); } }\n"
            "}\n";
    SZrFunction *function = compile_source(validSource);

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(compile_source(invalidSource));
}

static void test_runtime_initialization_capability_is_exact_and_single_use(void) {
    SZrString *typeName = ZrCore_String_CreateFromNative(g_state, "InitCapabilityBox");
    SZrString *frozenName = ZrCore_String_CreateFromNative(g_state, "frozen");
    SZrString *mutableName = ZrCore_String_CreateFromNative(g_state, "mutable");
    SZrString *propertyName = ZrCore_String_CreateFromNative(g_state, "property");
    SZrString *staticName = ZrCore_String_CreateFromNative(g_state, "staticField");
    SZrString *dynamicName = ZrCore_String_CreateFromNative(g_state, "dynamic");
    SZrObjectPrototype *prototype;
    SZrObject *instance;
    SZrMemberDescriptor descriptor;
    SZrTypeValue receiver;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(typeName);
    prototype = ZrCore_ObjectPrototype_New(
            g_state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = frozenName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
            g_state, prototype, &descriptor));
    descriptor.name = mutableName;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
            g_state, prototype, &descriptor));
    descriptor.name = propertyName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
    descriptor.isWritable = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
            g_state, prototype, &descriptor));
    descriptor.name = staticName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isStatic = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
            g_state, prototype, &descriptor));

    instance = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(g_state, instance);
    ZrCore_Value_InitAsRawObject(
            g_state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    receiver.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsInt(g_state, &value, 42);

    TEST_ASSERT_TRUE(ZrCore_Object_InitializeMember(
            g_state, &receiver, frozenName, &value));
    TEST_ASSERT_FALSE(ZrCore_Object_InitializeMember(
            g_state, &receiver, frozenName, &value));
    TEST_ASSERT_FALSE(ZrCore_Object_InitializeMember(
            g_state, &receiver, mutableName, &value));
    TEST_ASSERT_FALSE(ZrCore_Object_InitializeMember(
            g_state, &receiver, propertyName, &value));
    TEST_ASSERT_FALSE(ZrCore_Object_InitializeMember(
            g_state, &receiver, staticName, &value));
    TEST_ASSERT_FALSE(ZrCore_Object_InitializeMember(
            g_state, &receiver, dynamicName, &value));
}

static void test_let_field_constructor_initialization_executes(void) {
    static const char source[] =
            "class Ticket {\n"
            "  pub let id: int;\n"
            "  pub @constructor(value: int) { this.id = value; }\n"
            "}\n"
            "var ticket = new Ticket(42);\n"
            "return ticket.id;\n";
    static const char structSource[] =
            "struct Ticket {\n"
            "  pub let id: int;\n"
            "  pub @constructor(value: int) { this.id = value; }\n"
            "}\n"
            "var ticket: Ticket = init Ticket(43);\n"
            "return ticket.id;\n";
    SZrFunction *function = compile_source(source);
    const SExplicitFieldCompiledPrototypeView *prototype;
    const SExplicitFieldCompiledMemberView *members;
    SZrFunction *constructorFunction = ZR_NULL;
    TZrBool sawInitializationEntry = ZR_FALSE;
    TZrBool sawInitializationStore = ZR_FALSE;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    prototype = compiled_prototype_by_name(function, "Ticket");
    members = compiled_prototype_members(prototype);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(members);
    for (TZrUInt32 index = 0u; index < prototype->membersCount; index++) {
        if (members[index].isMetaMethod != 0u &&
            members[index].metaType == (TZrUInt32)ZR_META_CONSTRUCTOR) {
            constructorFunction = function_constant_at(
                    function, members[index].functionConstantIndex);
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(constructorFunction);
    for (TZrUInt32 index = 0u; index < constructorFunction->memberEntryLength; index++) {
        SZrFunctionMemberEntry *entry = &constructorFunction->memberEntries[index];
        if (entry->symbol != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(entry->symbol), "id") == 0 &&
            (entry->reserved0 &
             ZR_FUNCTION_MEMBER_ENTRY_FLAG_INITIALIZATION_WRITE) != 0u) {
            sawInitializationEntry = ZR_TRUE;
        }
    }
    for (TZrUInt32 index = 0u; index < constructorFunction->instructionsLength; index++) {
        const TZrInstruction *instruction = &constructorFunction->instructionsList[index];
        TZrUInt32 memberEntryIndex = UINT32_MAX;
        if (instruction->instruction.operationCode ==
            (TZrUInt16)ZR_INSTRUCTION_ENUM(SET_MEMBER)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
        } else if ((instruction->instruction.operationCode ==
                    (TZrUInt16)ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT) ||
                    instruction->instruction.operationCode ==
                            (TZrUInt16)ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT_NULL)) &&
                   instruction->instruction.operand.operand1[1] <
                           constructorFunction->callSiteCacheLength) {
            memberEntryIndex = constructorFunction
                                       ->callSiteCaches[instruction->instruction.operand.operand1[1]]
                                       .memberEntryIndex;
        }
        if (memberEntryIndex < constructorFunction->memberEntryLength &&
            (constructorFunction->memberEntries[memberEntryIndex].reserved0 &
             ZR_FUNCTION_MEMBER_ENTRY_FLAG_INITIALIZATION_WRITE) != 0u) {
            sawInitializationStore = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(sawInitializationEntry);
    TEST_ASSERT_TRUE(sawInitializationStore);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
    ZrCore_Function_Free(g_state, function);

    function = compile_source(structSource);
    result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(43, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_init_property_constructor_assignment_executes(void) {
    static const char source[] =
            "class Ticket {\n"
            "  pri let _seed: int;\n"
            "  pri var _value: int = 0;\n"
            "  pri property initial: int {\n"
            "    init { this._value = value; }\n"
            "  }\n"
            "  pub property value: int {\n"
            "    get { return this._value; }\n"
            "    set { this._value = value; }\n"
            "  }\n"
            "  pub @constructor(initial: int) {\n"
            "    this._seed = initial;\n"
            "    this.initial = initial;\n"
            "  }\n"
            "}\n"
            "var ticket = new Ticket(42);\n"
            "return ticket.value;\n";
    SZrFunction *function = compile_source(source);
    SZrFunction *runtimeFunction;
    static const char binaryPath[] =
            "property_explicit_field_init_execute_roundtrip.zro";
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state, function, binaryPath));
    runtimeFunction = load_binary_entry(binaryPath);
    result = 0;
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, runtimeFunction, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
    remove(binaryPath);
    ZrCore_Function_Free(g_state, runtimeFunction);
    ZrCore_Function_Free(g_state, function);
}

static void test_init_property_cannot_repeat_direct_immutable_field_initialization(void) {
    static const char validSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pri property initial: int { init { this.id = value; } }\n"
            "  pub property value: int { get { return this.id; } }\n"
            "  pub @constructor(value: int) { this.initial = value; }\n"
            "}\n"
            "var ticket = new Ticket(42);\n"
            "return ticket.value;\n";
    static const char repeatedSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pri property initial: int { init { this.id = value; } }\n"
            "  pub @constructor(value: int) {\n"
            "    this.id = value;\n"
            "    this.initial = value;\n"
            "  }\n"
            "}\n"
            "var ticket = new Ticket(42);\n"
            "return ticket.id;\n";
    static const char earlyReturnSource[] =
            "class Ticket {\n"
            "  pri let id: int;\n"
            "  pri property initial: int {\n"
            "    init { if (value < 0) { return; } this.id = value; }\n"
            "  }\n"
            "  pub @constructor(value: int) { this.initial = value; }\n"
            "}\n";
    SZrFunction *function = compile_source(validSource);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(compile_source(repeatedSource));
    TEST_ASSERT_NULL(compile_source(earlyReturnSource));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_let_and_var_fields_have_distinct_mutability);
    RUN_TEST(test_let_field_is_initialized_once_in_constructor);
    RUN_TEST(test_constructor_paths_require_complete_immutable_field_initialization);
    RUN_TEST(test_let_locals_and_foreach_bindings_are_immutable);
    RUN_TEST(test_binding_ranges_start_at_exact_let_or_var_keyword);
    RUN_TEST(test_syntax_writer_prints_explicit_field_names);
    RUN_TEST(test_let_field_is_shallow_for_handles_but_not_inline_structs);
    RUN_TEST(test_let_surface_covers_locals_struct_interface_and_foreach);
    RUN_TEST(test_explicit_fields_are_separate_from_property_contract);
    RUN_TEST(test_explicit_field_property_artifact_and_reflection_roundtrip);
    RUN_TEST(test_init_property_is_constructor_only_and_uses_explicit_field);
    RUN_TEST(test_set_property_remains_writable_during_and_after_construction);
    RUN_TEST(test_init_property_rejects_foreign_and_static_receivers);
    RUN_TEST(test_nested_callable_does_not_inherit_init_phase);
    RUN_TEST(test_hidden_init_accessor_cannot_be_called_directly);
    RUN_TEST(test_init_accessor_rejects_repeated_immutable_field_write);
    RUN_TEST(test_property_accessor_resolves_preceding_method_contract);
    RUN_TEST(test_runtime_initialization_capability_is_exact_and_single_use);
    RUN_TEST(test_let_field_constructor_initialization_executes);
    RUN_TEST(test_init_property_constructor_assignment_executes);
    RUN_TEST(test_init_property_cannot_repeat_direct_immutable_field_initialization);
    return UNITY_END();
}
