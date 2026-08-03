#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/io.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

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

static SZrFunction *compile_source(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_access_lowering.zr");
    return ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
}

static SZrAstNode *parse_source(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "property_access_lowering_contract.zr");
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
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

static void compile_type_declarations(SZrCompilerState *compiler,
                                      SZrAstNode *script) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    for (TZrSize index = 0u;
         index < script->data.script.statements->count && !compiler->hasError;
         index++) {
        SZrAstNode *declaration = script->data.script.statements->nodes[index];
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, declaration->type);
        ZrParser_Compiler_CompileClassDeclaration(compiler, declaration);
    }
}

static TZrInt64 compile_and_execute_int(const char *source) {
    SZrFunction *function = compile_source(source);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    ZrCore_Function_Free(g_state, function);
    return result;
}

static void fixture_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *load_binary_entry(SZrState *state, const char *path) {
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
    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(
            state,
            io,
            ZrTests_Fixture_ReaderRead,
            fixture_reader_close_noop,
            &reader);
    io->isBinary = ZR_TRUE;
    source = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(source);
    function = ZrCore_Io_LoadEntryFunctionToRuntime(state, source);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Io_Free(state->global, io);
    free(bytes);
    return function;
}

static TZrUInt32 count_instruction_opcode(const SZrFunction *function,
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

static TZrUInt32 count_call_site_cache_kind(
        const SZrFunction *function,
        EZrFunctionCallSiteCacheKind kind) {
    TZrUInt32 count = 0u;
    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; index++) {
        if (function->callSiteCaches[index].kind == kind) {
            count++;
        }
    }
    return count;
}

static void assert_property_member_entries_use_visible_identity(
        const SZrFunction *function) {
    TZrUInt32 propertyEntryCount = 0u;
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0u; index < function->memberEntryLength; index++) {
        const SZrFunctionMemberEntry *entry = &function->memberEntries[index];
        const char *symbol = entry->symbol != ZR_NULL
                                     ? ZrCore_String_GetNativeString(entry->symbol)
                                     : ZR_NULL;
        if (symbol == ZR_NULL) {
            continue;
        }
        TEST_ASSERT_NULL(strstr(symbol, "__get_"));
        TEST_ASSERT_NULL(strstr(symbol, "__set_"));
        if (strcmp(symbol, "value") == 0) {
            propertyEntryCount++;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, propertyEntryCount);
}

static void test_direct_property_get_and_set_execute(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int = 0;\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value = 7;\n"
            "return box.value;\n";

    TEST_ASSERT_EQUAL_INT64(7, compile_and_execute_int(source));
}

