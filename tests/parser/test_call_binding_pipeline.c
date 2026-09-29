#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

/* 每例运行时持有编译函数图；GC 终结时释放函数的原生绑定缓存。 */
static SZrState *state;

/* Unity 为每例提供独立运行时，使绑定代际和错误状态不串例。 */
void setUp(void) {
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}

/* 运行时析构会终结 GC 函数及其绑定元数据。 */
void tearDown(void) {
    ZrTests_Runtime_State_Destroy(state);
    state = ZR_NULL;
}

/* 编译脚本并返回由运行时 GC 持有的函数图。 */
static SZrFunction *compile_source(const char *source) {
    return ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "call_binding_pipeline.zr"));
}

/* 在前置语义校验通过后编译脚本，确认目标错误确由 call binding 报出。 */
/* BUG: 初始化 compiler 后任一 Unity 断言失败会跳过本函数末尾的 Free；
 * tearDown 只销毁运行时，compiler.semanticContext 等原生分配泄漏。 */
static void assert_compile_diagnostic(const char *source, const char *expectedMessage) {
    SZrAstNode *ast;
    SZrCompilerState compiler;

    ast = ZrParser_Parse(state,
                         source,
                         strlen(source),
                         ZrCore_String_CreateFromNative(state, "call_binding_diagnostic.zr"));
    TEST_ASSERT_NOT_NULL(ast);
    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = ast;
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&compiler, ast));
    TEST_ASSERT_TRUE(compiler.hasError == ZR_FALSE);
    TEST_ASSERT_TRUE(compiler_validate_ref_struct_rules(&compiler, ast));
    TEST_ASSERT_TRUE(compiler_validate_reference_escapes(&compiler, ast));
    TEST_ASSERT_TRUE(compiler_validate_task_effects(&compiler, ast));
    compiler.currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    compile_script(&compiler, ast);
    TEST_ASSERT_TRUE_MESSAGE(compiler.hasError, expectedMessage);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(compiler.errorMessage, expectedMessage),
                                 compiler.errorMessage);
    ZrCore_Function_Free(state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(state, ast);
}

/* 深度优先返回首个有契约的调用点，借用函数图内缓存项。 */
static const SZrFunctionCallSiteCacheEntry *find_binding(const SZrFunction *function) {
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->binding.contract.bindingKind != 0u) {
            return entry;
        }
    }
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = find_binding(&function->childFunctionList[index]);
        if (entry != ZR_NULL) return entry;
    }
    return ZR_NULL;
}

/* 遍历顶层与子函数缓存，按 GET/SET 操作统计 property 绑定。 */
static void count_property_bindings(const SZrFunction *function,
                                    TZrUInt32 *getters,
                                    TZrUInt32 *setters) {
    if (function == ZR_NULL) return;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (entry->binding.contract.operation == ZR_CALL_BINDING_OPERATION_GET) ++*getters;
        if (entry->binding.contract.operation == ZR_CALL_BINDING_OPERATION_SET) ++*setters;
    }
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        count_property_bindings(&function->childFunctionList[index], getters, setters);
    }
}

/* 检查首个绑定的 token/hash、实际执行结果和该缓存项的运行时命中次数。 */
/* TODO: 首个绑定未必是源码中的最终调用；需按 call-site 或目标身份定位，
 * 才能证明链式/接口/meta 调用自身的契约，而非旁侧构造调用的契约。 */
static void assert_bound_result(const char *source, TZrInt64 expected) {
    SZrFunction *function = compile_source(source);
    const SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    entry = find_binding(function);
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "a statically selected call needs a binding contract");
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.targetMetadataToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.signatureToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, entry->binding.contract.signatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, entry->binding.contract.moduleSignatureHash);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
}

static const SZrFunction *find_child_function_by_name(const SZrFunction *function,
                                                       const char *name) {
    if (function == ZR_NULL || name == ZR_NULL) return ZR_NULL;
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        const SZrFunction *child = &function->childFunctionList[index];
        const char *childName = child->functionName != ZR_NULL
                                        ? ZrCore_String_GetNativeString(child->functionName)
                                        : ZR_NULL;
        const SZrFunction *nested;
        if (childName != ZR_NULL && strcmp(childName, name) == 0) return child;
        nested = find_child_function_by_name(child, name);
        if (nested != ZR_NULL) return nested;
    }
    return ZR_NULL;
}

