#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "harness/module_fixture_support.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

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
            g_state, "property_ref_return.zr");
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrFunction *compile_source(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_ref_return_compile.zr");
    return ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
}

static SZrAstNode *first_property(SZrAstNode *script) {
    SZrAstNode *container;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u, (TZrUInt32)script->data.script.statements->count);
    container = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(container);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, container->type);
    TEST_ASSERT_NOT_NULL(container->data.classDeclaration.members);
    for (TZrSize index = 0u;
         index < container->data.classDeclaration.members->count;
         index++) {
        SZrAstNode *member =
                container->data.classDeclaration.members->nodes[index];
        if (member != ZR_NULL &&
            member->type == ZR_AST_PROPERTY_DECLARATION) {
            return member;
        }
    }
    TEST_FAIL_MESSAGE("Expected property declaration");
    return ZR_NULL;
}

static SZrAstNode *property_getter(SZrAstNode *propertyNode) {
    SZrPropertyDeclaration *property;

    TEST_ASSERT_NOT_NULL(propertyNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_DECLARATION, propertyNode->type);
    property = &propertyNode->data.propertyDeclaration;
    TEST_ASSERT_NOT_NULL(property->accessors);
    for (TZrSize index = 0u; index < property->accessors->count; index++) {
        SZrAstNode *accessor = property->accessors->nodes[index];
        if (accessor != ZR_NULL &&
            accessor->type == ZR_AST_PROPERTY_ACCESSOR &&
            accessor->data.propertyAccessor.kind ==
                    ZR_PROPERTY_ACCESSOR_GET) {
            return accessor;
        }
    }
    TEST_FAIL_MESSAGE("Expected property getter");
    return ZR_NULL;
}

static void assert_compile_rejected(const char *source) {
    SZrFunction *function = compile_source(source);

    if (function != ZR_NULL) {
        ZrCore_Function_Free(g_state, function);
        TEST_FAIL_MESSAGE("Expected source compilation to fail");
    }
}

static TZrUInt32 count_instruction_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode ==
            (TZrUInt16)opcode) {
            count++;
        }
    }
    return count;
}

static TZrUInt32 count_instruction_opcode_tree(
        const SZrFunction *function,
        EZrInstructionCode opcode,
        TZrUInt32 depth) {
    TZrUInt32 count = count_instruction_opcode(function, opcode);

    if (function == ZR_NULL || depth > 32U) {
        return count;
    }
    for (TZrUInt32 index = 0U; index < function->childFunctionLength; index++) {
        count += count_instruction_opcode_tree(
                &function->childFunctionList[index], opcode, depth + 1U);
    }
    for (TZrUInt32 index = 0U; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        const SZrFunction *child;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        child = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (child != function) {
            count += count_instruction_opcode_tree(
                    child, opcode, depth + 1U);
        }
    }
    return count;
}

static TZrUInt32 count_semir_opcode_tree(
        const SZrFunction *function,
        EZrSemIrOpcode opcode,
        TZrUInt32 depth) {
    TZrUInt32 count = 0U;

    if (function == ZR_NULL || depth > 32U) {
        return 0U;
    }
    if (function->semIrInstructions != ZR_NULL) {
        for (TZrUInt32 index = 0U;
             index < function->semIrInstructionLength;
             index++) {
            if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode ==
                opcode) {
                count++;
            }
        }
    }
    for (TZrUInt32 index = 0U; index < function->childFunctionLength; index++) {
        count += count_semir_opcode_tree(
                &function->childFunctionList[index], opcode, depth + 1U);
    }
    for (TZrUInt32 index = 0U; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        const SZrFunction *child;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        child = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (child != function) {
            count += count_semir_opcode_tree(
                    child, opcode, depth + 1U);
        }
    }
    return count;
}

static void fixture_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *load_binary_entry(const char *path) {
    TZrSize byteLength = 0U;
    TZrByte *bytes = ZrTests_Fixture_ReadFileBytes(path, &byteLength);
    ZrTestsFixtureReader reader;
    SZrIo *io;
    SZrIoSource *source;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, (TZrUInt32)byteLength);
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

