#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "compiler/compiler_internal.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/parser.h"

#define FIXTURE_ARRAY_RECORD_LIMIT 1024U

typedef enum ELifetimeMethod {
    LIFETIME_METHOD_NONE,
    LIFETIME_METHOD_MISSING_RETURN,
    LIFETIME_METHOD_VALID,
    LIFETIME_METHOD_INVALID_BODY
} ELifetimeMethod;

typedef enum EFieldProducer {
    FIELD_SOURCE_STRUCT,
    FIELD_SOURCE_CLASS,
    FIELD_EXTERN_STRUCT
} EFieldProducer;

typedef struct SArrayRecord {
    uintptr_t address;
    TZrSize size;
} SArrayRecord;

typedef struct SArrayLifetimeProbe {
    SZrGlobalState *global;
    FZrAllocator allocator;
    TZrPtr userData;
    SArrayRecord records[FIXTURE_ARRAY_RECORD_LIMIT];
    TZrSize allocations;
    TZrSize liveCount;
    TZrSize liveBytes;
    TZrBool overflow;
    TZrBool invalidRecord;
} SArrayLifetimeProbe;

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrInferredType g_child;
static TZrBool g_childInitialized;
static SArrayLifetimeProbe g_probe;

static TZrSize find_record(uintptr_t address) {
    for (TZrSize index = 0U; index < FIXTURE_ARRAY_RECORD_LIMIT; index++) {
        if (g_probe.records[index].address == address) {
            return index;
        }
    }
    return FIXTURE_ARRAY_RECORD_LIMIT;
}

static TZrPtr counting_allocator(TZrPtr userData, TZrPtr pointer,
        TZrSize oldSize, TZrSize newSize, TZrInt64 type) {
    SArrayLifetimeProbe *probe = (SArrayLifetimeProbe *)userData;
    TZrSize oldIndex = pointer != ZR_NULL ?
            find_record((uintptr_t)pointer) : FIXTURE_ARRAY_RECORD_LIMIT;
    TZrPtr result = probe->allocator(
            probe->userData, pointer, oldSize, newSize, type);

    if (oldIndex != FIXTURE_ARRAY_RECORD_LIMIT &&
        (newSize == 0U || result != ZR_NULL)) {
        SArrayRecord *record = &probe->records[oldIndex];

        if (record->size != oldSize || type != ZR_MEMORY_NATIVE_TYPE_ARRAY) {
            probe->invalidRecord = ZR_TRUE;
        }
        probe->liveCount--;
        probe->liveBytes -= record->size;
        memset(record, 0, sizeof(*record));
    }
    if (type != ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        return result;
    }
    if (newSize != 0U && result != ZR_NULL) {
        TZrSize index;

        probe->allocations++;
        if (find_record((uintptr_t)result) != FIXTURE_ARRAY_RECORD_LIMIT) {
            probe->invalidRecord = ZR_TRUE;
            return result;
        }
        index = find_record(0U);
        if (index == FIXTURE_ARRAY_RECORD_LIMIT) {
            probe->overflow = ZR_TRUE;
            return result;
        }
        probe->records[index].address = (uintptr_t)result;
        probe->records[index].size = newSize;
        probe->liveCount++;
        probe->liveBytes += newSize;
    }
    return result;
}

static void begin_probe(void) {
    memset(&g_probe, 0, sizeof(g_probe));
    g_probe.global = g_state->global;
    g_probe.allocator = g_state->global->allocator;
    g_probe.userData = g_state->global->userAllocationArguments;
    g_state->global->allocator = counting_allocator;
    g_state->global->userAllocationArguments = &g_probe;
}