static void test_property_accessors_keep_linked_typed_receiver_contracts(void) {
    static const char source[] =
            "class Box {\n"
            "  pub property value: int {\n"
            "    get { return 1; }\n"
            "    set { return; }\n"
            "  }\n"
            "}\n"
            "class InitBox {\n"
            "  pub property value: int { init { return; } }\n"
            "}\n"
            "class StaticBox {\n"
            "  pub static property value: int {\n"
            "    get { return 1; }\n"
            "    set { return; }\n"
            "  }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrTypePrototypeInfo *box;
    const SZrTypePrototypeInfo *initBox;
    const SZrTypePrototypeInfo *staticBox;
    const SZrTypeMemberInfo *property;
    const SZrTypeMemberInfo *getter;
    const SZrTypeMemberInfo *setter;
    const SZrTypeMemberInfo *initializer;
    const SZrTypeMemberInfo *staticGetter;
    const SZrTypeMemberInfo *staticSetter;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_type_declarations(&compiler, script);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);

    box = find_type_prototype(&compiler, "Box");
    initBox = find_type_prototype(&compiler, "InitBox");
    staticBox = find_type_prototype(&compiler, "StaticBox");
    property = find_property_member(box, ZR_PROPERTY_ACCESSOR_ROLE_NONE);
    getter = find_property_member(box, ZR_PROPERTY_ACCESSOR_ROLE_GET);
    setter = find_property_member(box, ZR_PROPERTY_ACCESSOR_ROLE_SET);
    initializer = find_property_member(initBox, ZR_PROPERTY_ACCESSOR_ROLE_INIT);
    staticGetter = find_property_member(staticBox, ZR_PROPERTY_ACCESSOR_ROLE_GET);
    staticSetter = find_property_member(staticBox, ZR_PROPERTY_ACCESSOR_ROLE_SET);

    TEST_ASSERT_NOT_NULL(property);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_NOT_NULL(setter);
    TEST_ASSERT_NOT_NULL(initializer);
    TEST_ASSERT_NOT_NULL(staticGetter);
    TEST_ASSERT_NOT_NULL(staticSetter);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, property->propertyValueTypeId);
    TEST_ASSERT_EQUAL_UINT64(property->symbolId, property->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT64(property->symbolId, getter->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT64(property->symbolId, setter->propertySymbolId);
    TEST_ASSERT_EQUAL_UINT64(getter->symbolId, property->getterAccessorSymbolId);
    TEST_ASSERT_EQUAL_UINT64(setter->symbolId, property->setterAccessorSymbolId);
    TEST_ASSERT_EQUAL_UINT32(0u, getter->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, setter->parameterCount);
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(getter->returnTypeName));
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_READONLY, getter->receiverEffect);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_MUTABLE, setter->receiverEffect);
    TEST_ASSERT_EQUAL_INT(ZR_PROPERTY_ACCESSOR_ROLE_INIT, initializer->accessorRole);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_MUTABLE, initializer->receiverEffect);
    TEST_ASSERT_TRUE(staticGetter->isStatic);
    TEST_ASSERT_TRUE(staticSetter->isStatic);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_NONE, staticGetter->receiverEffect);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_NONE, staticSetter->receiverEffect);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_compound_property_assignment_evaluates_each_stage_once_in_order(void) {
    static const char source[] =
            "class Box {\n"
            "  pub static var receiverCalls: int = 0;\n"
            "  pub static var rhsCalls: int = 0;\n"
            "  pub static var trace: int = 0;\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 4; }\n"
            "  pub property value: int {\n"
            "    get { Box.trace = Box.trace * 10 + 1; return this.stored; }\n"
            "    set { Box.trace = Box.trace * 10 + 3; this.stored = value; }\n"
            "  }\n"
            "  pub fn raw(): int { return this.stored; }\n"
            "}\n"
            "class Holder {\n"
            "  pri var stored: Box;\n"
            "  pub @constructor(box: Box) { this.stored = box; }\n"
            "  pub property box: Box {\n"
            "    get { Box.receiverCalls = Box.receiverCalls + 1; return this.stored; }\n"
            "  }\n"
            "  pub fn raw(): int { return this.stored.raw(); }\n"
            "}\n"
            "fn rhs(): int {\n"
            "  Box.rhsCalls = Box.rhsCalls + 1;\n"
            "  Box.trace = Box.trace * 10 + 2;\n"
            "  return 3;\n"
            "}\n"
            "var box = new Box();\n"
            "var holder = new Holder(box);\n"
            "holder.box.value += rhs();\n"
            "return holder.raw() * 100000 + Box.receiverCalls * 10000 + Box.rhsCalls * 1000 + Box.trace;\n";

    TEST_ASSERT_EQUAL_INT64(711123, compile_and_execute_int(source));
}

static void test_compound_property_assignment_requires_getter_and_setter(void) {
    static const char getterOnlySource[] =
            "class ReadOnlyBox {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: int { get { return this.stored; } }\n"
            "}\n"
            "var box = new ReadOnlyBox();\n"
            "box.value += 1;\n";
    static const char setterOnlySource[] =
            "class WriteOnlyBox {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: int { set { this.stored = value; } }\n"
            "}\n"
            "var box = new WriteOnlyBox();\n"
            "box.value += 1;\n";

    TEST_ASSERT_NULL(compile_source(getterOnlySource));
    TEST_ASSERT_NULL(compile_source(setterOnlySource));
}

static void test_non_additive_compound_property_assignment_executes(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 4; }\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value *= 3;\n"
            "return box.value;\n";

    TEST_ASSERT_EQUAL_INT64(12, compile_and_execute_int(source));
}

static void test_compound_property_assignment_uses_ordinary_operator_compatibility(void) {
    static const char propertySource[] =
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value *= \"invalid\";\n";
    static const char ordinarySource[] =
            "var value: int = 1;\n"
            "value *= \"invalid\";\n";
    SZrFunction *propertyFunction = compile_source(propertySource);
    SZrFunction *ordinaryFunction = compile_source(ordinarySource);

    TEST_ASSERT_NULL(propertyFunction);
    TEST_ASSERT_NULL(ordinaryFunction);
}