static char *read_text_file(const char *path) {
    TZrSize length = 0U;
    TZrByte *bytes = ZrTests_Fixture_ReadFileBytes(path, &length);
    char *text;

    if (bytes == ZR_NULL) {
        return ZR_NULL;
    }
    text = (char *)malloc((size_t)length + 1U);
    if (text == ZR_NULL) {
        free(bytes);
        return ZR_NULL;
    }
    memcpy(text, bytes, (size_t)length);
    text[length] = '\0';
    free(bytes);
    return text;
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

static const SZrTypeMemberInfo *find_property_contract_member(
        const SZrTypePrototypeInfo *prototype,
        EZrPropertyAccessorRole role) {
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        if (member != ZR_NULL && member->accessorRole == role &&
            (role != ZR_PROPERTY_ACCESSOR_ROLE_NONE ||
             member->memberType == ZR_AST_PROPERTY_DECLARATION)) {
            return member;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *find_semantic_symbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *record =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (record != ZR_NULL && record->id == symbolId) {
            return record;
        }
    }
    return ZR_NULL;
}

static const SZrCanonicalTypeNode *find_canonical_type(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < context->canonicalTypes.length; index++) {
        const SZrCanonicalTypeNode *node =
                (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                        (SZrArray *)&context->canonicalTypes, index);
        if (node != ZR_NULL && node->id == typeId) {
            return node;
        }
    }
    return ZR_NULL;
}

static void compile_type_declarations(
        SZrCompilerState *compiler,
        SZrAstNode *script) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    for (TZrSize index = 0u;
         index < script->data.script.statements->count && !compiler->hasError;
         index++) {
        SZrAstNode *declaration = script->data.script.statements->nodes[index];
        TEST_ASSERT_NOT_NULL(declaration);
        if (declaration->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(compiler, declaration);
        } else if (declaration->type == ZR_AST_STRUCT_DECLARATION) {
            ZrParser_Compiler_CompileStructDeclaration(compiler, declaration);
        } else {
            TEST_FAIL_MESSAGE("Expected a class or struct declaration");
        }
    }
}

