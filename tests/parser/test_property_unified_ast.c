#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

typedef struct SPropertyParserErrorCapture {
    TZrUInt32 count;
    SZrFileRange firstRange;
    char firstMessage[192];
} SPropertyParserErrorCapture;

static void capture_parser_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SPropertyParserErrorCapture *capture =
            (SPropertyParserErrorCapture *)userData;
    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL) {
        return;
    }
    if (capture->count == 0u) {
        if (location != ZR_NULL) {
            capture->firstRange = *location;
        }
        if (message != ZR_NULL) {
            snprintf(capture->firstMessage,
                     sizeof(capture->firstMessage),
                     "%s",
                     message);
        }
    }
    capture->count++;
}

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
    SZrString *sourceName =
            ZrCore_String_Create(g_state, "property_unified.zr", 19u);
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *top_level_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static SZrAstNodeArray *type_members(SZrAstNode *declaration) {
    TEST_ASSERT_NOT_NULL(declaration);
    switch (declaration->type) {
        case ZR_AST_CLASS_DECLARATION:
            return declaration->data.classDeclaration.members;
        case ZR_AST_STRUCT_DECLARATION:
            return declaration->data.structDeclaration.members;
        case ZR_AST_INTERFACE_DECLARATION:
            return declaration->data.interfaceDeclaration.members;
        default:
            TEST_FAIL_MESSAGE("Expected class, struct, resource class, or interface");
            return ZR_NULL;
    }
}

static SZrAstNode *type_member(SZrAstNode *declaration, TZrSize index) {
    SZrAstNodeArray *members = type_members(declaration);
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)index,
                                    (TZrUInt32)members->count);
    return members->nodes[index];
}

static SZrPropertyDeclaration *assert_property_declaration(
        SZrAstNode *member,
        const char *expectedName,
        TZrSize expectedAccessorCount) {
    SZrPropertyDeclaration *property;

    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_DECLARATION, member->type);
    property = &member->data.propertyDeclaration;
    TEST_ASSERT_NOT_NULL(property->name);
    TEST_ASSERT_NOT_NULL(property->name->name);
    TEST_ASSERT_EQUAL_STRING(
            expectedName,
            ZrCore_String_GetNativeString(property->name->name));
    TEST_ASSERT_NOT_NULL(property->typeInfo);
    TEST_ASSERT_NOT_NULL(property->accessors);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)expectedAccessorCount,
                             (TZrUInt32)property->accessors->count);
    TEST_ASSERT_TRUE(property->nameLocation.end.offset >
                     property->nameLocation.start.offset);
    TEST_ASSERT_TRUE(member->location.end.offset > member->location.start.offset);
    return property;
}

static SZrPropertyAccessor *assert_property_accessor(
        SZrPropertyDeclaration *property,
        TZrSize index,
        EZrPropertyAccessorKind expectedKind,
        EZrPropertyAccessorBodyKind expectedBodyKind) {
    SZrAstNode *node;
    SZrPropertyAccessor *accessor;

    TEST_ASSERT_NOT_NULL(property);
    TEST_ASSERT_NOT_NULL(property->accessors);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)property->accessors->count);
    node = property->accessors->nodes[index];
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_ACCESSOR, node->type);
    accessor = &node->data.propertyAccessor;
    TEST_ASSERT_EQUAL_INT(expectedKind, accessor->kind);
    TEST_ASSERT_EQUAL_INT(expectedBodyKind, accessor->bodyKind);
    TEST_ASSERT_TRUE(accessor->keywordLocation.end.offset >
                     accessor->keywordLocation.start.offset);
    TEST_ASSERT_TRUE(node->location.end.offset > node->location.start.offset);
    return accessor;
}

