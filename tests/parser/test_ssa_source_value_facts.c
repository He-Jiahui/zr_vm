#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrAstNode *compile_source(SZrCompilerState *compiler, const char *source) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "ssa_value_facts.zr");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TZrSize index;
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    for (index = 0u; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION)
            ZrParser_Compiler_CompileClassDeclaration(compiler, statement);
        else
            ZrParser_Statement_Compile(compiler, statement);
    }
    TEST_ASSERT_FALSE(compiler->hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(compiler));
    return ast;
}

static void free_source(SZrCompilerState *compiler, SZrAstNode *ast) {
    ZrCore_Function_Free(g_state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void assert_construct_requires_canonical_producer(
        const SZrCompilerState *compiler) {
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TEST_ASSERT_FALSE(compiler->preSemanticIrCfgActive);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(
            &compiler->preSemanticIr, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_source_ownership_and_borrow_facts_survive_rejected_constructor(void) {
    const char *source =
        "resource class Value {}\n"
        "var owner = own Value();\n"
        "var shared = share(owner);\n"
        "var borrowed: ref readonly Value = ref shared;\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    const SZrSemanticIrFunction *semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TZrSize index;
    TZrUInt32 uniqueCount = 0u, sharedCount = 0u, borrowedCount = 0u;
    assert_construct_requires_canonical_producer(&compiler);
    for (index = 0u; index < semantic->values.length; ++index) {
        const SZrSemanticIrValue *value = ZrParser_SemanticIr_Value(semantic, (TZrValueId)index + 1u);
        const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(compiler.semanticContext, value->typeId);
        EZrSemanticValueOwnership expected = ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN;
        if (type == ZR_NULL) continue;
        if (type->kind == ZR_CANONICAL_TYPE_REF) {
            expected = ZR_SEMANTIC_VALUE_OWNERSHIP_BORROWED;
            ++borrowedCount;
        } else if (type->kind == ZR_CANONICAL_TYPE_OWNER && type->data.owner.ownerKind == ZR_CANONICAL_OWNER_UNIQUE) {
            expected = ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE;
            ++uniqueCount;
        } else if (type->kind == ZR_CANONICAL_TYPE_OWNER && type->data.owner.ownerKind == ZR_CANONICAL_OWNER_SHARED) {
            expected = ZR_SEMANTIC_VALUE_OWNERSHIP_SHARED;
            ++sharedCount;
        } else continue;
        TEST_ASSERT_EQUAL_INT(expected, value->facts.ownership);
        TEST_ASSERT_EQUAL_UINT32(value->typeId, value->facts.typeId);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL,
                              value->facts.nullability);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, uniqueCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, sharedCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, borrowedCount);
    free_source(&compiler, ast);
}

static void test_explicit_source_drop_fact_stays_strict_but_constructor_is_rejected(void) {
    const char *source = "resource class Value {}\nvar owner = own Value();\ndrop(owner);\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    const SZrSemanticIrFunction *semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TZrSize index;
    TZrUInt32 drops = 0u;
    assert_construct_requires_canonical_producer(&compiler);
    for (index = 0u; index < semantic->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semantic, index);
        if (instruction->opcode == ZR_SEMANTIC_IR_DROP) {
            const SZrSemanticIrValue *owner =
                    ZrParser_SemanticIr_Value(semantic, instruction->valueId);
            TEST_ASSERT_NOT_NULL(owner);
            TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE,
                                  owner->facts.ownership);
            ++drops;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, drops);
    free_source(&compiler, ast);
}

static void test_weak_and_nullable_wake_remain_distinct(void) {
    const char *source = "resource class Value {}\n"
        "var owner = own Value();\nvar shared = share(owner);\n"
        "var weak = degrade(shared);\nvar revived = wake(weak);\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler, source);
    const SZrSemanticIrFunction *semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TZrSize index;
    TZrUInt32 weakCount = 0u, nullableCount = 0u;
    assert_construct_requires_canonical_producer(&compiler);
    for (index = 0u; index < semantic->values.length; ++index) {
        const SZrSemanticIrValue *value = ZrParser_SemanticIr_Value(semantic, (TZrValueId)index + 1u);
        const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(compiler.semanticContext, value->typeId);
        if (type == ZR_NULL) continue;
        if (type->kind == ZR_CANONICAL_TYPE_OWNER && type->data.owner.ownerKind == ZR_CANONICAL_OWNER_WEAK) {
            TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_OWNERSHIP_WEAK,
                                  value->facts.ownership);
            ++weakCount;
        } else if (type->kind == ZR_CANONICAL_TYPE_NULLABLE) {
            type = ZrParser_CanonicalType_Find(compiler.semanticContext, type->data.target.targetTypeId);
            if (type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_OWNER && type->data.owner.ownerKind == ZR_CANONICAL_OWNER_SHARED) {
                TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_OWNERSHIP_SHARED,
                                      value->facts.ownership);
                TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE,
                                      value->facts.nullability);
                ++nullableCount;
            }
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, weakCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, nullableCount);
    free_source(&compiler, ast);
}

static void test_straight_line_compiler_publishes_facts_before_cfg_lowering(void) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(&compiler,
            "resource class Value {}\nvar owner = own Value();\n");
    const SZrSemanticIrFunction *semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TZrSize index;
    TZrUInt32 owners = 0u;
    for (index = 0u; index < semantic->values.length; ++index) {
        const SZrSemanticIrValue *value = ZrParser_SemanticIr_Value(semantic, (TZrValueId)index + 1u);
        const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(compiler.semanticContext, value->typeId);
        if (type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_OWNER &&
            type->data.owner.ownerKind == ZR_CANONICAL_OWNER_UNIQUE) {
            TEST_ASSERT_EQUAL_UINT32(value->typeId, value->facts.typeId);
            TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE, value->facts.ownership);
            ++owners;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, owners);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    free_source(&compiler, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_source_ownership_and_borrow_facts_survive_rejected_constructor);
    RUN_TEST(test_explicit_source_drop_fact_stays_strict_but_constructor_is_rejected);
    RUN_TEST(test_weak_and_nullable_wake_remain_distinct);
    RUN_TEST(test_straight_line_compiler_publishes_facts_before_cfg_lowering);
    return UNITY_END();
}