static const SZrFunction *find_meta_function_from_prototype_data(const SZrFunction *function,
                                                                  EZrMetaType metaType) {
    const TZrByte *data;
    TZrUInt32 prototypeCount;
    TZrSize offset = sizeof(prototypeCount);

    if (function == ZR_NULL || function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(prototypeCount) ||
        function->constantValueList == ZR_NULL) {
        return ZR_NULL;
    }

    data = function->prototypeData;
    memcpy(&prototypeCount, data, sizeof(prototypeCount));
    for (TZrUInt32 prototypeIndex = 0u; prototypeIndex < prototypeCount; ++prototypeIndex) {
        SZrCompiledPrototypeInfo prototype;
        TZrSize inheritBytes;
        TZrSize decoratorBytes;
        TZrSize memberBytes;

        if (offset > function->prototypeDataLength ||
            function->prototypeDataLength - offset < sizeof(prototype)) {
            return ZR_NULL;
        }
        memcpy(&prototype, data + offset, sizeof(prototype));
        offset += sizeof(prototype);
        inheritBytes = (TZrSize)prototype.inheritsCount * sizeof(TZrUInt32);
        decoratorBytes = (TZrSize)prototype.decoratorsCount * sizeof(TZrUInt32);
        memberBytes = (TZrSize)prototype.membersCount * sizeof(SZrCompiledMemberInfo);
        if (inheritBytes > function->prototypeDataLength - offset) return ZR_NULL;
        offset += inheritBytes;
        if (decoratorBytes > function->prototypeDataLength - offset) return ZR_NULL;
        offset += decoratorBytes;
        if (memberBytes > function->prototypeDataLength - offset) return ZR_NULL;

        for (TZrUInt32 memberIndex = 0u; memberIndex < prototype.membersCount; ++memberIndex) {
            SZrCompiledMemberInfo member;
            const SZrTypeValue *constant;
            memcpy(&member, data + offset + (TZrSize)memberIndex * sizeof(member), sizeof(member));
            if (member.isMetaMethod == 0u || member.metaType != (TZrUInt32)metaType ||
                member.functionConstantIndex >= function->constantValueLength) {
                continue;
            }
            constant = &function->constantValueList[member.functionConstantIndex];
            if (constant->type == ZR_VALUE_TYPE_FUNCTION && constant->value.object != ZR_NULL &&
                constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                return ZR_CAST(const SZrFunction *, constant->value.object);
            }
        }
        offset += memberBytes;
    }
    return ZR_NULL;
}

static TZrUInt32 count_inline_struct_parameter_slots(const SZrFunction *function) {
    TZrUInt32 count = 0u;
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) return 0u;
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; ++index) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        if (layout->isParameter != 0u &&
            layout->slotKind == ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            ++count;
        }
    }
    return count;
}

static TZrUInt32 count_direct_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count = 0u;
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) return 0u;
    for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            ++count;
        }
    }
    return count;
}

static TZrUInt32 count_dynamic_tail_call_opcodes(const SZrFunction *function) {
    return count_direct_opcode(function, ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)) +
           count_direct_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED)) +
           count_direct_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS));
}

static TZrUInt32 count_dynamic_call_opcodes(const SZrFunction *function) {
    return count_direct_opcode(function, ZR_INSTRUCTION_ENUM(DYN_CALL)) +
           count_direct_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED)) +
           count_direct_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS));
}

/* Ordinary PreCall prepends the @call target before the original object and arguments.
 * The dynamic tail path must preserve the same receiver-plus-arguments window when
 * an inline-struct @call parameter declines frame reuse. */