static void test_ref_property_binds_exact_canonical_contracts(void) {
    static const char source[] =
            "class MutableBox {\n"
            "  pri var stored: int;\n"
            "  pub property value: ref int { get { return ref this.stored; } }\n"
            "}\n"
            "class ReadonlyBox {\n"
            "  pri var stored: int;\n"
            "  pub property value: ref readonly int { get => ref this.stored; }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypePrototypeInfo *mutablePrototype;
    const SZrTypePrototypeInfo *readonlyPrototype;
    const SZrTypeMemberInfo *mutableProperty;
    const SZrTypeMemberInfo *mutableGetter;
    const SZrTypeMemberInfo *readonlyProperty;
    const SZrTypeMemberInfo *readonlyGetter;
    const SZrSemanticSymbolRecord *mutablePropertySymbol;
    const SZrSemanticSymbolRecord *mutableGetterSymbol;
    const SZrCanonicalTypeNode *mutableGetterType;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_type_declarations(&compiler, script);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);

    mutablePrototype = find_type_prototype(&compiler, "MutableBox");
    readonlyPrototype = find_type_prototype(&compiler, "ReadonlyBox");
    mutableProperty = find_property_contract_member(
            mutablePrototype, ZR_PROPERTY_ACCESSOR_ROLE_NONE);
    mutableGetter = find_property_contract_member(
            mutablePrototype, ZR_PROPERTY_ACCESSOR_ROLE_GET);
    readonlyProperty = find_property_contract_member(
            readonlyPrototype, ZR_PROPERTY_ACCESSOR_ROLE_NONE);
    readonlyGetter = find_property_contract_member(
            readonlyPrototype, ZR_PROPERTY_ACCESSOR_ROLE_GET);

    TEST_ASSERT_NOT_NULL(mutableProperty);
    TEST_ASSERT_NOT_NULL(mutableGetter);
    TEST_ASSERT_NOT_NULL(readonlyProperty);
    TEST_ASSERT_NOT_NULL(readonlyGetter);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            mutableProperty->structuredReturnType.referenceAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            mutableGetter->structuredReturnType.referenceAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_READONLY,
            readonlyProperty->structuredReturnType.referenceAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_READONLY,
            readonlyGetter->structuredReturnType.referenceAccess);
    TEST_ASSERT_TRUE(mutableProperty->exportsWritableRef);
    TEST_ASSERT_FALSE(readonlyProperty->exportsWritableRef);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_MUTABLE, mutableProperty->receiverEffect);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_MUTABLE, mutableGetter->receiverEffect);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY, readonlyProperty->receiverEffect);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY, readonlyGetter->receiverEffect);
    TEST_ASSERT_EQUAL_UINT64(
            mutableProperty->symbolId, mutableProperty->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            mutableProperty->symbolId, mutableGetter->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            mutableGetter->symbolId, mutableProperty->getterAccessorSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_SEMANTIC_ID_INVALID, mutableProperty->setterAccessorSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_SEMANTIC_ID_INVALID, mutableProperty->initAccessorSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            mutableProperty->propertyValueTypeId,
            ZrParser_CanonicalType_FromInferred(
                    compiler.semanticContext,
                    &mutableGetter->structuredReturnType));
    TEST_ASSERT_EQUAL_UINT64(
            readonlyProperty->propertyValueTypeId,
            ZrParser_CanonicalType_FromInferred(
                    compiler.semanticContext,
                    &readonlyGetter->structuredReturnType));
    mutablePropertySymbol = find_semantic_symbol(
            compiler.semanticContext, mutableProperty->symbolId);
    mutableGetterSymbol = find_semantic_symbol(
            compiler.semanticContext, mutableGetter->symbolId);
    TEST_ASSERT_NOT_NULL(mutablePropertySymbol);
    TEST_ASSERT_NOT_NULL(mutableGetterSymbol);
    TEST_ASSERT_EQUAL_UINT64(
            mutableProperty->propertyValueTypeId,
            mutablePropertySymbol->typeId);
    mutableGetterType = find_canonical_type(
            compiler.semanticContext, mutableGetterSymbol->typeId);
    TEST_ASSERT_NOT_NULL(mutableGetterType);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_TYPE_FUNCTION, mutableGetterType->kind);
    TEST_ASSERT_EQUAL_UINT64(
            mutableProperty->propertyValueTypeId,
            mutableGetterType->data.function.returnTypeId);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_property_rejects_invalid_getter_shapes(void) {
    assert_compile_rejected(
            "class Box { pub property value: ref int { get; } }\n");
    assert_compile_rejected(
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub property value: ref int { get { return this.stored; } }\n"
            "}\n");
    assert_compile_rejected(
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub property value: int { get { return ref this.stored; } }\n"
            "}\n");
}

static void test_block_getter_preserves_explicit_ref_return_surface(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *property = first_property(script);
    SZrAstNode *getter = property_getter(property);
    SZrAstNode *body = getter->data.propertyAccessor.body;
    SZrAstNode *returnNode;

    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            property->data.propertyDeclaration.typeInfo->referenceAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_PROPERTY_ACCESSOR_BODY_BLOCK,
            getter->data.propertyAccessor.bodyKind);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)body->data.block.body->count);
    returnNode = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(returnNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    TEST_ASSERT_NOT_NULL(returnNode->data.returnStatement.expr);
    TEST_ASSERT_TRUE(returnNode->data.returnStatement.isReferenceReturn);
    TEST_ASSERT_EQUAL_INT64(4, returnNode->data.returnStatement.referenceLocation.start.line);
    TEST_ASSERT_EQUAL_UINT64(
            3u,
            returnNode->data.returnStatement.referenceLocation.end.offset -
                    returnNode->data.returnStatement.referenceLocation.start.offset);

    ZrParser_Ast_Free(g_state, script);
}

