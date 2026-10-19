#ifndef ZR_TEST_SEMANTIC_ANALYZER_PROTOTYPE_IDENTITY_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_PROTOTYPE_IDENTITY_CASES_H

#include "parser/parser_range_evidence.h"
#include "compiler/compiler_internal.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_library/native_binding.h"

static void test_semantic_analyzer_bootstrap_preserves_extern_prototype_identity(SZrState *state) {
    const char *summary = "Semantic Analyzer Bootstrap Preserves Extern Prototype Identity";
    SZrTestTimer timer;
    SZrTestDeclarationContextFixture fixture = {0};
    SZrAstNode *declaration;
    SZrAstNode *marker;
    SZrCompilerState *compiler;
    const SZrTypePrototypeInfo *prototype;
    TZrSize count;
    TZrBool ready = ZR_FALSE;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    if (!declaration_context_fixture_init(state, &fixture,
            "native extern(\"fixture\") { struct NativePoint { var x: i32; } }\n"
            "var marker = 1;\n", ZR_FALSE)) {
        goto cleanup;
    }
    declaration = declaration_context_node(&fixture, 0U);
    marker = declaration_context_node(&fixture, 1U);
    if (fixture.ast->data.script.statements->count != 2U ||
        declaration == ZR_NULL || declaration->type != ZR_AST_STRUCT_DECLARATION ||
        declaration->data.structDeclaration.name == ZR_NULL ||
        !declaration_context_text(declaration->data.structDeclaration.name->name, "NativePoint") ||
        marker == ZR_NULL || marker->type != ZR_AST_VARIABLE_DECLARATION ||
        fixture.ast->ownership == ZR_NULL || declaration->ownership != fixture.ast->ownership ||
        marker->ownership != fixture.ast->ownership ||
        !ZrParser_RangeEvidence_Validate(declaration->ownership, declaration->location)) {
        goto cleanup;
    }
    ready = ZR_TRUE;
    if (!declaration_context_prepare(state, &fixture)) {
        goto cleanup;
    }
    compiler = fixture.analyzer->compilerState;
    count = compiler->typePrototypes.length;
    prototype = count == 1U ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
    passed = prototype != ZR_NULL && prototype->declarationNode == declaration &&
             prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
             declaration_context_text(prototype->name, "NativePoint");
    for (TZrSize attempt = 0U; attempt < 2U; attempt++) {
        ZrParser_Compiler_PredeclareExternBindings(compiler, fixture.ast->data.script.statements);
        prototype = compiler->typePrototypes.length == 1U
                        ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
        passed = !compiler->hasError && compiler->externBindingsPredeclared &&
                 compiler->typePrototypes.length == count && prototype != ZR_NULL &&
                 prototype->declarationNode == declaration && passed;
    }

cleanup:
    declaration_context_fixture_free(state, &fixture);
    if (!ready) {
        TEST_FAIL(timer, summary, "Fixture prerequisite: real extern prototype identity AST/state unavailable");
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Bootstrap prototypes must retain the real extern declaration across same-AST predeclaration");
    } else {
        TEST_PASS(timer, summary);
    }
}

static TZrBool prototype_identity_owned_node(
        const SZrTestDeclarationContextFixture *fixture,
        const SZrAstNode *node, EZrAstNodeType kind) {
    return fixture->ast != ZR_NULL && fixture->ast->ownership != ZR_NULL &&
           node != ZR_NULL && node->type == kind &&
           node->ownership == fixture->ast->ownership &&
           ZrParser_RangeEvidence_Validate(node->ownership, node->location);
}