static void test_compound_property_receiver_owner_cleanup_runs_once(void) {
    static const char source[] =
            "resource class Tracker {\n"
            "  pub static var dropCount: int = 0;\n"
            "  pub static var getterCalls: int = 0;\n"
            "  pub static var setterCalls: int = 0;\n"
            "  pri var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub @destructor() { Tracker.dropCount = Tracker.dropCount + 1; }\n"
            "  pub property value: int {\n"
            "    get { Tracker.getterCalls = Tracker.getterCalls + 1; return this.stored; }\n"
            "    set { Tracker.setterCalls = Tracker.setterCalls + 1; this.stored = value; }\n"
            "  }\n"
            "}\n"
            "fn run(): int {\n"
            "  var owner: Unique<Tracker> = own Tracker(4);\n"
            "  owner.value += 2;\n"
            "  drop(owner);\n"
            "  return Tracker.dropCount * 100 + Tracker.getterCalls * 10 + Tracker.setterCalls;\n"
            "}\n"
            "return run();\n";

    SZrFunction *function = compile_source(source);
    SZrTypeValue returnValue;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(g_state, function, &returnValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(returnValue.type));
    TEST_ASSERT_EQUAL_INT64(111, returnValue.value.nativeObject.nativeInt64);
    ZrCore_Function_Free(g_state, function);
}

static void test_ref_return_property_compound_remains_deferred(void) {
    static const char source[] =
            "class Box {\n"
            "  pri var stored: int = 1;\n"
            "  pub property value: ref int { get { return this.stored; } }\n"
            "}\n"
            "var box = new Box();\n"
            "box.value += 1;\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_inline_struct_property_compound_preserves_writeback(void) {
    static const char source[] =
            "struct Point {\n"
            "  pri var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "}\n"
            "var point: Point = init Point(4);\n"
            "point.value += 3;\n"
            "return point.value;\n";

    TEST_ASSERT_EQUAL_INT64(7, compile_and_execute_int(source));
}

static void test_readonly_getter_cannot_mutate_instance_field(void) {
    static const char source[] =
            "class InvalidGetter {\n"
            "  pri var stored: int = 0;\n"
            "  pub property value: int {\n"
            "    get { this.stored = 1; return this.stored; }\n"
            "  }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_readonly_receiver_cannot_select_setter(void) {
    static const char source[] =
            "class InvalidWriter {\n"
            "  pri var stored: int = 0;\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "  pub const fn invalid(): int { this.value = 1; return 0; }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_init_accessor_is_not_selected_by_ordinary_assignment(void) {
    static const char source[] =
            "class InitOnly {\n"
            "  pri var stored: int = 0;\n"
            "  pub property value: int { init { this.stored = value; } }\n"
            "}\n"
            "var box = new InitOnly();\n"
            "box.value = 1;\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_static_property_access_has_no_receiver(void) {
    static const char source[] =
            "class Counts {\n"
            "  pri static var stored: int = 1;\n"
            "  pub static property value: int {\n"
            "    get { return Counts.stored; }\n"
            "    set { Counts.stored = value; }\n"
            "  }\n"
            "}\n"
            "Counts.value = 9;\n"
            "return Counts.value;\n";

    TEST_ASSERT_EQUAL_INT64(9, compile_and_execute_int(source));
}

static void test_virtual_property_dispatch_uses_derived_accessors(void) {
    static const char source[] =
            "abstract class Base {\n"
            "  pub abstract property value: int { get; set; }\n"
            "}\n"
            "class Derived : Base {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 0; }\n"
            "  pub override property value: int {\n"
            "    get { return this.stored + 1; }\n"
            "    set { this.stored = value * 2; }\n"
            "  }\n"
            "}\n"
            "var base: Base = new Derived();\n"
            "base.value += 4;\n"
            "return base.value;\n";

    TEST_ASSERT_EQUAL_INT64(11, compile_and_execute_int(source));
}

static void test_interface_property_dispatch_uses_implementation_accessors(void) {
    static const char source[] =
            "interface ValueContract {\n"
            "  pub property value: int { pub get; pub set; }\n"
            "}\n"
            "class ValueBox : ValueContract {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 0; }\n"
            "  pub property value: int {\n"
            "    get { return this.stored + 2; }\n"
            "    set { this.stored = value * 3; }\n"
            "  }\n"
            "}\n"
            "var contract: ValueContract = new ValueBox();\n"
            "contract.value += 4;\n"
            "return contract.value;\n";

    TEST_ASSERT_EQUAL_INT64(20, compile_and_execute_int(source));
}

static void test_inherited_property_dispatch_uses_base_accessors(void) {
    static const char source[] =
            "class Base {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 1; }\n"
            "  pub property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "}\n"
            "class Derived : Base {\n"
            "  pub @constructor() super() {}\n"
            "}\n"
            "var derived = new Derived();\n"
            "derived.value += 5;\n"
            "return derived.value;\n";

    TEST_ASSERT_EQUAL_INT64(6, compile_and_execute_int(source));
}