static void test_expression_getter_accepts_explicit_ref_result(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref readonly int {\n"
            "    get => ref this.stored;\n"
            "  }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *property = first_property(script);
    SZrAstNode *getter = property_getter(property);

    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_READONLY,
            property->data.propertyDeclaration.typeInfo->referenceAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_PROPERTY_ACCESSOR_BODY_EXPRESSION,
            getter->data.propertyAccessor.bodyKind);
    TEST_ASSERT_NOT_NULL(getter->data.propertyAccessor.body);
    TEST_ASSERT_TRUE(getter->data.propertyAccessor.isReferenceResult);
    TEST_ASSERT_EQUAL_INT64(4, getter->data.propertyAccessor.referenceLocation.start.line);
    TEST_ASSERT_EQUAL_UINT64(
            3u,
            getter->data.propertyAccessor.referenceLocation.end.offset -
                    getter->data.propertyAccessor.referenceLocation.start.offset);

    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_property_rejects_set_and_init_accessors(void) {
    assert_compile_rejected(
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "    set { return; }\n"
            "  }\n"
            "}\n");
    assert_compile_rejected(
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref readonly int {\n"
            "    get { return ref this.stored; }\n"
            "    init { return; }\n"
            "  }\n"
            "}\n");
}

static void test_writable_ref_property_assignment_uses_getter_place(void) {
    static const char source[] =
            "class Box {\n"
            "  pub static var getterCalls: int = 0;\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 1; }\n"
            "  pub property value: ref int {\n"
            "    get { Box.getterCalls = Box.getterCalls + 1; return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value += 4;\n"
            "var observed = box.value;\n"
            "return Box.getterCalls * 10 + observed;\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(25, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_ref_argument_preserves_property_reference_identity(void) {
    static const char source[] =
            "fn consume(value: ref int): int { return 0; }\n"
            "class Box {\n"
            "  pub static var getterCalls: int = 0;\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 3; }\n"
            "  pub property value: ref int {\n"
            "    get { Box.getterCalls = Box.getterCalls + 1; return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "consume(ref box.value);\n"
            "return 0;\n";
    static const char readonlySource[] =
            "fn consume(value: ref int): int { return 0; }\n"
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub property value: ref readonly int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "consume(ref box.value);\n";
    SZrFunction *function = compile_source(source);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            count_instruction_opcode(
                    function, ZR_INSTRUCTION_ENUM(PROPERTY_REF_LOAD)));
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(compile_source(readonlySource));
}

static void test_static_ref_property_mutates_static_place(void) {
    static const char source[] =
            "class Counter {\n"
            "  pri static var stored: int;\n"
            "  pub static property value: ref int {\n"
            "    get { return ref Counter.stored; }\n"
            "  }\n"
            "}\n"
            "Counter.stored = 2;\n"
            "Counter.value += 3;\n"
            "var observed = Counter.value;\n"
            "return observed;\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_struct_ref_property_mutates_addressable_frame_place(void) {
    static const char source[] =
            "struct Point {\n"
            "  pri var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var point: Point = init Point(2);\n"
            "point.value += 3;\n"
            "var observed = point.value;\n"
            "return observed;\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_index_ref_property_mutates_array_element_place(void) {
    static const char source[] =
            "class Buffer {\n"
            "  pri var values: int[1];\n"
            "  pub @constructor() { this.values = [2]; }\n"
            "  pub property first: ref int {\n"
            "    get { return ref this.values[0]; }\n"
            "  }\n"
            "}\n"
            "var buffer = new Buffer();\n"
            "buffer.first += 3;\n"
            "var observed = buffer.first;\n"
            "return observed;\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_non_addressable_struct_receiver_rejects_writable_ref(void) {
    assert_compile_rejected(
            "struct Point {\n"
            "  pri var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "fn make(): Point { return init Point(1); }\n"
            "make().value += 1;\n");
}

static void test_readonly_ref_property_rejects_store(void) {
    assert_compile_rejected(
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref readonly int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value = 2;\n");
}

static void test_ref_property_rejects_local_temporary_escape(void) {
    assert_compile_rejected(
            "class Box {\n"
            "  pub property value: ref int {\n"
            "    get { var temporary: int = 1; return ref temporary; }\n"
            "  }\n"
            "}\n");
}

static void test_ref_property_override_and_interface_access_are_invariant(void) {
    assert_compile_rejected(
            "abstract class Base {\n"
            "  pub abstract property value: ref int { get; }\n"
            "}\n"
            "class Invalid : Base {\n"
            "  pri var stored: int;\n"
            "  pub override property value: ref readonly int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n");
    assert_compile_rejected(
            "interface Contract {\n"
            "  pub property value: ref readonly int { pub get; }\n"
            "}\n"
            "class Invalid : Contract {\n"
            "  pri var stored: int;\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n");
}

static void test_writable_ref_property_requires_writable_receiver(void) {
    assert_compile_rejected(
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "  pub const fn observe(): int { return this.value; }\n"
            "}\n");
}

static void test_ref_struct_property_mutates_view_frame_place(void) {
    static const char source[] =
            "ref struct CellView {\n"
            "  var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "fn run(): int {\n"
            "  var view: CellView = init CellView(2);\n"
            "  view.value += 3;\n"
            "  return view.value;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_property_ref_get_publishes_canonical_place_loan_facts(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 3; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "var observed = box.value;\n"
            "return observed;\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semanticIr;
    const SZrSemanticIrInstruction *propertyRefGet = ZR_NULL;
    const SZrSemanticIrInstruction *dereference = ZR_NULL;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_script(&compiler, script);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_PreSemanticIrIsValidated(&compiler));
    semanticIr = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semanticIr);
    for (TZrSize index = 0U; index < semanticIr->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semanticIr, index);
        if (instruction != ZR_NULL &&
            instruction->opcode == ZR_SEMANTIC_IR_PROPERTY_REF_GET) {
            propertyRefGet = instruction;
        } else if (instruction != ZR_NULL &&
                   instruction->opcode == ZR_SEMANTIC_IR_DEREFERENCE) {
            dereference = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(propertyRefGet);
    TEST_ASSERT_NOT_NULL(dereference);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, propertyRefGet->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, propertyRefGet->symbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID, propertyRefGet->accessorSymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_LOAN_ID_INVALID, propertyRefGet->loanId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_REGION_ID_INVALID, propertyRefGet->regionId);
    TEST_ASSERT_EQUAL_UINT32(propertyRefGet->loanId, dereference->loanId);
    TEST_ASSERT_EQUAL_UINT32(propertyRefGet->regionId, dereference->regionId);
    {
        const SZrParserPlace *place = ZrParser_PlaceGraph_Get(
                &semanticIr->places, dereference->placeId);
        const SZrParserPlaceProjection *projection;

        TEST_ASSERT_NOT_NULL(place);
        TEST_ASSERT_GREATER_THAN_UINT32(0U, (TZrUInt32)place->projections.length);
        projection = ZrParser_Place_ProjectionAt(
                place, place->projections.length - 1U);
        TEST_ASSERT_NOT_NULL(projection);
        TEST_ASSERT_EQUAL_INT(
                ZR_PARSER_PLACE_PROJECTION_DEREFERENCE, projection->kind);
    }

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_property_ref_owner_loan_blocks_move_drop_and_share(void) {
    static const char declaration[] =
            "resource class Counter {\n"
            "  pub var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n";
    char source[4096];

    snprintf(
            source,
            sizeof(source),
            "%sfn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  var alias: ref int = ref owner.value;\n"
            "  drop(owner);\n"
            "  return alias;\n"
            "}\nreturn run();\n",
            declaration);
    assert_compile_rejected(source);

    snprintf(
            source,
            sizeof(source),
            "%sfn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  var alias: ref int = ref owner.value;\n"
            "  var shared: Shared<Counter> = share(owner);\n"
            "  return alias;\n"
            "}\nreturn run();\n",
            declaration);
    assert_compile_rejected(source);

    snprintf(
            source,
            sizeof(source),
            "%sfn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  var alias: ref int = ref owner.value;\n"
            "  var moved: Unique<Counter> = owner;\n"
            "  return alias;\n"
            "}\nreturn run();\n",
            declaration);
    assert_compile_rejected(source);
}

static void test_property_ref_owner_loan_ends_after_value_load(void) {
    static const char source[] =
            "resource class Counter {\n"
            "  pub var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "fn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(7);\n"
            "  var observed = owner.value;\n"
            "  var moved: Unique<Counter> = owner;\n"
            "  drop(moved);\n"
            "  return observed;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
}

static void test_ref_property_virtual_dispatch_preserves_reference_place(void) {
    static const char source[] =
            "abstract class Base {\n"
            "  pub abstract property value: ref int { get; }\n"
            "}\n"
            "class Derived : Base {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 2; }\n"
            "  pub override property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var base: Base = new Derived();\n"
            "base.value += 3;\n"
            "return base.value;\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_ref_property_rejects_native_raw_pointer_projection(void) {
    assert_compile_rejected(
            "native extern(\"sample\") { fn raw(): ref int; }\n"
            "class Box {\n"
            "  pub property value: ref int {\n"
            "    get { return ref raw(); }\n"
            "  }\n"
            "}\n");
}

static void test_ref_property_binary_roundtrip_preserves_managed_reference(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 2; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value += 3;\n"
            "return box.value;\n";
    static const char binaryPath[] =
            "property_ref_return_roundtrip.zro";
    SZrFunction *sourceFunction = compile_source(source);
    SZrFunction *loadedFunction;
    TZrInt64 sourceResult = 0;
    TZrInt64 sourceQuickenedResult = 0;
    TZrInt64 loadedResult = 0;
    TZrInt64 loadedQuickenedResult = 0;

    TEST_ASSERT_NOT_NULL(sourceFunction);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_instruction_opcode_tree(
                    sourceFunction,
                    ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_MEMBER),
                    0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_instruction_opcode_tree(
                    sourceFunction,
                    ZR_INSTRUCTION_ENUM(PROPERTY_REF_LOAD),
                    0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_semir_opcode_tree(
                    sourceFunction, ZR_SEMIR_OPCODE_PROPERTY_REF_GET, 0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_semir_opcode_tree(
                    sourceFunction, ZR_SEMIR_OPCODE_DEREFERENCE, 0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_semir_opcode_tree(
                    sourceFunction, ZR_SEMIR_OPCODE_PROPERTY_REF_STORE, 0U));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state, sourceFunction, binaryPath));
    loadedFunction = load_binary_entry(binaryPath);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_instruction_opcode_tree(
                    loadedFunction,
                    ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_MEMBER),
                    0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_instruction_opcode_tree(
                    loadedFunction,
                    ZR_INSTRUCTION_ENUM(PROPERTY_REF_LOAD),
                    0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_semir_opcode_tree(
                    loadedFunction, ZR_SEMIR_OPCODE_PROPERTY_REF_GET, 0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_semir_opcode_tree(
                    loadedFunction, ZR_SEMIR_OPCODE_DEREFERENCE, 0U));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U,
            count_semir_opcode_tree(
                    loadedFunction, ZR_SEMIR_OPCODE_PROPERTY_REF_STORE, 0U));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, sourceFunction, &sourceResult));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, sourceFunction, &sourceQuickenedResult));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, loadedFunction, &loadedResult));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, loadedFunction, &loadedQuickenedResult));
    TEST_ASSERT_EQUAL_INT64(5, sourceResult);
    TEST_ASSERT_EQUAL_INT64(sourceResult, sourceQuickenedResult);
    TEST_ASSERT_EQUAL_INT64(sourceResult, loadedResult);
    TEST_ASSERT_EQUAL_INT64(sourceResult, loadedQuickenedResult);

    ZrCore_Function_Free(g_state, loadedFunction);
    ZrCore_Function_Free(g_state, sourceFunction);
    remove(binaryPath);
}