static void test_dynamic_tail_meta_call_preserves_last_argument_after_reuse_declines(void) {
    static const char *ordinarySource =
            "struct Payload { pub var value: int; "
            "pub @constructor(value: int) { this.value = value; } } "
            "class Callable { pri var bias: int; "
            "pub @constructor() { this.bias = 3; } "
            "pub @call(payload: Payload, sentinel: int): int { "
            "return (this.bias * 100) + (payload.value * 10) + sentinel; } } "
            "fn relayOrdinary(target: object, payload: Payload, sentinel: int): object { "
            "var result: object = target(payload, sentinel); return result; } "
            "var target: object = new Callable(); "
            "var payload: Payload = init Payload(4); "
            "return <int> relayOrdinary(target, payload, 7);";
    static const char *tailSource =
            "struct Payload { pub var value: int; "
            "pub @constructor(value: int) { this.value = value; } } "
            "class Callable { pri var bias: int; "
            "pub @constructor() { this.bias = 3; } "
            "pub @call(payload: Payload, sentinel: int): int { "
            "return (this.bias * 100) + (payload.value * 10) + sentinel; } } "
            "fn relayTail(target: object, payload: Payload, sentinel: int): object { "
            "return target(payload, sentinel); } "
            "var target: object = new Callable(); "
            "var payload: Payload = init Payload(4); "
            "return <int> relayTail(target, payload, 7);";
    SZrFunction *ordinaryFunction;
    SZrFunction *tailFunction;
    const SZrFunction *callFunction;
    const SZrFunction *tailRelay;
    const SZrFunction *ordinaryRelay;
    TZrInt64 result = 0;

    ordinaryFunction = compile_source(ordinarySource);
    TEST_ASSERT_NOT_NULL(ordinaryFunction);
    callFunction = find_meta_function_from_prototype_data(ordinaryFunction, ZR_META_CALL);
    ordinaryRelay = find_child_function_by_name(ordinaryFunction, "relayOrdinary");
    TEST_ASSERT_NOT_NULL_MESSAGE(callFunction, "the inline @call body must remain a compiled child function");
    TEST_ASSERT_NOT_NULL_MESSAGE(ordinaryRelay, "the ordinary-call control must remain a compiled child function");
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, count_inline_struct_parameter_slots(callFunction));
    TEST_ASSERT_TRUE_MESSAGE(count_dynamic_call_opcodes(ordinaryRelay) > 0u,
                             "relayOrdinary must retain an ordinary dynamic call");
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, ordinaryFunction, &result));
    TEST_ASSERT_EQUAL_INT64_MESSAGE(347,
                                    result,
                                    "ordinary dynamic-call control must pass before the tail-call case");

    tailFunction = compile_source(tailSource);
    TEST_ASSERT_NOT_NULL(tailFunction);
    callFunction = find_meta_function_from_prototype_data(tailFunction, ZR_META_CALL);
    tailRelay = find_child_function_by_name(tailFunction, "relayTail");
    TEST_ASSERT_NOT_NULL_MESSAGE(callFunction, "the inline @call body must remain in prototype metadata");
    TEST_ASSERT_NOT_NULL_MESSAGE(tailRelay, "the dynamic tail relay must remain a compiled child function");
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, count_inline_struct_parameter_slots(callFunction));
    TEST_ASSERT_TRUE_MESSAGE(count_dynamic_tail_call_opcodes(tailRelay) > 0u,
                             "relayTail must use the DYN_TAIL_CALL opcode family");
    result = 0;
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, tailFunction, &result));
    TEST_ASSERT_EQUAL_INT64_MESSAGE(347, result, "dynamic tail call must preserve receiver and final argument");
}

/* 静态方法编译应产生可执行的带 token 调用。 */
static void test_static_method_has_token_binding(void) {
    assert_bound_result(
            "class Math { pub static fn answer(): int { return 42; } }\n"
            "return Math.answer();\n", 42);
}

/* 字段链访问后最终方法调用应保留运行时结果。 */
static void test_object_chain_preserves_field_reads_and_binds_final_call(void) {
    assert_bound_result(
            "class Leaf { pub fn read(): int { return 17; } }\n"
            "class Middle { pub var leaf: Leaf; pub @constructor() { this.leaf = new Leaf(); } }\n"
            "class Root { pub var middle: Middle; pub @constructor() { this.middle = new Middle(); } }\n"
            "var root = new Root(); return root.middle.leaf.read();\n", 17);
}

/* 未声明成员必须在编译期被拒绝。 */
static void test_unknown_instance_member_call_is_a_compile_error(void) {
    TEST_ASSERT_NULL(compile_source(
            "class Box { pub fn read(): int { return 1; } }\n"
            "var box = new Box(); return box.missing();\n"));
}