static void test_class_property_owns_ordered_get_and_set_accessors(void) {
    static const char source[] =
            "class Box {\n"
            "  pub property value: int {\n"
            "    pub get { return 1; }\n"
            "    pri set { return; }\n"
            "  }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_AST_CONSTANT_PROPERTY_DECLARATION,
            (TZrUInt32)ZR_AST_PROPERTY_DECLARATION);
    SZrAstNode *classNode = top_level_statement(script, 0u);
    SZrPropertyDeclaration *property =
            assert_property_declaration(type_member(classNode, 0u), "value", 2u);
    SZrPropertyAccessor *getter = assert_property_accessor(
            property, 0u, ZR_PROPERTY_ACCESSOR_GET,
            ZR_PROPERTY_ACCESSOR_BODY_BLOCK);
    SZrPropertyAccessor *setter = assert_property_accessor(
            property, 1u, ZR_PROPERTY_ACCESSOR_SET,
            ZR_PROPERTY_ACCESSOR_BODY_BLOCK);

    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, property->access);
    TEST_ASSERT_TRUE(getter->hasAccessOverride);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, getter->access);
    TEST_ASSERT_TRUE(setter->hasAccessOverride);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PRIVATE, setter->access);
    TEST_ASSERT_NOT_NULL(getter->body);
    TEST_ASSERT_NOT_NULL(setter->body);
    ZrParser_Ast_Free(g_state, script);
}

static void test_struct_property_uses_expression_accessor_body(void) {
    static const char source[] =
            "struct Pair {\n"
            "  pub property first: int { pub get => 1; }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *structNode = top_level_statement(script, 0u);
    SZrPropertyDeclaration *property =
            assert_property_declaration(type_member(structNode, 0u), "first", 1u);
    SZrPropertyAccessor *getter = assert_property_accessor(
            property, 0u, ZR_PROPERTY_ACCESSOR_GET,
            ZR_PROPERTY_ACCESSOR_BODY_EXPRESSION);

    TEST_ASSERT_NOT_NULL(getter->body);
    ZrParser_Ast_Free(g_state, script);
}

static void test_resource_class_property_uses_the_same_ast(void) {
    static const char source[] =
            "resource class Handle {\n"
            "  pub property id: int { pub get { return 1; } }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *classNode = top_level_statement(script, 0u);
    SZrPropertyDeclaration *property;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_TRUE((classNode->data.classDeclaration.modifierFlags &
                      ZR_DECLARATION_MODIFIER_RESOURCE) != 0u);
    property = assert_property_declaration(
            type_member(classNode, 0u), "id", 1u);
    assert_property_accessor(property, 0u, ZR_PROPERTY_ACCESSOR_GET,
                             ZR_PROPERTY_ACCESSOR_BODY_BLOCK);
    ZrParser_Ast_Free(g_state, script);
}

static void test_interface_property_uses_bodyless_accessor_nodes(void) {
    static const char source[] =
            "interface Named {\n"
            "  pub property name: string { pub get; }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *interfaceNode = top_level_statement(script, 0u);
    SZrPropertyDeclaration *property = assert_property_declaration(
            type_member(interfaceNode, 0u), "name", 1u);
    SZrPropertyAccessor *getter = assert_property_accessor(
            property, 0u, ZR_PROPERTY_ACCESSOR_GET,
            ZR_PROPERTY_ACCESSOR_BODY_BODYLESS);

    TEST_ASSERT_NULL(getter->body);
    ZrParser_Ast_Free(g_state, script);
}

static void test_property_accessors_preserve_inherited_access_and_body_kinds(void) {
    static const char source[] =
            "class Settings {\n"
            "  pub static property current: int {\n"
            "    get => value;\n"
            "    init { return; }\n"
            "  }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrPropertyDeclaration *property = assert_property_declaration(
            type_member(top_level_statement(script, 0u), 0u), "current", 2u);
    SZrPropertyAccessor *getter = assert_property_accessor(
            property, 0u, ZR_PROPERTY_ACCESSOR_GET,
            ZR_PROPERTY_ACCESSOR_BODY_EXPRESSION);
    SZrPropertyAccessor *initializer = assert_property_accessor(
            property, 1u, ZR_PROPERTY_ACCESSOR_INIT,
            ZR_PROPERTY_ACCESSOR_BODY_BLOCK);

    TEST_ASSERT_TRUE(property->isStatic);
    TEST_ASSERT_FALSE(getter->hasAccessOverride);
    TEST_ASSERT_FALSE(initializer->hasAccessOverride);
    ZrParser_Ast_Free(g_state, script);
}

