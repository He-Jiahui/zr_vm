#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;
static TZrUInt32 g_gc_box_drop_count;
static TZrUInt32 g_gc_box_drop_allocation_count;

static TZrInt64 count_gc_box_drop(SZrState *state) {
    SZrString *dropAllocation =
            ZrCore_String_CreateFromNative(state, "gc-box-drop-allocation");

    if (dropAllocation != ZR_NULL) {
        g_gc_box_drop_allocation_count++;
    }
    ZrCore_Gc_SafePoint(state);
    g_gc_box_drop_count++;
    return 0;
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

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "resource_unique_drop.zr");
    SZrParserState parser;
    SZrAstNode *script;

    ZrParser_State_Init(&parser, g_state, source, strlen(source), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_FALSE_MESSAGE(parser.hasError, parser.errorMessage);
    ZrParser_State_Free(&parser);
    return script;
}

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)index,
                                    (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static SZrFunction *compile_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "resource_unique_drop.zr");
    SZrAstNode *script = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    ZrParser_Ast_Free(g_state, script);
    return function;
}

static TZrBool function_contains_opcode_recursive(const SZrFunction *function,
                                                   EZrInstructionCode opcode,
                                                   TZrUInt32 depth) {
    TZrUInt32 index;

    if (function == ZR_NULL || depth > 32U) {
        return ZR_FALSE;
    }
    for (index = 0U; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }
    for (index = 0U; index < function->childFunctionLength; index++) {
        if (function_contains_opcode_recursive(&function->childFunctionList[index], opcode, depth + 1U)) {
            return ZR_TRUE;
        }
    }
    for (index = 0U; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        SZrFunction *child;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION || constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        child = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (child != function && function_contains_opcode_recursive(child, opcode, depth + 1U)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_resource_surface_builds_canonical_owner_ast(void) {
    SZrAstNode *script = parse_source(
            "resource class FileHandle {}\n"
            "var file: Unique<FileHandle> = own FileHandle();\n"
            "drop(file);\n");
    SZrAstNode *resource = script_statement(script, 0U);
    SZrAstNode *variable = script_statement(script, 1U);
    SZrAstNode *dropStatement = script_statement(script, 2U);
    SZrAstNode *dropExpression;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, resource->type);
    TEST_ASSERT_TRUE(resource->data.classDeclaration.isOwned);
    TEST_ASSERT_BITS_HIGH(ZR_DECLARATION_MODIFIER_RESOURCE,
                          resource->data.classDeclaration.modifierFlags);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, variable->type);
    TEST_ASSERT_NOT_NULL(variable->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION,
                          variable->data.variableDeclaration.value->type);
    TEST_ASSERT_TRUE(variable->data.variableDeclaration.value->data.constructExpression.isNew);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                          variable->data.variableDeclaration.value->data.constructExpression.ownershipQualifier);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE,
                          variable->data.variableDeclaration.value->data.constructExpression.builtinKind);

    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, dropStatement->type);
    dropExpression = dropStatement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(dropExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, dropExpression->type);
    TEST_ASSERT_FALSE(dropExpression->data.constructExpression.isNew);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_RELEASE,
                          dropExpression->data.constructExpression.builtinKind);

    ZrParser_Ast_Free(g_state, script);
}

static void test_resource_construction_world_is_type_directed(void) {
    SZrFunction *valid = compile_source(
            "resource class FileHandle {}\n"
            "var file: Unique<FileHandle> = own FileHandle();\n"
            "drop(file);\n");
    SZrFunction *invalidNew = compile_source(
            "resource class FileHandle {}\n"
            "var file = new FileHandle();\n");
    SZrFunction *invalidOwn = compile_source(
            "class Document {}\n"
            "var document = own Document();\n");

    TEST_ASSERT_NOT_NULL(valid);
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(valid, ZR_INSTRUCTION_ENUM(OWN_UNIQUE), 0U));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(valid, ZR_INSTRUCTION_ENUM(OWN_RELEASE), 0U));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(valid, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED), 0U));
    TEST_ASSERT_NULL(invalidNew);
    TEST_ASSERT_NULL(invalidOwn);
    ZrCore_Function_Free(g_state, valid);
}