static void close_fixture(void) {
    if (g_childInitialized) {
        ZrParser_InferredType_Free(g_state, &g_child);
        g_childInitialized = ZR_FALSE;
    }
    if (g_compilerInitialized) {
        if (g_compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, g_compiler.currentFunction);
            g_compiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        /* The delegate remains installed through GC/global destruction, then its owner is gone. */
        g_probe.global = ZR_NULL;
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

void setUp(void) {
    SZrScope scope = {0};

    g_state = ZR_NULL;
    g_ast = ZR_NULL;
    g_compilerInitialized = ZR_FALSE;
    g_childInitialized = ZR_FALSE;
    memset(&g_probe, 0, sizeof(g_probe));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
    g_compiler.isScriptLevel = ZR_TRUE;
    scope.parentCompiler = &g_compiler;
    ZrCore_Array_Push(g_state, &g_compiler.scopeStack, &scope);
    ZrParser_InferredType_Init(g_state, &g_child, ZR_VALUE_TYPE_OBJECT);
    g_childInitialized = ZR_TRUE;
}

void tearDown(void) {
    close_fixture();
}

static void clear_error_flags(void) {
    g_compiler.hasError = ZR_FALSE;
    g_compiler.hadRecoverableError = ZR_FALSE;
    g_compiler.errorMessage = ZR_NULL;
}

static void compile_type(SZrAstNode *node, TZrBool isClass) {
    if (isClass) {
        ZrParser_Compiler_CompileClassDeclaration(&g_compiler, node);
    } else {
        ZrParser_Compiler_CompileStructDeclaration(&g_compiler, node);
    }
}

static TZrSize compile_class_baseline(SZrAstNode *node) {
    SZrTypePrototypeInfo *prototype;
    const SZrNamedTypeBinding *binding;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, node->type);
    TEST_ASSERT_NOT_NULL(node->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT(0U, node->data.classDeclaration.members->count);
    TEST_ASSERT_NOT_NULL(node->data.classDeclaration.name);
    compile_type(node, ZR_TRUE);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    TEST_ASSERT_TRUE(g_compiler.typePrototypes.length > 0U);
    prototype = ZrCore_Array_Get(&g_compiler.typePrototypes,
            g_compiler.typePrototypes.length - 1U);
    TEST_ASSERT_EQUAL_PTR(node, prototype->declarationNode);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_CLASS, prototype->type);
    TEST_ASSERT_EQUAL_UINT(0U, prototype->members.length);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            node->data.classDeclaration.name->name, ZrParser_SyntaxContext_Root());
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_PTR(node, binding->declarationNode);
    return g_compiler.typePrototypes.length;
}

static void assert_array_parameter(SZrInferredType *type) {
    const SZrInferredType *element;

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, type->baseType);
    TEST_ASSERT_EQUAL_UINT(1U, type->elementTypes.length);
    element = ZrCore_Array_Get(&type->elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(element);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, element->baseType);
}

static void assert_probe_drained(void) {
    close_fixture();
    TEST_ASSERT_FALSE_MESSAGE(g_probe.overflow, "ARRAY ledger capacity is a fixture prerequisite");
    TEST_ASSERT_FALSE_MESSAGE(g_probe.invalidRecord, "ARRAY sizes and addresses must pair correctly");
    TEST_ASSERT_TRUE(g_probe.allocations > 0U);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0U, g_probe.liveCount,
            "published or rejected declaration buffers must be released by their owners");
    TEST_ASSERT_EQUAL_UINT64(0U, g_probe.liveBytes);
}

static void assert_published_type(SZrAstNode *node, TZrBool isClass,
        TZrSize prototypeCount, TZrBool hasArrayMethod) {
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *member;

    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    TEST_ASSERT_EQUAL_UINT(prototypeCount, g_compiler.typePrototypes.length);
    prototype = ZrCore_Array_Get(&g_compiler.typePrototypes, prototypeCount - 1U);
    TEST_ASSERT_EQUAL_PTR(node, prototype->declarationNode);
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_OBJECT_PROTOTYPE_TYPE_CLASS :
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, prototype->type);
    TEST_ASSERT_EQUAL_UINT(hasArrayMethod ? 2U : 1U, prototype->members.length);
    member = ZrCore_Array_Get(&prototype->members, 0U);
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_FIELD : ZR_AST_STRUCT_FIELD,
            member->memberType);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(member->name));
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(member->fieldTypeName));
    if (hasArrayMethod) {
        SZrAstNodeArray *nodes = isClass ? node->data.classDeclaration.members :
                node->data.structDeclaration.members;

        member = ZrCore_Array_Get(&prototype->members, 1U);
        TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_METHOD : ZR_AST_STRUCT_METHOD,
                member->memberType);
        TEST_ASSERT_EQUAL_PTR(nodes->nodes[1], member->declarationNode);
        TEST_ASSERT_NOT_NULL(member->compiledFunction);
        TEST_ASSERT_TRUE(member->hasStructuredReturnType);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, member->structuredReturnType.baseType);
        TEST_ASSERT_EQUAL_UINT(1U, member->parameterTypes.length);
        TEST_ASSERT_EQUAL_UINT(1U, member->parameterPassingModes.length);
        assert_array_parameter(ZrCore_Array_Get(&member->parameterTypes, 0U));
    }
}