static void test_ref_property_aot_writers_keep_managed_reference_helpers(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 2; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value += 3;\n"
            "return box.value;\n";
    static const char cPath[] = "property_ref_return_aot.c";
    static const char llvmPath[] = "property_ref_return_aot.ll";
    SZrFunction *function = compile_source(source);
    char *cText;
    char *llvmText;
#if defined(ZR_PLATFORM_UNIX)
    char compileCommand[4096];
#endif

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(
            g_state, function, cPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(
            g_state, function, llvmPath));
    cText = read_text_file(cPath);
    llvmText = read_text_file(llvmPath);
    TEST_ASSERT_NOT_NULL(cText);
    TEST_ASSERT_NOT_NULL(llvmText);
    TEST_ASSERT_NOT_NULL(strstr(cText, "PropertyReference"));
    TEST_ASSERT_NOT_NULL(strstr(llvmText, "PropertyReference"));
    TEST_ASSERT_NULL(strstr(cText, "unsupported instruction opcode"));
    TEST_ASSERT_NULL(strstr(llvmText, "unsupported instruction opcode"));
#if defined(ZR_PLATFORM_UNIX)
    TEST_ASSERT_GREATER_THAN_INT(
            0,
            snprintf(
                    compileCommand,
                    sizeof(compileCommand),
                    "cc -std=c11 -fPIC "
                    "-I\"%s/zr_vm_common/include\" "
                    "-I\"%s/zr_vm_core/include\" "
                    "-I\"%s/zr_vm_library/include\" "
                    "-I\"%s/zr_vm_aot/include\" "
                    "-c property_ref_return_aot.c "
                    "-o property_ref_return_aot.o",
                    ZR_VM_TESTS_REPO_ROOT,
                    ZR_VM_TESTS_REPO_ROOT,
                    ZR_VM_TESTS_REPO_ROOT,
                    ZR_VM_TESTS_REPO_ROOT));
    TEST_ASSERT_EQUAL_INT(
            0,
            system(compileCommand));
    TEST_ASSERT_EQUAL_INT(
            0,
            system(
                    "clang -mllvm -opaque-pointers "
                    "-c property_ref_return_aot.ll "
                    "-o property_ref_return_aot_llvm.o"));
#endif

    free(cText);
    free(llvmText);
    ZrCore_Function_Free(g_state, function);
    remove(cPath);
    remove(llvmPath);
#if defined(ZR_PLATFORM_UNIX)
    remove("property_ref_return_aot.o");
    remove("property_ref_return_aot_llvm.o");
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_block_getter_preserves_explicit_ref_return_surface);
    RUN_TEST(test_expression_getter_accepts_explicit_ref_result);
    RUN_TEST(test_ref_property_binds_exact_canonical_contracts);
    RUN_TEST(test_ref_property_rejects_set_and_init_accessors);
    RUN_TEST(test_ref_property_rejects_invalid_getter_shapes);
    RUN_TEST(test_writable_ref_property_assignment_uses_getter_place);
    RUN_TEST(test_ref_argument_preserves_property_reference_identity);
    RUN_TEST(test_static_ref_property_mutates_static_place);
    RUN_TEST(test_struct_ref_property_mutates_addressable_frame_place);
    RUN_TEST(test_index_ref_property_mutates_array_element_place);
    RUN_TEST(test_non_addressable_struct_receiver_rejects_writable_ref);
    RUN_TEST(test_readonly_ref_property_rejects_store);
    RUN_TEST(test_ref_property_rejects_local_temporary_escape);
    RUN_TEST(test_ref_property_override_and_interface_access_are_invariant);
    RUN_TEST(test_writable_ref_property_requires_writable_receiver);
    RUN_TEST(test_ref_struct_property_mutates_view_frame_place);
    RUN_TEST(test_property_ref_get_publishes_canonical_place_loan_facts);
    RUN_TEST(test_property_ref_owner_loan_blocks_move_drop_and_share);
    RUN_TEST(test_property_ref_owner_loan_ends_after_value_load);
    RUN_TEST(test_ref_property_virtual_dispatch_preserves_reference_place);
    RUN_TEST(test_ref_property_rejects_native_raw_pointer_projection);
    RUN_TEST(test_ref_property_binary_roundtrip_preserves_managed_reference);
    RUN_TEST(test_ref_property_aot_writers_keep_managed_reference_helpers);
    return UNITY_END();
}