static void test_resource_contextual_tokens_preserve_identifier_calls(void) {
    SZrFunction *function = compile_source(
            "own(value: int): int { return value; }\n"
            "pub resource(value: int): int { return value + 1; }\n"
            "return own(2) + resource(3);\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_unique_uses_direct_owner_without_control_block(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "DirectResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrObject *object;
    SZrTypeValue owner;

    TEST_ASSERT_NOT_NULL(prototype);
    prototype->modifierFlags |= ZR_DECLARATION_MODIFIER_RESOURCE;
    object = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    ZrCore_Value_ResetAsNull(&owner);

    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state, &owner, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_UNIQUE, owner.ownershipKind);
    TEST_ASSERT_NULL(owner.ownershipControl);
    TEST_ASSERT_NULL(object->super.ownershipControl);

    ZrCore_Ownership_ReleaseValue(g_state, &owner);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(owner.type));
}

static void test_resource_unique_into_gc_box_consumes_owner_and_defers_drop(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "GcBoxResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0u);
    SZrObject *object;
    SZrTypeValue unique;
    SZrTypeValue boxed;
    SZrGcRootHandle keepAlive;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(destructor);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    destructor->nativeFunction = count_gc_box_drop;
    ZrCore_RawObject_MarkAsPermanent(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_ObjectPrototype_AddMeta(
            g_state,
            prototype,
            ZR_META_DESTRUCTOR,
            ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor)));

    object = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    ZrCore_Value_ResetAsNull(&unique);
    ZrCore_Value_ResetAsNull(&boxed);
    g_gc_box_drop_count = 0u;
    g_gc_box_drop_allocation_count = 0u;

    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state, &unique, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_TRUE(ZrCore_Ownership_IntoGcBoxValue(g_state, &boxed, &unique));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(unique.type));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, boxed.ownershipKind);
    TEST_ASSERT_TRUE(ZrCore_Ownership_IsGcBoxObject(boxed.value.object));
    TEST_ASSERT_EQUAL_UINT32(
            0u, (TZrUInt32)ZrCore_GcDomain_GetOwnershipRootCount(g_state));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_state, boxed.value.object, &keepAlive));

    ZrCore_Value_ResetAsNull(&boxed);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(0u, g_gc_box_drop_count);

    ZrCore_GcRootHandle_Release(g_state, &keepAlive);
    TEST_ASSERT_EQUAL_UINT32(
            0u, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(1u, g_gc_box_drop_count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_gc_box_drop_allocation_count);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(1u, g_gc_box_drop_count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_gc_box_drop_allocation_count);
}

static void test_aot_own_detach_consumes_resource_unique_into_gc_box(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "AotGcBoxResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrObject *object;
    SZrFunction function;
    ZrAotGeneratedFrame frame;
    TZrStackValuePointer slotBase;
    SZrTypeValue *destination;
    SZrTypeValue *source;

    TEST_ASSERT_NOT_NULL(prototype);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    object = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);

    slotBase = ZrCore_Function_CheckStackAndGc(
            g_state, 2u, g_state->stackTop.valuePointer);
    TEST_ASSERT_NOT_NULL(slotBase);
    g_state->stackTop.valuePointer = slotBase + 2u;
    destination = ZrCore_Stack_GetValue(slotBase);
    source = ZrCore_Stack_GetValue(slotBase + 1u);
    TEST_ASSERT_NOT_NULL(destination);
    TEST_ASSERT_NOT_NULL(source);
    ZrCore_Value_ResetAsNull(destination);
    ZrCore_Value_ResetAsNull(source);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state, source, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));

    memset(&function, 0, sizeof(function));
    memset(&frame, 0, sizeof(frame));
    frame.function = &function;
    frame.slotBase = slotBase;
    frame.generatedFrameSlotCount = 2u;

    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_OwnDetach(g_state, &frame, 0u, 1u));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source->type));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination->ownershipKind);
    TEST_ASSERT_TRUE(ZrCore_Ownership_IsGcBoxObject(destination->value.object));
    TEST_ASSERT_EQUAL_UINT32(
            0u, (TZrUInt32)ZrCore_GcDomain_GetOwnershipRootCount(g_state));
}