static void assert_source_lifetime_details(const TZrChar *source, TZrBool isClass,
        ELifetimeMethod methodKind, TZrBool abstractFinal, TZrBool recovery,
        TZrBool arrayParameter) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "unpublished_type_lifetime.zr");
    SZrAstNodeArray *statements;
    SZrAstNode *node;
    SZrAstNodeArray *members;
    SZrType *fieldType;
    TZrBool missingReturn = methodKind == LIFETIME_METHOD_MISSING_RETURN;
    TZrBool hasMethod = methodKind != LIFETIME_METHOD_NONE;
    TZrBool invalidBody = methodKind == LIFETIME_METHOD_INVALID_BODY;
    TZrBool invalid = missingReturn || abstractFinal || invalidBody;
    TZrSize subjectIndex = isClass ? 1U : 0U;
    TZrSize baselineCount = 0U;

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    statements = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    TEST_ASSERT_EQUAL_UINT(subjectIndex + 1U + (recovery ? 1U : 0U), statements->count);
    g_compiler.scriptAst = g_ast;
    if (isClass) {
        baselineCount = compile_class_baseline(statements->nodes[0]);
    }
    node = statements->nodes[subjectIndex];
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_DECLARATION : ZR_AST_STRUCT_DECLARATION,
            node->type);
    members = isClass ? node->data.classDeclaration.members :
            node->data.structDeclaration.members;
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_EQUAL_UINT(hasMethod ? 2U : 1U, members->count);
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_FIELD : ZR_AST_STRUCT_FIELD,
            members->nodes[0]->type);
    fieldType = isClass ? members->nodes[0]->data.classField.typeInfo :
            members->nodes[0]->data.structField.typeInfo;
    TEST_ASSERT_NOT_NULL(fieldType);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, fieldType, &g_child));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_child.baseType);
    ZrParser_InferredType_Free(g_state, &g_child);
    if (hasMethod) {
        SZrAstNode *method = members->nodes[1];
        SZrType *returnType;
        SZrType *parameterType;
        SZrAstNodeArray *params;

        TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_METHOD : ZR_AST_STRUCT_METHOD,
                method->type);
        returnType = isClass ? method->data.classMethod.returnType :
                method->data.structMethod.returnType;
        params = isClass ? method->data.classMethod.params : method->data.structMethod.params;
        TEST_ASSERT_NOT_NULL(params);
        TEST_ASSERT_EQUAL_UINT(1U, params->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, params->nodes[0]->type);
        parameterType = params->nodes[0]->data.parameter.typeInfo;
        TEST_ASSERT_NOT_NULL(parameterType);
        TEST_ASSERT_EQUAL_INT(arrayParameter ? 1 : 0, parameterType->dimensions);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
                parameterType, &g_child));
        if (arrayParameter) {
            assert_array_parameter(&g_child);
        } else {
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_child.baseType);
        }
        ZrParser_InferredType_Free(g_state, &g_child);
        TEST_ASSERT_NOT_NULL(returnType);
        TEST_ASSERT_EQUAL_INT(!missingReturn, ZrParser_AstTypeToInferredType_Convert(
                &g_compiler, returnType, &g_child));
        TEST_ASSERT_EQUAL_INT(missingReturn, g_compiler.hasError);
        if (missingReturn) {
            TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage,
                    "requires an explicit module qualifier"));
        } else {
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_child.baseType);
        }
        ZrParser_InferredType_Free(g_state, &g_child);
        clear_error_flags();
        if (invalidBody) {
            SZrAstNode *body = isClass ? method->data.classMethod.body :
                    method->data.structMethod.body;

            TEST_ASSERT_NOT_NULL(body);
            TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
            TEST_ASSERT_NOT_NULL(body->data.block.body);
            TEST_ASSERT_EQUAL_UINT(1U, body->data.block.body->count);
            TEST_ASSERT_EQUAL_INT(ZR_AST_BREAK_CONTINUE_STATEMENT,
                    body->data.block.body->nodes[0]->type);
            TEST_ASSERT_TRUE(body->data.block.body->nodes[0]->data.breakContinueStatement.isBreak);
        }
    }
    if (abstractFinal) {
        TEST_ASSERT_TRUE(isClass);
        TEST_ASSERT_TRUE(node->data.classDeclaration.modifierFlags &
                ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_TRUE(node->data.classDeclaration.modifierFlags &
                ZR_DECLARATION_MODIFIER_FINAL);
    }
    begin_probe();
    compile_type(node, isClass);
    if (invalid) {
        TEST_ASSERT_TRUE(g_compiler.hasError);
        TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, abstractFinal ?
                "abstract final class is not allowed" : invalidBody ?
                "break statement not inside a loop" : "requires an explicit module qualifier"));
        TEST_ASSERT_EQUAL_UINT(baselineCount, g_compiler.typePrototypes.length);
        TEST_ASSERT_NULL(g_compiler.currentTypeName);
        TEST_ASSERT_NULL(g_compiler.currentTypeNode);
        TEST_ASSERT_NULL(g_compiler.currentTypePrototypeInfo);
        if (recovery) {
            clear_error_flags();
            compile_type(statements->nodes[subjectIndex + 1U], isClass);
            assert_published_type(statements->nodes[subjectIndex + 1U],
                    isClass, baselineCount + 1U, ZR_FALSE);
        }
    } else {
        assert_published_type(node, isClass, baselineCount + 1U, hasMethod);
    }
    assert_probe_drained();
}