static void test_unknown_static_member_is_a_compile_error(void) {
    assert_compile_diagnostic(
            "class Box { pub static fn read(): int { return 1; } }\n"
            "return Box.missing();\n",
            "Unknown static member 'Box.missing'");
}

/* 返回类型不同但参数相同的重载无法唯一绑定，检查诊断文本。 */
static void test_member_overload_ambiguity_is_a_compile_error(void) {
    assert_compile_diagnostic(
            "class Box { pub fn pick(value: int): int { return value; } "
            "pub fn pick(value: int): bool { return true; } } "
            "var box = new Box(); return box.pick(1);",
            "Ambiguous overload for member 'pick'");
}

/* 参数类型不匹配时检查编译器的具体诊断。 */
static void test_member_signature_mismatch_is_a_compile_error(void) {
    assert_compile_diagnostic(
            "class Box { pub fn pick(value: int): int { return value; } } "
            "var box = new Box(); return box.pick(1.5);",
            "Expected 'int' but found 'float'");
}

/* 篡改函数图代际后旧静态绑定应拒绝执行并报告 token。 */
static void test_invalidated_generation_rejects_static_call(void) {
    SZrFunction *function = compile_source(
            "class Math { pub static fn answer(): int { return 42; } } return Math.answer();");
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(find_binding(function));
    ++function->callBindingGeneration;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, state->lastCallBindingError.targetMetadataToken);
}

/* getter 与 setter 应各生成一条操作契约且能完成读写。 */
static void test_property_getter_and_setter_have_binding_contracts(void) {
    SZrFunction *function = compile_source(
            "class Box { pri var stored: int = 1; "
            "pub property value: int { get { return this.stored; } set { this.stored = value; } } } "
            "var box = new Box(); box.value = 31; return box.value;");
    TZrUInt32 getters = 0u, setters = 0u;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    count_property_bindings(function, &getters, &setters);
    TEST_ASSERT_EQUAL_UINT32(1u, getters);
    TEST_ASSERT_EQUAL_UINT32(1u, setters);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(31, result);
}

/* 基类声明的虚调用在运行时应分派到派生类覆盖实现。 */
static void test_virtual_binding_uses_receiver_override(void) {
    assert_bound_result(
            "class Base { pub virtual fn read(): int { return 1; } } "
            "class Derived : Base { pub override fn read(): int { return 23; } } "
            "var box: Base = new Derived(); return box.read();", 23);
}

/* 接口实现调用应得到结果；本例另编译一份函数图只验证成功创建。 */
static void test_interface_binding_uses_contract_slot(void) {
    SZrFunction *function = compile_source(
            "interface Readable { fn read(): int; } "
            "class Box : Readable { pub fn read(): int { return 37; } } "
            "var box: Readable = new Box(); return box.read();");
    TEST_ASSERT_NOT_NULL(function);
    assert_bound_result(
            "interface Readable { fn read(): int; } "
            "class Box : Readable { pub fn read(): int { return 37; } } "
            "var box: Readable = new Box(); return box.read();", 37);
}

/* 同一虚调用点面对两种接收者仍须按覆盖实现分派。 */
static void test_virtual_parameter_dispatches_multiple_receiver_types(void) {
    assert_bound_result(
            "class Base { pub virtual fn read(): int { return 1; } } "
            "class First : Base { pub override fn read(): int { return 17; } } "
            "class Second : Base { pub override fn read(): int { return 25; } } "
            "fn readValue(value: Base): int { return value.read(); } "
            "return readValue(new First()) + readValue(new Second());", 42);
}

/* 不同接口即使槽号相同，也不能把各自的目标混淆。 */
static void test_two_interfaces_with_equal_slots_remain_distinct(void) {
    assert_bound_result(
            "interface Left { fn left(): int; } interface Right { fn right(): int; } "
            "class Both : Left, Right { pub fn left(): int { return 13; } "
            "pub fn right(): int { return 29; } } "
            "fn fromLeft(value: Left): int { return value.left(); } "
            "fn fromRight(value: Right): int { return value.right(); } "
            "var value = new Both(); return fromLeft(value) + fromRight(value);", 42);
}