static void test_gc_bridge_types_preserve_canonical_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrInferredType resourceType;
    TZrTypeId resourceTypeId;
    TZrTypeId gcHandleTypeId;
    TZrTypeId gcBoxTypeId;
    TZrChar display[128];
    SZrString *resourceName =
            ZrCore_String_CreateFromNative(g_state, "BridgeResource");

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_InferredType_InitFull(
            g_state,
            &resourceType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            resourceName);
    resourceTypeId = ZrParser_CanonicalType_FromInferred(context, &resourceType);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, resourceTypeId);

    resourceType.gcBridgeKind = ZR_GC_BRIDGE_HANDLE;
    gcHandleTypeId = ZrParser_CanonicalType_FromInferred(context, &resourceType);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, gcHandleTypeId);
    TEST_ASSERT_NOT_EQUAL(resourceTypeId, gcHandleTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            context, gcHandleTypeId, display, sizeof(display)));
    TEST_ASSERT_EQUAL_STRING("Gc<BridgeResource>", display);

    resourceType.gcBridgeKind = ZR_GC_BRIDGE_BOX;
    gcBoxTypeId = ZrParser_CanonicalType_FromInferred(context, &resourceType);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, gcBoxTypeId);
    TEST_ASSERT_NOT_EQUAL(resourceTypeId, gcBoxTypeId);
    TEST_ASSERT_NOT_EQUAL(gcHandleTypeId, gcBoxTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            context, gcBoxTypeId, display, sizeof(display)));
    TEST_ASSERT_EQUAL_STRING("GcBox<BridgeResource>", display);
    TEST_ASSERT_EQUAL_STRING(
            "GcBox<BridgeResource>",
            ZrParser_TypeNameString_Get(
                    g_state, &resourceType, display, sizeof(display)));

    ZrParser_InferredType_Free(g_state, &resourceType);
    ZrParser_SemanticContext_Free(context);
}

static void test_gc_bridge_surface_enforces_target_worlds(void) {
    SZrFunction *gcHandle = compile_source(
            "class Document {}\n"
            "resource class Request {\n"
            "  var document: Gc<Document>;\n"
            "  pub @constructor(document: Gc<Document>) { this.document = document; }\n"
            "}\n");
    SZrFunction *gcRejectsResource = compile_source(
            "resource class Socket {}\n"
            "accept(value: Gc<Socket>) {}\n");
    SZrFunction *gcBoxRejectsOrdinaryClass = compile_source(
            "class Document {}\n"
            "accept(value: GcBox<Document>) {}\n");

    TEST_ASSERT_NOT_NULL(gcHandle);
    TEST_ASSERT_NULL(gcRejectsResource);
    TEST_ASSERT_NULL(gcBoxRejectsOrdinaryClass);
    ZrCore_Function_Free(g_state, gcHandle);
}