static void test_static_compound_property_access_has_no_receiver(void) {
    static const char source[] =
            "class Counts {\n"
            "  pri static var stored: int = 2;\n"
            "  pub static property value: int {\n"
            "    get { return Counts.stored; }\n"
            "    set { Counts.stored = value; }\n"
            "  }\n"
            "}\n"
            "Counts.value *= 4;\n"
            "return Counts.value;\n";

    TEST_ASSERT_EQUAL_INT64(8, compile_and_execute_int(source));
}

static void test_property_override_cannot_drop_required_setter(void) {
    static const char source[] =
            "abstract class Base {\n"
            "  pub abstract property value: int { get; set; }\n"
            "}\n"
            "class Invalid : Base {\n"
            "  pub override property value: int { get { return 1; } }\n"
            "}\n";

    TEST_ASSERT_NULL(compile_source(source));
}

static void test_property_compound_binary_roundtrip_preserves_access_contract(void) {
    static const char source[] =
            "abstract class Base {\n"
            "  pub abstract property value: int { get; set; }\n"
            "}\n"
            "class Box : Base {\n"
            "  pri var stored: int;\n"
            "  pub @constructor() { this.stored = 4; }\n"
            "  pub override property value: int {\n"
            "    get { return this.stored; }\n"
            "    set { this.stored = value; }\n"
            "  }\n"
            "}\n"
            "var box: Base = new Box();\n"
            "box.value *= 3;\n"
            "return box.value;\n";
    static const char binaryPath[] =
            "zr_vm_property_access_lowering_roundtrip.zro";
    SZrFunction *sourceFunction = compile_source(source);
    SZrFunction *runtimeFunction;
    TZrInstruction *serializedInstructions;
    TZrInt64 sourceResult = 0;
    TZrInt64 artifactResult = 0;
    TZrBool sourceFunctionIgnored;

    TEST_ASSERT_NOT_NULL(sourceFunction);
    sourceFunctionIgnored = ZrCore_GarbageCollector_IgnoreObject(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceFunction));
    TEST_ASSERT_TRUE(sourceFunctionIgnored);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            count_instruction_opcode(sourceFunction, ZR_INSTRUCTION_ENUM(META_GET)) +
                    count_instruction_opcode(
                            sourceFunction, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)) +
                    count_instruction_opcode(
                            sourceFunction, ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            count_instruction_opcode(sourceFunction, ZR_INSTRUCTION_ENUM(META_SET)) +
                    count_instruction_opcode(
                            sourceFunction, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)) +
                    count_instruction_opcode(
                            sourceFunction, ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED)));
    assert_property_member_entries_use_visible_identity(sourceFunction);
    serializedInstructions = (TZrInstruction *)malloc(
            sourceFunction->instructionsLength * sizeof(*serializedInstructions));
    TEST_ASSERT_NOT_NULL(serializedInstructions);
    memcpy(serializedInstructions,
           sourceFunction->instructionsList,
           sourceFunction->instructionsLength * sizeof(*serializedInstructions));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, sourceFunction, &sourceResult));
    TEST_ASSERT_EQUAL_INT64(12, sourceResult);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            count_call_site_cache_kind(
                    sourceFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            count_call_site_cache_kind(
                    sourceFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state, sourceFunction, binaryPath));
    runtimeFunction = load_binary_entry(g_state, binaryPath);
    assert_property_member_entries_use_visible_identity(runtimeFunction);
    TEST_ASSERT_EQUAL_UINT32(sourceFunction->stackSize, runtimeFunction->stackSize);
    TEST_ASSERT_EQUAL_UINT32(sourceFunction->vmEntryClearStackSizePlusOne,
                             runtimeFunction->vmEntryClearStackSizePlusOne);
    TEST_ASSERT_EQUAL_UINT32(sourceFunction->instructionsLength,
                             runtimeFunction->instructionsLength);
    TEST_ASSERT_EQUAL_UINT32(sourceFunction->memberEntryLength,
                             runtimeFunction->memberEntryLength);
    TEST_ASSERT_EQUAL_MEMORY(serializedInstructions,
                             runtimeFunction->instructionsList,
                             sourceFunction->instructionsLength *
                                     sizeof(*sourceFunction->instructionsList));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, runtimeFunction, &artifactResult));
    TEST_ASSERT_EQUAL_INT64(sourceResult, artifactResult);

    remove(binaryPath);
    free(serializedInstructions);
    ZrCore_Function_Free(g_state, runtimeFunction);
    ZrCore_GarbageCollector_UnignoreObject(
            g_state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceFunction));
    ZrCore_Function_Free(g_state, sourceFunction);
}