static void test_property_parser_recovers_after_half_written_accessor(void) {
    static const char source[] =
            "class Recover {\n"
            "  pub property broken: int { pub get }\n"
            "  pub fn ok(): int { return 1; }\n"
            "}\n";
    SZrString *sourceName =
            ZrCore_String_Create(g_state, "property_recovery.zr", 20u);
    SZrParserState parserState;
    SPropertyParserErrorCapture capture;
    SZrAstNode *script;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, capture.count);
    TEST_ASSERT_NOT_EQUAL(0, capture.firstMessage[0]);
    TEST_ASSERT_TRUE(capture.firstRange.end.offset >=
                     capture.firstRange.start.offset);
    ZrParser_Ast_Free(g_state, script);

    memset(&capture, 0, sizeof(capture));
    sourceName = ZrCore_String_Create(
            g_state, "property_missing_body.zr", 24u);
    ZrParser_State_Init(
            &parserState,
            g_state,
            "class Recover { property broken: int pub fn ok(): int { return 1; } }",
            strlen("class Recover { property broken: int pub fn ok(): int { return 1; } }"),
            sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, capture.count);
    TEST_ASSERT_EQUAL_UINT32(
            2u,
            (TZrUInt32)top_level_statement(script, 0u)
                    ->data.classDeclaration.members->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_CLASS_METHOD,
            type_members(top_level_statement(script, 0u))->nodes[1]->type);
    ZrParser_Ast_Free(g_state, script);

    memset(&capture, 0, sizeof(capture));
    sourceName = ZrCore_String_Create(
            g_state, "property_missing_close.zr", 25u);
    ZrParser_State_Init(
            &parserState,
            g_state,
            "class Recover { property broken: int { get; pub fn ok(): int { return 1; } }",
            strlen("class Recover { property broken: int { get; pub fn ok(): int { return 1; } }"),
            sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, capture.count);
    TEST_ASSERT_EQUAL_UINT32(
            2u,
            (TZrUInt32)top_level_statement(script, 0u)
                    ->data.classDeclaration.members->count);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_CLASS_METHOD,
            type_members(top_level_statement(script, 0u))->nodes[1]->type);
    TEST_ASSERT_TRUE(
            type_members(top_level_statement(script, 0u))
                            ->nodes[0]
                            ->location.end.offset <=
                    type_members(top_level_statement(script, 0u))
                            ->nodes[1]
                            ->location.start.offset);
    ZrParser_Ast_Free(g_state, script);
}

static void assert_property_parse_error(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_malformed.zr");
    SZrParserState parserState;
    SPropertyParserErrorCapture capture;
    SZrAstNode *script;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(
            &parserState, g_state, source, strlen(source), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, capture.count);
    TEST_ASSERT_TRUE(capture.firstRange.end.offset >=
                     capture.firstRange.start.offset);
    ZrParser_Ast_Free(g_state, script);
}

static void test_property_parser_reports_malformed_type_close_and_semicolon(void) {
    assert_property_parse_error(
            "class Broken { property value: { get; } }");
    assert_property_parse_error(
            "class Broken { property value: int { get;");
    assert_property_parse_error(
            "interface Broken { property value: int { get } }");
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

static const SZrTypeMemberInfo *find_property_member(
        const SZrTypePrototypeInfo *prototype) {
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        if (member != ZR_NULL &&
            member->memberType == ZR_AST_PROPERTY_DECLARATION &&
            member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            return member;
        }
    }
    return ZR_NULL;
}