static void test_resource_unique_into_gc_surface_consumes_owner(void) {
    SZrFunction *function = compile_source(
            "resource class BoxedCounter {\n"
            "  var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub const fn read(): int { return this.value; }\n"
            "}\n"
            "run(): int {\n"
            "  var owner: Unique<BoxedCounter> = own BoxedCounter(9);\n"
            "  var boxed: GcBox<BoxedCounter> = owner.intoGc();\n"
            "  return boxed.read();\n"
            "}\n"
            "return run();\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_DETACH), 0U));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(9, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_into_gc_surface_rejects_shared_and_active_borrow(void) {
    SZrFunction *shared = compile_source(
            "resource class BoxedCounter {}\n"
            "var owner: Unique<BoxedCounter> = own BoxedCounter();\n"
            "var shared = owner.share();\n"
            "var boxed = shared.intoGc();\n");
    SZrFunction *activeBorrow = compile_source(
            "resource class BoxedCounter {\n"
            "  var value: int;\n"
            "  pub fn borrowValue(): ref int { return this.value; }\n"
            "}\n"
            "run(): int {\n"
            "  var owner: Unique<BoxedCounter> = own BoxedCounter();\n"
            "  var view: ref int = owner.borrowValue();\n"
            "  var boxed = owner.intoGc();\n"
            "  return view;\n"
            "}\n"
            "return run();\n");
    SZrFunction *useAfterMove = compile_source(
            "resource class BoxedCounter {}\n"
            "var owner: Unique<BoxedCounter> = own BoxedCounter();\n"
            "var boxed = owner.intoGc();\n"
            "drop(owner);\n");

    TEST_ASSERT_NULL(shared);
    TEST_ASSERT_NULL(activeBorrow);
    TEST_ASSERT_NULL(useAfterMove);
}

static void test_resource_drop_order_move_and_explicit_drop_execute_once(void) {
    static const TZrChar *source =
            "resource class Tracer {\n"
            "  pub static var dropLog: int = 0;\n"
            "  var id: int;\n"
            "  pub @constructor(id: int) { this.id = id; }\n"
            "  pub @destructor() { Tracer.dropLog = Tracer.dropLog * 10 + this.id; }\n"
            "}\n"
            "run(): int {\n"
            "  {\n"
            "    var first: Unique<Tracer> = own Tracer(1);\n"
            "    var moved: Unique<Tracer> = first;\n"
            "    var second: Unique<Tracer> = own Tracer(2);\n"
            "    drop(second);\n"
            "  }\n"
            "  return Tracer.dropLog;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(21, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_nested_fields_drop_in_reverse_declaration_order(void) {
    static const TZrChar *source =
            "resource class Leaf {\n"
            "  pub static var dropLog: int = 0;\n"
            "  var id: int;\n"
            "  pub @constructor(id: int) { this.id = id; }\n"
            "  pub @destructor() { Leaf.dropLog = Leaf.dropLog * 10 + this.id; }\n"
            "}\n"
            "resource class Pair {\n"
            "  var first: Unique<Leaf>;\n"
            "  var second: Unique<Leaf>;\n"
            "  pub @constructor() {\n"
            "    this.first = own Leaf(1);\n"
            "    this.second = own Leaf(2);\n"
            "  }\n"
            "}\n"
            "run(): int {\n"
            "  { var pair: Unique<Pair> = own Pair(); }\n"
            "  return Leaf.dropLog;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(21, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_partial_construction_and_throw_cleanup_drop_initialized_fields_only(void) {
    static const TZrChar *source =
            "resource class Leaf {\n"
            "  pub static var dropLog: int = 0;\n"
            "  var id: int;\n"
            "  pub @constructor(id: int) { this.id = id; }\n"
            "  pub @destructor() { Leaf.dropLog = Leaf.dropLog * 10 + this.id; }\n"
            "}\n"
            "resource class Broken {\n"
            "  var first: Unique<Leaf>;\n"
            "  var second: Unique<Leaf>;\n"
            "  pub @constructor() {\n"
            "    this.first = own Leaf(1);\n"
            "    throw \"stop\";\n"
            "  }\n"
            "  pub @destructor() { Leaf.dropLog = Leaf.dropLog + 100; }\n"
            "}\n"
            "run(): int {\n"
            "  try { var broken: Unique<Broken> = own Broken(); } catch (error) {}\n"
            "  return Leaf.dropLog;\n"
            "}\n"
            "return run();\n";
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_move_invalidates_source_statically(void) {
    SZrFunction *invalid = compile_source(
            "resource class Socket {}\n"
            "run() {\n"
            "  var source: Unique<Socket> = own Socket();\n"
            "  var destination: Unique<Socket> = source;\n"
            "  drop(source);\n"
            "}\n");

    TEST_ASSERT_NULL(invalid);
}

static void test_resource_custom_drop_cannot_throw(void) {
    SZrFunction *invalid = compile_source(
            "resource class BrokenDrop {\n"
            "  pub @destructor() { throw \"drop failed\"; }\n"
            "}\n");

    TEST_ASSERT_NULL(invalid);
}

static void test_resource_cleanup_runs_on_return_break_and_continue(void) {
    SZrFunction *function = compile_source(
            "resource class Tracker {\n"
            "  pub static var dropLog: int = 0;\n"
            "  var id: int;\n"
            "  pub @constructor(id: int) { this.id = id; }\n"
            "  pub @destructor() { Tracker.dropLog = Tracker.dropLog * 10 + this.id; }\n"
            "}\n"
            "early(): int {\n"
            "  var value: Unique<Tracker> = own Tracker(1);\n"
            "  return 7;\n"
            "}\n"
            "loop(): int {\n"
            "  var i = 0;\n"
            "  while (i < 2) {\n"
            "    i = i + 1;\n"
            "    var value: Unique<Tracker> = own Tracker(i + 1);\n"
            "    if (i == 1) { continue; }\n"
            "    break;\n"
            "  }\n"
            "  return Tracker.dropLog;\n"
            "}\n"
            "var ignored = early();\n"
            "return loop();\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(123, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_cleanup_runs_on_throw(void) {
    SZrFunction *function = compile_source(
            "resource class Tracker {\n"
            "  pub static var dropLog: int = 0;\n"
            "  var id: int;\n"
            "  pub @constructor(id: int) { this.id = id; }\n"
            "  pub @destructor() { Tracker.dropLog = Tracker.dropLog * 10 + this.id; }\n"
            "}\n"
            "explode() {\n"
            "  var value: Unique<Tracker> = own Tracker(5);\n"
            "  throw \"stop\";\n"
            "}\n"
            "run(): int {\n"
            "  try { explode(); } catch (error) {}\n"
            "  return Tracker.dropLog;\n"
            "}\n"
            "return run();\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_unique_moves_through_value_parameter_and_return(void) {
    SZrFunction *function = compile_source(
            "resource class Tracker {\n"
            "  pub static var dropLog: int = 0;\n"
            "  var id: int;\n"
            "  pub @constructor(id: int) { this.id = id; }\n"
            "  pub @destructor() { Tracker.dropLog = Tracker.dropLog * 10 + this.id; }\n"
            "}\n"
            "make(id: int): Unique<Tracker> {\n"
            "  var value: Unique<Tracker> = own Tracker(id);\n"
            "  return value;\n"
            "}\n"
            "consume(value: Unique<Tracker>) { drop(value); }\n"
            "var returned: Unique<Tracker> = make(4);\n"
            "consume(returned);\n"
            "return Tracker.dropLog;\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_unique_is_non_nullable(void) {
    SZrFunction *invalid = compile_source(
            "resource class Socket {}\n"
            "var missing: Unique<Socket> = null;\n");

    TEST_ASSERT_NULL(invalid);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resource_surface_builds_canonical_owner_ast);
    RUN_TEST(test_resource_construction_world_is_type_directed);
    RUN_TEST(test_resource_contextual_tokens_preserve_identifier_calls);
    RUN_TEST(test_resource_unique_uses_direct_owner_without_control_block);
    RUN_TEST(test_resource_unique_into_gc_box_consumes_owner_and_defers_drop);
    RUN_TEST(test_aot_own_detach_consumes_resource_unique_into_gc_box);
    RUN_TEST(test_gc_bridge_types_preserve_canonical_identity);
    RUN_TEST(test_gc_bridge_surface_enforces_target_worlds);
    RUN_TEST(test_resource_unique_into_gc_surface_consumes_owner);
    RUN_TEST(test_resource_into_gc_surface_rejects_shared_and_active_borrow);
    RUN_TEST(test_resource_drop_order_move_and_explicit_drop_execute_once);
    RUN_TEST(test_resource_nested_fields_drop_in_reverse_declaration_order);
    RUN_TEST(test_resource_partial_construction_and_throw_cleanup_drop_initialized_fields_only);
    RUN_TEST(test_resource_move_invalidates_source_statically);
    RUN_TEST(test_resource_custom_drop_cannot_throw);
    RUN_TEST(test_resource_cleanup_runs_on_return_break_and_continue);
    RUN_TEST(test_resource_cleanup_runs_on_throw);
    RUN_TEST(test_resource_unique_moves_through_value_parameter_and_return);
    RUN_TEST(test_resource_unique_is_non_nullable);
    return UNITY_END();
}