static TZrBool prototype_identity_parse_metadata_fixture(
        SZrState *state, SZrTestDeclarationContextFixture *fixture,
        const char *source, EZrAstNodeType kind, const char *name,
        SZrAstNode **outDeclaration) {
    SZrAstNode *marker;
    SZrIdentifier *identifier;

    *outDeclaration = ZR_NULL;
    if (!declaration_context_fixture_init(state, fixture, source, ZR_FALSE)) {
        return ZR_FALSE;
    }
    *outDeclaration = declaration_context_node(fixture, 0U);
    marker = declaration_context_node(fixture, 1U);
    identifier = declaration_context_identifier(*outDeclaration);
    return fixture->ast->data.script.statements->count == 2U &&
           prototype_identity_owned_node(fixture, *outDeclaration, kind) &&
           prototype_identity_owned_node(fixture, marker, ZR_AST_VARIABLE_DECLARATION) &&
           identifier != ZR_NULL && declaration_context_text(identifier->name, name);
}

static TZrBool prototype_identity_enum_on_is_seven(
        SZrState *state, const SZrTypePrototypeInfo *prototype) {
    const SZrTypeValue *members;
    const SZrTypeValue *on;
    SZrObject *membersObject;

    if (!prototype->hasDecoratorMetadata ||
        prototype->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
        prototype->decoratorMetadataValue.value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    members = ZrLib_Object_GetFieldCString(state,
            ZR_CAST_OBJECT(state, prototype->decoratorMetadataValue.value.object),
            "__zr_enumMembers");
    if (members == ZR_NULL || members->type != ZR_VALUE_TYPE_OBJECT ||
        members->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    membersObject = ZR_CAST_OBJECT(state, members->value.object);
    on = ZrLib_Object_GetFieldCString(state, membersObject, "On");
    return on != ZR_NULL && ZR_VALUE_IS_TYPE_SIGNED_INT(on->type) &&
           on->value.nativeObject.nativeInt64 == 7;
}

static void test_semantic_analyzer_bootstrap_extern_enum_preserves_runtime_metadata(SZrState *state) {
    const char *summary = "Semantic Analyzer Bootstrap Extern Enum Preserves Runtime Metadata";
    SZrTestTimer timer;
    SZrTestDeclarationContextFixture fixture = {0};
    SZrAstNode *declaration;
    SZrAstNode *onAst;
    SZrCompilerState *compiler;
    const SZrTypePrototypeInfo *prototype;
    const SZrTypeMemberInfo *member;
    TZrSize count;
    TZrBool ready = ZR_FALSE;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    if (!prototype_identity_parse_metadata_fixture(state, &fixture,
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.underlying(\"i32\")#\n"
            "    enum Mode { #zr.ffi.value(7)# On }\n"
            "}\nvar marker = 1;\n",
            ZR_AST_ENUM_DECLARATION, "Mode", &declaration)) {
        goto cleanup;
    }
    if (declaration->data.enumDeclaration.members == ZR_NULL ||
        declaration->data.enumDeclaration.members->count != 1U) {
        goto cleanup;
    }
    onAst = declaration->data.enumDeclaration.members->nodes[0];
    if (!prototype_identity_owned_node(&fixture, onAst, ZR_AST_ENUM_MEMBER) ||
        onAst->data.enumMember.name == ZR_NULL ||
        !declaration_context_text(onAst->data.enumMember.name->name, "On")) {
        goto cleanup;
    }
    ready = ZR_TRUE;
    if (!declaration_context_prepare(state, &fixture)) {
        goto cleanup;
    }
    compiler = fixture.analyzer->compilerState;
    count = compiler->typePrototypes.length;
    passed = count == 1U;
    for (TZrSize attempt = 0U; attempt < 2U; attempt++) {
        ZrParser_Compiler_PredeclareExternBindings(compiler, fixture.ast->data.script.statements);
        prototype = compiler->typePrototypes.length == 1U
                        ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
        member = prototype != ZR_NULL && prototype->members.length == 1U
                        ? ZrCore_Array_Get((SZrArray *)&prototype->members, 0U) : ZR_NULL;
        passed = !compiler->hasError && compiler->externBindingsPredeclared &&
                 compiler->typePrototypes.length == count && prototype != ZR_NULL &&
                 prototype->declarationNode == declaration &&
                 prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM &&
                 declaration_context_text(prototype->enumValueTypeName, "i32") &&
                 member != ZR_NULL && member->declarationNode == onAst &&
                 declaration_context_text(member->name, "On") &&
                 declaration_context_text(member->fieldTypeName, "Mode") &&
                 prototype_identity_enum_on_is_seven(state, prototype) && passed;
    }

cleanup:
    declaration_context_fixture_free(state, &fixture);
    if (!ready) {
        TEST_FAIL(timer, summary, "Fixture prerequisite: real extern enum metadata AST unavailable");
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Extern enum bootstrap must retain producer members, underlying type, and runtime value metadata");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_semantic_analyzer_bootstrap_extern_struct_preserves_field_layout(SZrState *state) {
    const char *summary = "Semantic Analyzer Bootstrap Extern Struct Preserves Field Layout";
    SZrTestTimer timer;
    SZrTestDeclarationContextFixture fixture = {0};
    SZrAstNode *declaration;
    SZrAstNode *tagAst;
    SZrAstNode *valueAst;
    SZrCompilerState *compiler;
    const SZrTypePrototypeInfo *prototype;
    const SZrTypeMemberInfo *tag;
    const SZrTypeMemberInfo *value;
    TZrSize count;
    TZrBool ready = ZR_FALSE;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    if (!prototype_identity_parse_metadata_fixture(state, &fixture,
            "native extern(\"fixture\") {\n"
            "    struct NativePair {\n"
            "        var tag: u8;\n"
            "        #zr.ffi.offset(8)# var value: i32;\n"
            "    }\n}\nvar marker = 1;\n",
            ZR_AST_STRUCT_DECLARATION, "NativePair", &declaration)) {
        goto cleanup;
    }
    if (declaration->data.structDeclaration.members == ZR_NULL ||
        declaration->data.structDeclaration.members->count != 2U) {
        goto cleanup;
    }
    tagAst = declaration->data.structDeclaration.members->nodes[0];
    valueAst = declaration->data.structDeclaration.members->nodes[1];
    if (!prototype_identity_owned_node(&fixture, tagAst, ZR_AST_STRUCT_FIELD) ||
        !prototype_identity_owned_node(&fixture, valueAst, ZR_AST_STRUCT_FIELD) ||
        tagAst->data.structField.typeInfo == ZR_NULL ||
        valueAst->data.structField.typeInfo == ZR_NULL) {
        goto cleanup;
    }
    ready = ZR_TRUE;
    if (!declaration_context_prepare(state, &fixture)) {
        goto cleanup;
    }
    compiler = fixture.analyzer->compilerState;
    count = compiler->typePrototypes.length;
    passed = count == 1U;
    for (TZrSize attempt = 0U; attempt < 2U; attempt++) {
        ZrParser_Compiler_PredeclareExternBindings(compiler, fixture.ast->data.script.statements);
        prototype = compiler->typePrototypes.length == 1U
                        ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
        tag = prototype != ZR_NULL && prototype->members.length == 2U
                        ? ZrCore_Array_Get((SZrArray *)&prototype->members, 0U) : ZR_NULL;
        value = prototype != ZR_NULL && prototype->members.length == 2U
                        ? ZrCore_Array_Get((SZrArray *)&prototype->members, 1U) : ZR_NULL;
        passed = !compiler->hasError && compiler->externBindingsPredeclared &&
                 compiler->typePrototypes.length == count && prototype != ZR_NULL &&
                 prototype->declarationNode == declaration &&
                 prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
                 tag != ZR_NULL && value != ZR_NULL &&
                 tag->memberType == ZR_AST_STRUCT_FIELD && value->memberType == ZR_AST_STRUCT_FIELD &&
                 declaration_context_text(tag->name, "tag") && declaration_context_text(value->name, "value") &&
                 declaration_context_text(tag->fieldTypeName, "u8") &&
                 declaration_context_text(value->fieldTypeName, "i32") &&
                 tag->fieldType == tagAst->data.structField.typeInfo &&
                 value->fieldType == valueAst->data.structField.typeInfo &&
                 tag->fieldSize == (TZrUInt32)sizeof(TZrUInt8) && tag->fieldOffset == 0U &&
                 value->fieldSize == (TZrUInt32)sizeof(TZrInt32) && value->fieldOffset == 8U && passed;
    }

cleanup:
    declaration_context_fixture_free(state, &fixture);
    if (!ready) {
        TEST_FAIL(timer, summary, "Fixture prerequisite: real extern struct layout AST unavailable");
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Extern struct bootstrap must retain producer field types, sizes, and explicit offsets");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_shared_extern_struct_producer_preserves_field_declaration_identity(SZrState *state) {
    const char *summary = "Shared Extern Struct Producer Preserves Field Declaration Identity";
    SZrTestTimer timer;
    SZrTestDeclarationContextFixture fixture = {0};
    SZrCompilerState compiler = {0};
    SZrAstNode *declaration, *tagAst, *valueAst;
    const SZrTypePrototypeInfo *prototype;
    const SZrTypeMemberInfo *tag, *value;
    TZrBool initialized = ZR_FALSE, ready = ZR_FALSE, passed = ZR_FALSE;
    TEST_START(summary);
    if (!prototype_identity_parse_metadata_fixture(state, &fixture,
            "native extern(\"fixture\") {\n"
            "    struct NativePair {\n"
            "        var tag: u8;\n"
            "        #zr.ffi.offset(8)# var value: i32;\n"
            "    }\n}\nvar marker = 1;\n",
            ZR_AST_STRUCT_DECLARATION, "NativePair", &declaration)) {
        goto cleanup;
    }
    if (declaration->data.structDeclaration.members == ZR_NULL ||
        declaration->data.structDeclaration.members->count != 2U) { goto cleanup; }
    tagAst = declaration->data.structDeclaration.members->nodes[0];
    valueAst = declaration->data.structDeclaration.members->nodes[1];
    if (!prototype_identity_owned_node(&fixture, tagAst, ZR_AST_STRUCT_FIELD) ||
        !prototype_identity_owned_node(&fixture, valueAst, ZR_AST_STRUCT_FIELD) ||
        tagAst->data.structField.typeInfo == ZR_NULL ||
        valueAst->data.structField.typeInfo == ZR_NULL) { goto cleanup; }
    ZrParser_CompilerState_Init(&compiler, state);
    initialized = ZR_TRUE;
    compiler.scriptAst = fixture.ast;
    ZrParser_Compiler_PredeclareExternBindings(&compiler, fixture.ast->data.script.statements);
    prototype = compiler.typePrototypes.length == 1U
            ? ZrCore_Array_Get(&compiler.typePrototypes, 0U) : ZR_NULL;
    if (compiler.hasError || !compiler.externBindingsPredeclared || prototype == ZR_NULL ||
        prototype->declarationNode != declaration || prototype->members.length != 2U ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) { goto cleanup; }
    tag = ZrCore_Array_Get((SZrArray *)&prototype->members, 0U);
    value = ZrCore_Array_Get((SZrArray *)&prototype->members, 1U);
    if (tag == ZR_NULL || value == ZR_NULL ||
        tag->memberType != ZR_AST_STRUCT_FIELD || value->memberType != ZR_AST_STRUCT_FIELD ||
        !declaration_context_text(tag->name, "tag") || !declaration_context_text(value->name, "value") ||
        !declaration_context_text(tag->fieldTypeName, "u8") ||
        !declaration_context_text(value->fieldTypeName, "i32") ||
        tag->fieldType != tagAst->data.structField.typeInfo ||
        value->fieldType != valueAst->data.structField.typeInfo ||
        tag->fieldSize != (TZrUInt32)sizeof(TZrUInt8) || tag->fieldOffset != 0U ||
        value->fieldSize != (TZrUInt32)sizeof(TZrInt32) || value->fieldOffset != 8U) { goto cleanup; }
    ready = ZR_TRUE;
    passed = tag->declarationNode == tagAst && value->declarationNode == valueAst;
cleanup:
    if (initialized) { ZrParser_CompilerState_Free(&compiler); }
    declaration_context_fixture_free(state, &fixture);
    if (!ready) {
        TEST_FAIL(timer, summary, "Fixture prerequisite: normal extern struct producer metadata unavailable");
    } else if (!passed) {
        TEST_FAIL(timer, summary, "Shared extern struct members must retain real field declaration identity");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_semantic_analyzer_failed_extern_completion_retains_bootstrap_metadata(SZrState *state) {
    const char *summary = "Semantic Analyzer Failed Extern Completion Retains Bootstrap Metadata";
    SZrTestTimer timer;
    SZrTestDeclarationContextFixture fixture = {0};
    SZrAstNode *declaration, *tagAst, *missingAst;
    SZrCompilerState *compiler;
    const SZrTypePrototypeInfo *prototype, *oldSlot;
    const SZrTypeMemberInfo *tag, *missing;
    SZrTypePrototypeInfo oldPrototype;
    SZrTypeMemberInfo oldMembers[2];
    TZrBytePtr oldSlots;
    TZrBool ready = ZR_FALSE, passed = ZR_FALSE;

    TEST_START(summary);
    if (!prototype_identity_parse_metadata_fixture(state, &fixture,
            "native extern(\"fixture\") { struct BrokenNativePair {\n"
            "    var tag: u8; var missing: MissingNativeFieldType;\n"
            "} }\nvar marker = 1;\n",
            ZR_AST_STRUCT_DECLARATION, "BrokenNativePair", &declaration) ||
        declaration->data.structDeclaration.members == ZR_NULL ||
        declaration->data.structDeclaration.members->count != 2U) { goto cleanup; }
    tagAst = declaration->data.structDeclaration.members->nodes[0];
    missingAst = declaration->data.structDeclaration.members->nodes[1];
    if (!prototype_identity_owned_node(&fixture, tagAst, ZR_AST_STRUCT_FIELD) ||
        !prototype_identity_owned_node(&fixture, missingAst, ZR_AST_STRUCT_FIELD) ||
        missingAst->data.structField.typeInfo == ZR_NULL ||
        !prototype_identity_owned_node(&fixture, missingAst->data.structField.typeInfo->name,
                ZR_AST_IDENTIFIER_LITERAL) ||
        !declaration_context_text(missingAst->data.structField.typeInfo->name->data.identifier.name,
                "MissingNativeFieldType") ||
        !declaration_context_prepare(state, &fixture)) { goto cleanup; }
    compiler = fixture.analyzer->compilerState;
    prototype = compiler->typePrototypes.length == 1U
            ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
    if (compiler->hasError || compiler->hadRecoverableError || compiler->errorMessage != ZR_NULL ||
        compiler->externBindingsPredeclared || prototype == ZR_NULL ||
        prototype->declarationNode != declaration || prototype->members.length != 2U ||
        prototype->members.head == ZR_NULL) { goto cleanup; }
    tag = ZrCore_Array_Get((SZrArray *)&prototype->members, 0U);
    missing = ZrCore_Array_Get((SZrArray *)&prototype->members, 1U);
    if (tag == ZR_NULL || missing == ZR_NULL || tag->declarationNode != tagAst ||
        missing->declarationNode != missingAst) { goto cleanup; }
    oldSlot = prototype;
    oldSlots = compiler->typePrototypes.head;
    memcpy(&oldPrototype, prototype, sizeof(oldPrototype));
    memcpy(oldMembers, prototype->members.head, sizeof(oldMembers));
    ready = ZR_TRUE;
    ZrParser_Compiler_PredeclareExternBindings(compiler, fixture.ast->data.script.statements);
    prototype = compiler->typePrototypes.length == 1U
            ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
    passed = compiler->hasError && compiler->hadRecoverableError && !compiler->hasFatalError &&
        !compiler->externBindingsPredeclared && compiler->errorMessage != ZR_NULL &&
        strcmp(compiler->errorMessage,
                "Unqualified type name 'MissingNativeFieldType' requires an explicit module qualifier or destructuring import") == 0 &&
        compiler->typePrototypes.head == oldSlots && prototype == oldSlot &&
        memcmp(prototype, &oldPrototype, sizeof(oldPrototype)) == 0 &&
        memcmp(prototype->members.head, oldMembers, sizeof(oldMembers)) == 0;
cleanup:
    declaration_context_fixture_free(state, &fixture);
    if (!ready) {
        TEST_FAIL(timer, summary, "Fixture prerequisite: clean real bootstrap struct and owned field metadata unavailable");
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Failed extern completion must report the missing field binding and retain the original bootstrap metadata");
    } else {
        TEST_PASS(timer, summary);
    }
}

static void test_semantic_analyzer_bootstrap_ordinary_enum_completes_same_declaration(SZrState *state) {
    const char *summary = "Semantic Analyzer Bootstrap Ordinary Enum Completes Same Declaration";
    SZrTestTimer timer;
    SZrTestDeclarationContextFixture fixture = {0};
    SZrAstNode *declaration, *onAst;
    SZrCompilerState *compiler;
    const SZrTypePrototypeInfo *prototype;
    const SZrTypeMemberInfo *member;
    TZrBool ready = ZR_FALSE, passed = ZR_FALSE;

    TEST_START(summary);
    if (!prototype_identity_parse_metadata_fixture(state, &fixture,
            "enum Mode: int { On = 7; }\nvar marker = 1;\n",
            ZR_AST_ENUM_DECLARATION, "Mode", &declaration) ||
        declaration->data.enumDeclaration.members == ZR_NULL ||
        declaration->data.enumDeclaration.members->count != 1U) { goto cleanup; }
    onAst = declaration->data.enumDeclaration.members->nodes[0];
    if (!prototype_identity_owned_node(&fixture, onAst, ZR_AST_ENUM_MEMBER)) { goto cleanup; }
    ready = ZR_TRUE;
    if (!declaration_context_prepare(state, &fixture)) { goto cleanup; }
    compiler = fixture.analyzer->compilerState;
    passed = compiler->typePrototypes.length == 1U;
    for (TZrSize attempt = 0U; attempt < 2U; attempt++) {
        compile_enum_declaration(compiler, declaration);
        prototype = compiler->typePrototypes.length == 1U
                ? ZrCore_Array_Get(&compiler->typePrototypes, 0U) : ZR_NULL;
        member = prototype != ZR_NULL && prototype->members.length == 1U
                ? ZrCore_Array_Get((SZrArray *)&prototype->members, 0U) : ZR_NULL;
        passed = !compiler->hasError && prototype != ZR_NULL &&
                prototype->declarationNode == declaration &&
                prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM &&
                declaration_context_text(prototype->enumValueTypeName, "int") &&
                member != ZR_NULL && member->declarationNode == onAst &&
                declaration_context_text(member->name, "On") &&
                prototype_identity_enum_on_is_seven(state, prototype) && passed;
    }
cleanup:
    declaration_context_fixture_free(state, &fixture);
    if (!ready) {
        TEST_FAIL(timer, summary, "Fixture prerequisite: real ordinary enum declaration and member AST unavailable");
    } else if (!passed) {
        TEST_FAIL(timer, summary,
                "Ordinary enum compilation must complete the same bootstrap declaration once with runtime metadata");
    } else {
        TEST_PASS(timer, summary);
    }
}

#endif