static const SZrTypeMemberInfo *find_property_accessor(
        const SZrTypePrototypeInfo *prototype,
        EZrPropertyAccessorRole role) {
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        if (member != ZR_NULL && member->accessorRole == role) {
            return member;
        }
    }
    return ZR_NULL;
}

static void compile_type_declarations(SZrCompilerState *compiler,
                                      SZrAstNode *script) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    for (TZrSize index = 0u;
         index < script->data.script.statements->count && !compiler->hasError;
         index++) {
        SZrAstNode *declaration = script->data.script.statements->nodes[index];
        switch (declaration->type) {
            case ZR_AST_CLASS_DECLARATION:
                ZrParser_Compiler_CompileClassDeclaration(compiler, declaration);
                break;
            case ZR_AST_STRUCT_DECLARATION:
                ZrParser_Compiler_CompileStructDeclaration(compiler, declaration);
                break;
            case ZR_AST_INTERFACE_DECLARATION:
                ZrParser_Compiler_CompileInterfaceDeclaration(compiler, declaration);
                break;
            default:
                TEST_FAIL_MESSAGE("Expected only type declarations");
                break;
        }
    }
}

static void assert_property_symbol_contract(
        const SZrTypePrototypeInfo *prototype,
        TZrBool expectsSetter) {
    const SZrTypeMemberInfo *property = find_property_member(prototype);
    const SZrTypeMemberInfo *getter = find_property_accessor(
            prototype, ZR_PROPERTY_ACCESSOR_ROLE_GET);
    const SZrTypeMemberInfo *setter = find_property_accessor(
            prototype, ZR_PROPERTY_ACCESSOR_ROLE_SET);

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(property);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_EQUAL_STRING(
            "value", ZrCore_String_GetNativeString(property->name));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, property->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID,
                          property->propertyValueTypeId);
    TEST_ASSERT_EQUAL_UINT64(property->symbolId,
                             property->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT64(property->symbolId,
                             getter->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT32(property->propertyIdentity,
                             getter->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT64(getter->symbolId,
                             property->getterAccessorSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_READONLY,
                          getter->receiverEffect);
    TEST_ASSERT_FALSE(property->exportsWritableRef);
    if (expectsSetter) {
        TEST_ASSERT_NOT_NULL(setter);
        TEST_ASSERT_EQUAL_UINT64(property->symbolId,
                                 setter->propertySymbolId);
        TEST_ASSERT_EQUAL_UINT32(property->propertyIdentity,
                                 setter->propertyIdentity);
        TEST_ASSERT_EQUAL_UINT64(setter->symbolId,
                                 property->setterAccessorSymbolId);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_MUTABLE,
                              setter->receiverEffect);
    } else {
        TEST_ASSERT_NULL(setter);
        TEST_ASSERT_EQUAL_UINT64(ZR_SEMANTIC_ID_INVALID,
                                 property->setterAccessorSymbolId);
    }
}

static void test_property_symbol_links_accessors_for_every_container(void) {
    static const char source[] =
            "interface Contract { pub property value: int { pub get; pub set; } }\n"
            "class Box { pub property value: int { pub get { return 1; } pub set { return; } } }\n"
            "struct Pair { pub property value: int { pub get { return 1; } } }\n"
            "resource class Handle { pub property value: int { pub get { return 1; } } }\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_type_declarations(&compiler, script);

    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            4u, (TZrUInt32)compiler.typePrototypes.length);
    assert_property_symbol_contract(find_type_prototype(&compiler, "Contract"),
                                    ZR_TRUE);
    assert_property_symbol_contract(find_type_prototype(&compiler, "Box"),
                                    ZR_TRUE);
    assert_property_symbol_contract(find_type_prototype(&compiler, "Pair"),
                                    ZR_FALSE);
    assert_property_symbol_contract(find_type_prototype(&compiler, "Handle"),
                                    ZR_FALSE);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void assert_property_compile_error(const char *source,
                                          const char *expectedMessage) {
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_type_declarations(&compiler, script);

    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(compiler.errorMessage, expectedMessage),
            compiler.errorMessage);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_property_symbol_rejects_duplicate_and_invalid_accessors(void) {
    assert_property_compile_error(
            "class Bad { property value: int { get { return 1; } get { return 2; } } }",
            "duplicate get accessor");
    assert_property_compile_error(
            "class Bad { property value: int { set { return; } set { return; } } }",
            "duplicate set accessor");
    assert_property_compile_error(
            "class Bad { property value: int { init { return; } init { return; } } }",
            "duplicate init accessor");
    assert_property_compile_error(
            "class Bad { property value: int { set { return; } init { return; } } }",
            "set and init accessors are mutually exclusive");
    assert_property_compile_error(
            "pub class Bad { pri property value: int { pub get { return 1; } } }",
            "accessor visibility cannot be wider than property visibility");
    assert_property_compile_error(
            "class Bad { property value: int { get; } }",
            "concrete property accessor requires a body");
    assert_property_compile_error(
            "interface Bad { property value: int { get { return 1; } } }",
            "interface property accessor must be bodyless");
    assert_property_compile_error(
            "class Bad { property value: int {} }",
            "property requires at least one accessor");
    assert_property_compile_error(
            "interface Contract { property value: int { get; } } "
            "class Bad : Contract { property value: string { get { return \"bad\"; } } }",
            "Concrete class does not implement all abstract/interface members");
    assert_property_compile_error(
            "pub abstract class Base { "
            "pub abstract property value: int { get; set; } } "
            "class Bad : Base { "
            "pub override property value: int { get { return 1; } } }",
            "override must target an inherited base member");
    assert_property_compile_error(
            "class ReadOnly { property value: int { get { return 1; } } "
            "fn mutate() { this.value = 2; } }",
            "does not declare an accessible setter");
    assert_property_compile_error(
            "class WriteOnly { property value: int { set { return; } } "
            "fn read(): int { return this.value; } }",
            "does not declare an accessible getter");
    assert_property_compile_error(
            "class InitOnly { property value: int { init { return; } } "
            "fn read(): int { return this.value; } }",
            "does not declare an accessible getter");
}

static void test_property_symbol_links_initializer_without_setter(void) {
    static const char source[] =
            "class Initializable { property value: int { init { return; } } }";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypePrototypeInfo *prototype;
    const SZrTypeMemberInfo *property;
    const SZrTypeMemberInfo *initializer;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_type_declarations(&compiler, script);

    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    prototype = find_type_prototype(&compiler, "Initializable");
    property = find_property_member(prototype);
    initializer = find_property_accessor(
            prototype, ZR_PROPERTY_ACCESSOR_ROLE_INIT);
    TEST_ASSERT_NOT_NULL(property);
    TEST_ASSERT_NOT_NULL(initializer);
    TEST_ASSERT_EQUAL_UINT64(initializer->symbolId,
                             property->initAccessorSymbolId);
    TEST_ASSERT_EQUAL_UINT64(ZR_SEMANTIC_ID_INVALID,
                             property->setterAccessorSymbolId);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_abstract_property_override_preserves_property_identity(void) {
    static const char source[] =
            "pub abstract class Base {\n"
            "  pub abstract property score: int { get; }\n"
            "}\n"
            "pub final class Derived : Base {\n"
            "  pub override final property score: int { get { return 2; } }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypePrototypeInfo *basePrototype;
    const SZrTypePrototypeInfo *derivedPrototype;
    const SZrTypeMemberInfo *baseProperty;
    const SZrTypeMemberInfo *derivedProperty;
    const SZrTypeMemberInfo *baseGetter;
    const SZrTypeMemberInfo *derivedGetter;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_type_declarations(&compiler, script);

    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    basePrototype = find_type_prototype(&compiler, "Base");
    derivedPrototype = find_type_prototype(&compiler, "Derived");
    baseProperty = find_property_member(basePrototype);
    derivedProperty = find_property_member(derivedPrototype);
    baseGetter = find_property_accessor(
            basePrototype, ZR_PROPERTY_ACCESSOR_ROLE_GET);
    derivedGetter = find_property_accessor(
            derivedPrototype, ZR_PROPERTY_ACCESSOR_ROLE_GET);
    TEST_ASSERT_NOT_NULL(baseProperty);
    TEST_ASSERT_NOT_NULL(derivedProperty);
    TEST_ASSERT_NOT_NULL(baseGetter);
    TEST_ASSERT_NOT_NULL(derivedGetter);
    TEST_ASSERT_EQUAL_UINT32(baseProperty->propertyIdentity,
                             derivedProperty->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(baseGetter->propertyIdentity,
                             derivedGetter->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(baseProperty->virtualSlotIndex,
                             derivedProperty->virtualSlotIndex);
    TEST_ASSERT_EQUAL_UINT32(baseGetter->virtualSlotIndex,
                             derivedGetter->virtualSlotIndex);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_legacy_property_nodes_are_not_semantic_sources(void) {
    static const char legacySource[] =
            "class Legacy { pub get value: int { return 1; } } "
            "interface LegacyContract { get value: int; }";
    SZrString *sourceName = ZrCore_String_Create(
            g_state, "legacy_property.zr", 18u);
    SZrParserState parserState;
    SPropertyParserErrorCapture capture;
    SZrAstNode *script;
    SZrAstNode *classNode;
    SZrAstNode *validScript;
    SZrAstNode *propertyNode;
    SZrCompilerState compiler;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(
            &parserState,
            g_state,
            legacySource,
            strlen(legacySource),
            sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, capture.count);
    classNode = top_level_statement(script, 0u);
    TEST_ASSERT_EQUAL_UINT32(
            0u, (TZrUInt32)type_members(classNode)->count);
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            (TZrUInt32)type_members(top_level_statement(script, 1u))->count);
    ZrParser_Ast_Free(g_state, script);

    validScript = parse_source(
            "class Legacy { property value: int { get { return 1; } } }");
    propertyNode = type_members(top_level_statement(validScript, 0u))->nodes[0];
    propertyNode->type = ZR_AST_CLASS_PROPERTY;
    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = validScript;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    ZrParser_Compiler_CompileClassDeclaration(
            &compiler, top_level_statement(validScript, 0u));
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage,
            "legacy property accessor syntax is not a semantic source"));
    propertyNode->type = ZR_AST_PROPERTY_DECLARATION;
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, validScript);
}

static void test_property_read_write_uses_linked_accessor_symbols(void) {
    static const char source[] =
            "class Score {\n"
            "  pub var stored: int;\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set => this.stored = value;\n"
            "  }\n"
            "}\n"
            "var score = new Score();\n"
            "score.value = 9;\n"
            "return score.value;\n";
    SZrAstNode *script = parse_source(source);
    SZrFunction *function = ZrParser_Compiler_Compile(g_state, script);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(9, result);
    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

static void test_struct_property_getter_uses_borrowed_receiver_frame(void) {
    static const char source[] =
            "struct Pair {\n"
            "  pub var raw: int;\n"
            "  pub property first: int { get { return this.raw; } }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypeMemberInfo *getter;
    const SZrFunctionFrameSlotLayout *receiverLayout;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    compile_type_declarations(&compiler, script);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    getter = find_property_accessor(
            find_type_prototype(&compiler, "Pair"),
            ZR_PROPERTY_ACCESSOR_ROLE_GET);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_NOT_NULL(getter->compiledFunction);
    receiverLayout = ZrCore_Function_FindFrameSlotLayout(
            getter->compiledFunction, 0u);
    TEST_ASSERT_NOT_NULL(receiverLayout);
    TEST_ASSERT_BITS_HIGH(
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS,
            receiverLayout->reserved0);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_source_hidden_accessor_spelling_does_not_create_property(void) {
    static const char source[] =
            "class MethodOnly {\n"
            "  pub __get_value(): int { return 7; }\n"
            "}\n"
            "\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypePrototypeInfo *prototype;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    compile_type_declarations(&compiler, script);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    prototype = find_type_prototype(&compiler, "MethodOnly");
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NULL(find_property_member(prototype));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_init_accessor_survives_runtime_descriptor_materialization(void) {
    static const char source[] =
            "%module \"property_init\";\n"
            "pub class Initializable {\n"
            "  pub property value: int { init { return; } }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_init.zr");
    SZrFunction *entryFunction = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    SZrObjectModule *module;
    SZrString *moduleName;
    SZrString *typeName;
    const SZrTypeValue *typeValue;
    SZrObjectPrototype *prototype;
    const SZrMemberDescriptor *propertyDescriptor = ZR_NULL;

    TEST_ASSERT_NOT_NULL(entryFunction);
    module = ZrCore_Module_Create(g_state);
    TEST_ASSERT_NOT_NULL(module);
    moduleName = ZrCore_String_CreateFromNative(g_state, "property_init");
    ZrCore_Module_SetInfo(
            g_state,
            module,
            moduleName,
            ZrCore_Module_CalculatePathHash(g_state, sourceName),
            sourceName);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            (TZrUInt32)ZrCore_Module_CreatePrototypesFromData(
                    g_state, module, entryFunction));
    typeName = ZrCore_String_CreateFromNative(g_state, "Initializable");
    typeValue = ZrCore_Module_GetPubExport(g_state, module, typeName);
    TEST_ASSERT_NOT_NULL(typeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, typeValue->type);
    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(
            g_state, typeValue->value.object);
    TEST_ASSERT_NOT_NULL(prototype);
    for (TZrUInt32 index = 0u;
         index < prototype->memberDescriptorCount;
         index++) {
        const SZrMemberDescriptor *candidate =
                &prototype->memberDescriptors[index];
        if (candidate->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            candidate->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(candidate->name), "value") == 0) {
            propertyDescriptor = candidate;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(propertyDescriptor);
    TEST_ASSERT_NULL(propertyDescriptor->getterFunction);
    TEST_ASSERT_NULL(propertyDescriptor->setterFunction);
    TEST_ASSERT_NOT_NULL(propertyDescriptor->initializerFunction);
    TEST_ASSERT_FALSE(propertyDescriptor->isWritable);

    ZrCore_Function_Free(g_state, entryFunction);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_class_property_owns_ordered_get_and_set_accessors);
    RUN_TEST(test_struct_property_uses_expression_accessor_body);
    RUN_TEST(test_resource_class_property_uses_the_same_ast);
    RUN_TEST(test_interface_property_uses_bodyless_accessor_nodes);
    RUN_TEST(test_property_accessors_preserve_inherited_access_and_body_kinds);
    RUN_TEST(test_property_parser_recovers_after_half_written_accessor);
    RUN_TEST(test_property_parser_reports_malformed_type_close_and_semicolon);
    RUN_TEST(test_property_symbol_links_accessors_for_every_container);
    RUN_TEST(test_property_symbol_rejects_duplicate_and_invalid_accessors);
    RUN_TEST(test_property_symbol_links_initializer_without_setter);
    RUN_TEST(test_abstract_property_override_preserves_property_identity);
    RUN_TEST(test_legacy_property_nodes_are_not_semantic_sources);
    RUN_TEST(test_property_read_write_uses_linked_accessor_symbols);
    RUN_TEST(test_struct_property_getter_uses_borrowed_receiver_frame);
    RUN_TEST(test_source_hidden_accessor_spelling_does_not_create_property);
    RUN_TEST(test_init_accessor_survives_runtime_descriptor_materialization);
    return UNITY_END();
}