static void compile_field_owner(SZrAstNode *node, EFieldProducer producer) {
    if (producer == FIELD_EXTERN_STRUCT) {
        extern_compiler_register_struct_prototype(&g_compiler, node);
    } else {
        compile_type(node, producer == FIELD_SOURCE_CLASS);
    }
}

static void assert_field_capture_lifetime(EFieldProducer producer,
        TZrBool missingReturn, TZrBool recovery, TZrBool abstractFinal) {
    TZrChar source[768];
    SZrString *name;
    SZrAstNodeArray *statements;
    SZrAstNodeArray *declarations;
    SZrAstNodeArray *members;
    SZrAstNode *owner;
    SZrType *priorType;
    SZrType *callbackType;
    SZrFunctionType *signature;
    TZrBool isClass = producer == FIELD_SOURCE_CLASS;
    TZrBool isExtern = producer == FIELD_EXTERN_STRUCT;
    TZrBool rejected = missingReturn || abstractFinal;
    TZrSize ownerIndex = isClass ? 1U : 0U;
    TZrSize baselineCount;
    int length = snprintf(source, sizeof(source),
            "%s%s%s Holder { pub var prior: int[]; pub var callback: fn(int[]) -> %s; } %s%s",
            isExtern ? "native extern(\"fixture\") { " : isClass ? "class Baseline {} " : "",
            abstractFinal ? "abstract final " : "", isClass ? "class" : "struct",
            missingReturn ? "Missing" : "int",
            recovery ? (isClass ? "class Recovery { pub var value: int; }" :
                    "struct Recovery { pub var value: int; }") : "", isExtern ? " }" : "");

    TEST_ASSERT_TRUE(length > 0 && (TZrSize)length < sizeof(source));
    TEST_ASSERT_FALSE(abstractFinal && !isClass);
    TEST_ASSERT_FALSE(recovery && !missingReturn);
    name = ZrCore_String_CreateFromNative(g_state, "field_capture_lifetime.zr");
    g_ast = ZrParser_Parse(g_state, source, (TZrSize)length, name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    statements = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    g_compiler.scriptAst = g_ast;
    declarations = statements;
    if (isExtern) {
        TEST_ASSERT_EQUAL_UINT(1U, statements->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, statements->nodes[0]->type);
        declarations = statements->nodes[0]->data.externBlock.declarations;
        TEST_ASSERT_NOT_NULL(declarations);
    }
    TEST_ASSERT_EQUAL_UINT(ownerIndex + 1U + (recovery ? 1U : 0U), declarations->count);
    if (isClass) {
        (void)compile_class_baseline(declarations->nodes[0]);
    }
    baselineCount = g_compiler.typePrototypes.length;
    owner = declarations->nodes[ownerIndex];
    TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_DECLARATION : ZR_AST_STRUCT_DECLARATION,
            owner->type);
    members = isClass ? owner->data.classDeclaration.members :
            owner->data.structDeclaration.members;
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_EQUAL_UINT(2U, members->count);
    for (TZrSize index = 0U; index < members->count; index++) {
        TEST_ASSERT_EQUAL_INT(isClass ? ZR_AST_CLASS_FIELD : ZR_AST_STRUCT_FIELD,
                members->nodes[index]->type);
    }
    priorType = isClass ? members->nodes[0]->data.classField.typeInfo :
            members->nodes[0]->data.structField.typeInfo;
    callbackType = isClass ? members->nodes[1]->data.classField.typeInfo :
            members->nodes[1]->data.structField.typeInfo;
    TEST_ASSERT_NOT_NULL(priorType);
    TEST_ASSERT_EQUAL_INT(1, priorType->dimensions);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler, priorType, &g_child));
    assert_array_parameter(&g_child);
    ZrParser_InferredType_Free(g_state, &g_child);
    TEST_ASSERT_NOT_NULL(callbackType);
    TEST_ASSERT_NOT_NULL(callbackType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_TYPE, callbackType->name->type);
    signature = &callbackType->name->data.functionType;
    TEST_ASSERT_NOT_NULL(signature->params);
    TEST_ASSERT_EQUAL_UINT(1U, signature->params->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, signature->params->nodes[0]->type);
    TEST_ASSERT_NOT_NULL(signature->params->nodes[0]->data.parameter.typeInfo);
    TEST_ASSERT_EQUAL_INT(1, signature->params->nodes[0]->data.parameter.typeInfo->dimensions);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(&g_compiler,
            signature->params->nodes[0]->data.parameter.typeInfo, &g_child));
    assert_array_parameter(&g_child);
    ZrParser_InferredType_Free(g_state, &g_child);
    TEST_ASSERT_EQUAL_INT(!missingReturn, ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, signature->returnType, &g_child));
    TEST_ASSERT_EQUAL_INT(missingReturn, g_compiler.hasError);
    if (missingReturn) {
        TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "requires an explicit module qualifier"));
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_child.baseType);
    }
    ZrParser_InferredType_Free(g_state, &g_child);
    clear_error_flags();
    TEST_ASSERT_EQUAL_INT(!missingReturn, ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, callbackType, &g_child));
    TEST_ASSERT_EQUAL_INT(missingReturn, g_compiler.hasError);
    if (!missingReturn) {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, g_child.baseType);
    } else {
        TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "requires an explicit module qualifier"));
    }
    ZrParser_InferredType_Free(g_state, &g_child);
    clear_error_flags();
    if (abstractFinal) {
        TEST_ASSERT_TRUE(owner->data.classDeclaration.modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_TRUE(owner->data.classDeclaration.modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);
    }
    begin_probe();
    compile_field_owner(owner, producer);
    TEST_ASSERT_EQUAL_INT(rejected, g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(baselineCount + (rejected ? 0U : 1U), g_compiler.typePrototypes.length);
    if (rejected) {
        TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, abstractFinal ?
                "abstract final class is not allowed" : "requires an explicit module qualifier"));
        TEST_ASSERT_NULL(g_compiler.currentTypeName);
        TEST_ASSERT_NULL(g_compiler.currentTypeNode);
        TEST_ASSERT_NULL(g_compiler.currentTypePrototypeInfo);
    } else {
        SZrTypePrototypeInfo *prototype = ZrCore_Array_Get(
                &g_compiler.typePrototypes, baselineCount);
        SZrTypeMemberInfo *member;

        TEST_ASSERT_EQUAL_PTR(owner, prototype->declarationNode);
        TEST_ASSERT_EQUAL_UINT(2U, prototype->members.length);
        member = ZrCore_Array_Get(&prototype->members, 0U);
        TEST_ASSERT_EQUAL_PTR(priorType, member->fieldType);
        TEST_ASSERT_TRUE(member->hasStructuredReturnType);
        assert_array_parameter(&member->structuredReturnType);
        TEST_ASSERT_NULL(member->returnTypeName);
        member = ZrCore_Array_Get(&prototype->members, 1U);
        TEST_ASSERT_EQUAL_PTR(callbackType, member->fieldType);
        TEST_ASSERT_TRUE(member->hasStructuredReturnType);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, member->structuredReturnType.baseType);
        TEST_ASSERT_NULL(member->returnTypeName);
    }
    if (recovery) {
        clear_error_flags();
        compile_field_owner(declarations->nodes[ownerIndex + 1U], producer);
        assert_published_type(declarations->nodes[ownerIndex + 1U],
                isClass, baselineCount + 1U, ZR_FALSE);
    }
    assert_probe_drained();
}