static void test_throwing_property_getter_is_catchable(void) {
    static const char source[] =
            "class Box {\n"
            "  pub static var rhsCalls: int = 0;\n"
            "  pub static var setterCalls: int = 0;\n"
            "  pub property value: int {\n"
            "    get { throw \"getter\"; }\n"
            "    set { Box.setterCalls = Box.setterCalls + 1; }\n"
            "  }\n"
            "}\n"
            "fn rhs(): int { Box.rhsCalls = Box.rhsCalls + 1; return 1; }\n"
            "var box = new Box();\n"
            "try { box.value += rhs(); } catch (error) {}\n"
            "return Box.rhsCalls * 10 + Box.setterCalls;\n";

    TEST_ASSERT_EQUAL_INT64(0, compile_and_execute_int(source));
}

static void test_throwing_rhs_prevents_setter(void) {
    static const char source[] =
            "class Box {\n"
            "  pub static var getterCalls: int = 0;\n"
            "  pub static var rhsCalls: int = 0;\n"
            "  pub static var setterCalls: int = 0;\n"
            "  pub property value: int {\n"
            "    get { Box.getterCalls = Box.getterCalls + 1; return 1; }\n"
            "    set { Box.setterCalls = Box.setterCalls + 1; }\n"
            "  }\n"
            "}\n"
            "fn rhs(): int { Box.rhsCalls = Box.rhsCalls + 1; throw \"rhs\"; }\n"
            "var box = new Box();\n"
            "try { box.value += rhs(); } catch (error) {}\n"
            "return Box.getterCalls * 100 + Box.rhsCalls * 10 + Box.setterCalls;\n";

    TEST_ASSERT_EQUAL_INT64(110, compile_and_execute_int(source));
}

static void test_throwing_setter_runs_after_getter_and_rhs(void) {
    static const char source[] =
            "class Box {\n"
            "  pub static var getterCalls: int = 0;\n"
            "  pub static var rhsCalls: int = 0;\n"
            "  pub static var setterCalls: int = 0;\n"
            "  pub property value: int {\n"
            "    get { Box.getterCalls = Box.getterCalls + 1; return 1; }\n"
            "    set { Box.setterCalls = Box.setterCalls + 1; throw \"setter\"; }\n"
            "  }\n"
            "}\n"
            "fn rhs(): int { Box.rhsCalls = Box.rhsCalls + 1; return 2; }\n"
            "var box = new Box();\n"
            "try { box.value += rhs(); } catch (error) {}\n"
            "return Box.getterCalls * 100 + Box.rhsCalls * 10 + Box.setterCalls;\n";

    TEST_ASSERT_EQUAL_INT64(111, compile_and_execute_int(source));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_direct_property_get_and_set_execute);
    RUN_TEST(test_property_accessors_keep_linked_typed_receiver_contracts);
    RUN_TEST(test_compound_property_assignment_evaluates_each_stage_once_in_order);
    RUN_TEST(test_compound_property_assignment_requires_getter_and_setter);
    RUN_TEST(test_non_additive_compound_property_assignment_executes);
    RUN_TEST(test_compound_property_assignment_uses_ordinary_operator_compatibility);
    RUN_TEST(test_compound_property_receiver_owner_cleanup_runs_once);
    RUN_TEST(test_ref_return_property_compound_remains_deferred);
    RUN_TEST(test_inline_struct_property_compound_preserves_writeback);
    RUN_TEST(test_readonly_getter_cannot_mutate_instance_field);
    RUN_TEST(test_readonly_receiver_cannot_select_setter);
    RUN_TEST(test_init_accessor_is_not_selected_by_ordinary_assignment);
    RUN_TEST(test_static_property_access_has_no_receiver);
    RUN_TEST(test_virtual_property_dispatch_uses_derived_accessors);
    RUN_TEST(test_interface_property_dispatch_uses_implementation_accessors);
    RUN_TEST(test_inherited_property_dispatch_uses_base_accessors);
    RUN_TEST(test_static_compound_property_access_has_no_receiver);
    RUN_TEST(test_property_override_cannot_drop_required_setter);
    RUN_TEST(test_property_compound_binary_roundtrip_preserves_access_contract);
    RUN_TEST(test_throwing_property_getter_is_catchable);
    RUN_TEST(test_throwing_rhs_prevents_setter);
    RUN_TEST(test_throwing_setter_runs_after_getter_and_rhs);
    return UNITY_END();
}