/* meta @call 的有参与无参形式都须消费绑定目标并返回正确值。 */
static void test_meta_call_consumes_bound_target_with_and_without_arguments(void) {
    assert_bound_result(
            "class Callable { pub @call(value: int): int { return value + 1; } } "
            "var value = new Callable(); return value(41);", 42);
    assert_bound_result(
            "class Callable { pub @call(): int { return 42; } } "
            "var value = new Callable(); return value();", 42);
}

/* meta @call 契约的代际失效应在执行前被拒绝。 */
static void test_meta_call_rejects_stale_binding_generation(void) {
    SZrFunction *function = compile_source(
            "class Callable { pub @call(): int { return 42; } } "
            "var value = new Callable(); return value();");
    const SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    entry = find_binding(function);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_OPERATION_META, entry->binding.contract.operation);
    ++function->callBindingGeneration;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
}

/* 子方法内的绑定也应执行正常，并随整个函数图的代际推进失效。 */
static void test_nested_method_body_is_linked_and_invalidated(void) {
    SZrFunction *function = compile_source(
            "class Box { pub fn read(): int { return 42; } } "
            "class Holder { pub var box: Box; pub @constructor() { this.box = new Box(); } "
            "pub fn read(): int { return this.box.read(); } } "
            "var holder = new Holder(); return holder.read();");
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
    TEST_ASSERT_TRUE(ZrCore_CallBinding_AdvanceGeneration(function));
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
}

/* 接口实现布局改变后旧绑定应给出布局不匹配错误。 */
static void test_interface_implementation_layout_change_is_rejected(void) {
    SZrFunction *function = compile_source(
            "interface Readable { fn read(): int; } "
            "class Box : Readable { pub fn read(): int { return 42; } } "
            "var value: Readable = new Box(); return value.read();");
    TZrInt64 result = 0;
    TZrUInt32 changed = 0u;
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0u; index < function->prototypeInstancesLength; ++index) {
        SZrObjectPrototype *prototype = function->prototypeInstances[index];
        if (prototype != ZR_NULL && prototype->interfaceDispatchCount != 0u) {
            ZrCore_ObjectPrototype_MarkMutation(prototype);
            ++changed;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, changed);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_LAYOUT_MISMATCH, state->lastCallBindingError.status);
}

/* GC 前把函数图放入 VM 栈根，收集后从根重新取回再执行绑定。 */
static void test_bound_graph_survives_full_collection(void) {
    SZrFunction *function = compile_source(
            "class Box { pub fn read(): int { return 42; } } "
            "class Holder { pub var box: Box; pub @constructor() { this.box = new Box(); } "
            "pub fn read(): int { return this.box.read(); } } "
            "var holder = new Holder(); return holder.read();");
    TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Stack_SetRawObjectValue(state, rootSlot, (SZrRawObject *)function);
    state->stackTop.valuePointer = rootSlot + 1;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    function = (SZrFunction *)ZrCore_Stack_GetValue(rootSlot)->value.object;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
}

/* CTest call_binding_pipeline 经此入口运行全部绑定与失效场景。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_static_method_has_token_binding);
    RUN_TEST(test_object_chain_preserves_field_reads_and_binds_final_call);
    RUN_TEST(test_unknown_instance_member_call_is_a_compile_error);
    RUN_TEST(test_unknown_static_member_is_a_compile_error);
    RUN_TEST(test_member_overload_ambiguity_is_a_compile_error);
    RUN_TEST(test_member_signature_mismatch_is_a_compile_error);
    RUN_TEST(test_invalidated_generation_rejects_static_call);
    RUN_TEST(test_property_getter_and_setter_have_binding_contracts);
    RUN_TEST(test_virtual_binding_uses_receiver_override);
    RUN_TEST(test_interface_binding_uses_contract_slot);
    RUN_TEST(test_virtual_parameter_dispatches_multiple_receiver_types);
    RUN_TEST(test_two_interfaces_with_equal_slots_remain_distinct);
    RUN_TEST(test_meta_call_consumes_bound_target_with_and_without_arguments);
    RUN_TEST(test_dynamic_tail_meta_call_preserves_last_argument_after_reuse_declines);
    RUN_TEST(test_meta_call_rejects_stale_binding_generation);
    RUN_TEST(test_nested_method_body_is_linked_and_invalidated);
    RUN_TEST(test_interface_implementation_layout_change_is_rejected);
    RUN_TEST(test_bound_graph_survives_full_collection);
    return UNITY_END();
}