static void assert_source_lifetime(const TZrChar *source, TZrBool isClass,
        TZrBool missingReturn, TZrBool abstractFinal, TZrBool recovery) {
    assert_source_lifetime_details(source, isClass, missingReturn ?
            LIFETIME_METHOD_MISSING_RETURN : LIFETIME_METHOD_NONE,
            abstractFinal, recovery, ZR_FALSE);
}

static void test_successful_struct_publication_closes_owned_arrays(void) {
    assert_source_lifetime("struct Holder { pub var value: int; }",
            ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_successful_class_publication_closes_owned_arrays(void) {
    assert_source_lifetime("class Baseline {} class Holder { pub var value: int; }",
            ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_struct_pending_method_rejection_closes_owned_arrays(void) {
    assert_source_lifetime(
            "struct Holder { pub var value: int; pub fn broken(value: int): Missing { return 1; } }",
            ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}

static void test_class_pending_method_rejection_closes_owned_arrays(void) {
    assert_source_lifetime(
            "class Baseline {}"
            "class Holder { pub var value: int; pub fn broken(value: int): Missing { return 1; } }",
            ZR_TRUE, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}

static void test_class_rejection_after_field_collection_closes_owned_arrays(void) {
    assert_source_lifetime("class Baseline {} abstract final class Holder { pub var value: int; }",
            ZR_TRUE, ZR_FALSE, ZR_TRUE, ZR_FALSE);
}

static void test_struct_error_allows_distinct_declaration_recovery_and_cleanup(void) {
    assert_source_lifetime(
            "struct Holder { pub var value: int; pub fn broken(value: int): Missing { return 1; } }"
            "struct Recovery { pub var value: int; }",
            ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_TRUE);
}

static void test_class_error_allows_distinct_declaration_recovery_and_cleanup(void) {
    assert_source_lifetime(
            "class Baseline {}"
            "class Holder { pub var value: int; pub fn broken(value: int): Missing { return 1; } }"
            "class Recovery { pub var value: int; }",
            ZR_TRUE, ZR_TRUE, ZR_FALSE, ZR_TRUE);
}

static void test_struct_pending_array_parameter_is_released(void) {
    assert_source_lifetime_details(
            "struct Holder { pub var value: int; pub fn broken(value: int[]): Missing { return 1; } }",
            ZR_FALSE, LIFETIME_METHOD_MISSING_RETURN, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_class_pending_array_parameter_is_released(void) {
    assert_source_lifetime_details(
            "class Baseline {}"
            "class Holder { pub var value: int; pub fn broken(value: int[]): Missing { return 1; } }",
            ZR_TRUE, LIFETIME_METHOD_MISSING_RETURN, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_successful_struct_method_owns_array_parameter(void) {
    assert_source_lifetime_details(
            "struct Holder { pub var value: int; pub fn read(value: int[]): int { return 1; } }",
            ZR_FALSE, LIFETIME_METHOD_VALID, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_successful_class_method_owns_array_parameter(void) {
    assert_source_lifetime_details(
            "class Baseline {}"
            "class Holder { pub var value: int; pub fn read(value: int[]): int { return 1; } }",
            ZR_TRUE, LIFETIME_METHOD_VALID, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_class_rejection_releases_pushed_method_parameter(void) {
    assert_source_lifetime_details(
            "class Baseline {} abstract final class Holder {"
            "pub var value: int; pub fn read(value: int[]): int { return 1; } }",
            ZR_TRUE, LIFETIME_METHOD_VALID, ZR_TRUE, ZR_FALSE, ZR_TRUE);
}

static void test_struct_body_rejection_releases_array_parameter(void) {
    assert_source_lifetime_details(
            "struct Holder { pub var value: int; pub fn broken(value: int[]): int { break; } }",
            ZR_FALSE, LIFETIME_METHOD_INVALID_BODY, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_class_body_rejection_releases_array_parameter(void) {
    assert_source_lifetime_details(
            "class Baseline {}"
            "class Holder { pub var value: int; pub fn broken(value: int[]): int { break; } }",
            ZR_TRUE, LIFETIME_METHOD_INVALID_BODY, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_struct_array_field_publication_closes_owned_arrays(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_STRUCT, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}
static void test_class_array_field_publication_closes_owned_arrays(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_CLASS, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}
static void test_extern_array_field_publication_closes_owned_arrays(void) {
    assert_field_capture_lifetime(FIELD_EXTERN_STRUCT, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}
static void test_struct_invalid_field_annotation_releases_prior_array(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_STRUCT, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}
static void test_class_invalid_field_annotation_releases_prior_array(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_CLASS, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}
static void test_extern_invalid_field_annotation_releases_prior_array(void) {
    assert_field_capture_lifetime(FIELD_EXTERN_STRUCT, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}
static void test_struct_invalid_field_allows_recovery(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_STRUCT, ZR_TRUE, ZR_TRUE, ZR_FALSE);
}
static void test_class_invalid_field_allows_recovery(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_CLASS, ZR_TRUE, ZR_TRUE, ZR_FALSE);
}
static void test_extern_invalid_field_allows_recovery(void) {
    assert_field_capture_lifetime(FIELD_EXTERN_STRUCT, ZR_TRUE, ZR_TRUE, ZR_FALSE);
}
static void test_class_abstract_final_releases_captured_array_field(void) {
    assert_field_capture_lifetime(FIELD_SOURCE_CLASS, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_successful_struct_publication_closes_owned_arrays);
    RUN_TEST(test_successful_class_publication_closes_owned_arrays);
    RUN_TEST(test_struct_pending_method_rejection_closes_owned_arrays);
    RUN_TEST(test_class_pending_method_rejection_closes_owned_arrays);
    RUN_TEST(test_class_rejection_after_field_collection_closes_owned_arrays);
    RUN_TEST(test_struct_error_allows_distinct_declaration_recovery_and_cleanup);
    RUN_TEST(test_class_error_allows_distinct_declaration_recovery_and_cleanup);
    RUN_TEST(test_struct_pending_array_parameter_is_released);
    RUN_TEST(test_class_pending_array_parameter_is_released);
    RUN_TEST(test_successful_struct_method_owns_array_parameter);
    RUN_TEST(test_successful_class_method_owns_array_parameter);
    RUN_TEST(test_class_rejection_releases_pushed_method_parameter);
    RUN_TEST(test_struct_body_rejection_releases_array_parameter);
    RUN_TEST(test_class_body_rejection_releases_array_parameter);
    RUN_TEST(test_struct_array_field_publication_closes_owned_arrays);
    RUN_TEST(test_class_array_field_publication_closes_owned_arrays);
    RUN_TEST(test_extern_array_field_publication_closes_owned_arrays);
    RUN_TEST(test_struct_invalid_field_annotation_releases_prior_array);
    RUN_TEST(test_class_invalid_field_annotation_releases_prior_array);
    RUN_TEST(test_extern_invalid_field_annotation_releases_prior_array);
    RUN_TEST(test_struct_invalid_field_allows_recovery);
    RUN_TEST(test_class_invalid_field_allows_recovery);
    RUN_TEST(test_extern_invalid_field_allows_recovery);
    RUN_TEST(test_class_abstract_final_releases_captured_array_field);
    return UNITY_END();
}
