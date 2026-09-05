#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;
static SZrTypeValue *g_drop_time_weak;
static TZrBool g_drop_time_wake_attempted;
static TZrBool g_drop_time_wake_failed;

static TZrInt64 observe_drop_time_wake(SZrState *state) {
    SZrTypeValue woken;

    ZrCore_Value_ResetAsNull(&woken);
    g_drop_time_wake_attempted = ZR_TRUE;
    if (g_drop_time_weak != ZR_NULL &&
        ZrCore_Ownership_WakeValue(state, &woken, g_drop_time_weak)) {
        g_drop_time_wake_failed = ZR_VALUE_IS_TYPE_NULL(woken.type);
    }
    ZrCore_Ownership_ReleaseValue(state, &woken);
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

static SZrObject *create_resource_object(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "SharedResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    object = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    return object;
}

static void init_direct_unique(SZrObject *object, SZrTypeValue *owner) {
    ZrCore_Value_ResetAsNull(owner);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state, owner, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_NULL(owner->ownershipControl);
}

static SZrFunction *compile_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "resource_shared_weak.zr");
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    ZrParser_Ast_Free(g_state, script);
    return function;
}

static TZrSize compile_and_count_diagnostics(const TZrChar *source,
                                             const TZrChar *diagnosticCode,
                                             EZrStructuredDiagnosticSeverity expectedSeverity) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "resource_shared_cycle.zr");
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrCompilerState cs;
    TZrSize count = 0u;

    TEST_ASSERT_NOT_NULL(script);
    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, script);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    for (TZrSize index = 0u; index < cs.semanticContext->queryDiagnostics.length; index++) {
        const SZrStructuredDiagnostic *diagnostic =
                (const SZrStructuredDiagnostic *)ZrCore_Array_Get(
                        &cs.semanticContext->queryDiagnostics, index);
        if (diagnostic != ZR_NULL && diagnostic->code != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), diagnosticCode) == 0) {
            TEST_ASSERT_EQUAL_INT(expectedSeverity, diagnostic->severity);
            count++;
        }
    }

    if (cs.topLevelFunction != ZR_NULL && cs.topLevelFunction != cs.currentFunction) {
        ZrCore_Function_Free(g_state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    if (cs.currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs.currentFunction);
        cs.currentFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, script);
    return count;
}

static void test_resource_share_creates_one_non_atomic_control_block(void) {
    SZrObject *object = create_resource_object();
    SZrTypeValue unique;
    SZrTypeValue shared;
    SZrOwnershipControl *control;

    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(&shared);

    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, &shared, &unique));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(unique.type));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_SHARED, shared.ownershipKind);
    TEST_ASSERT_NOT_NULL(shared.ownershipControl);
    TEST_ASSERT_EQUAL_PTR(shared.ownershipControl, object->super.ownershipControl);

    control = shared.ownershipControl;
    TEST_ASSERT_EQUAL_UINT32(1U, control->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(1U, control->weakRefCount);
    TEST_ASSERT_TRUE(control->objectIsAlive);
    TEST_ASSERT_FALSE(control->dropInProgress);
    TEST_ASSERT_FALSE(control->usesAtomicRefCounts);
    TEST_ASSERT_NOT_EQUAL(0U, control->isolationDomainId);

    ZrCore_Ownership_ReleaseValue(g_state, &shared);
}

static void test_resource_shared_cannot_be_converted_into_gc_box(void) {
    SZrObject *object = create_resource_object();
    SZrTypeValue unique;
    SZrTypeValue shared;
    SZrTypeValue boxed;

    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(&shared);
    ZrCore_Value_ResetAsNull(&boxed);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, &shared, &unique));
    TEST_ASSERT_FALSE(ZrCore_Ownership_IntoGcBoxValue(g_state, &boxed, &shared));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_SHARED, shared.ownershipKind);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(boxed.type));

    ZrCore_Ownership_ReleaseValue(g_state, &shared);
}

static void test_shared_clone_and_repeated_wake_account_strong_refs(void) {
    SZrObject *object = create_resource_object();
    SZrTypeValue unique;
    SZrTypeValue shared;
    SZrTypeValue clone;
    SZrTypeValue weak;
    SZrTypeValue firstWake;
    SZrTypeValue secondWake;
    SZrOwnershipControl *control;

    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(&shared);
    ZrCore_Value_ResetAsNull(&clone);
    ZrCore_Value_ResetAsNull(&weak);
    ZrCore_Value_ResetAsNull(&firstWake);
    ZrCore_Value_ResetAsNull(&secondWake);

    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, &shared, &unique));
    control = shared.ownershipControl;
    ZrCore_Value_Copy(g_state, &clone, &shared);
    TEST_ASSERT_EQUAL_UINT32(2U, control->strongRefCount);

    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    TEST_ASSERT_EQUAL_UINT32(2U, control->weakRefCount);
    TEST_ASSERT_TRUE(ZrCore_Ownership_WakeValue(g_state, &firstWake, &weak));
    TEST_ASSERT_TRUE(ZrCore_Ownership_WakeValue(g_state, &secondWake, &weak));
    TEST_ASSERT_EQUAL_UINT32(4U, control->strongRefCount);

    ZrCore_Ownership_ReleaseValue(g_state, &firstWake);
    ZrCore_Ownership_ReleaseValue(g_state, &secondWake);
    ZrCore_Ownership_ReleaseValue(g_state, &clone);
    TEST_ASSERT_EQUAL_UINT32(1U, control->strongRefCount);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
}

static void test_many_weak_handles_survive_final_strong_and_wake_to_none(void) {
    enum { WEAK_COUNT = 8 };
    SZrObject *object = create_resource_object();
    SZrTypeValue unique;
    SZrTypeValue shared;
    SZrTypeValue weakValues[WEAK_COUNT];
    SZrTypeValue woken;
    SZrOwnershipControl *control;
    TZrUInt32 index;

    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(&shared);
    ZrCore_Value_ResetAsNull(&woken);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, &shared, &unique));
    control = shared.ownershipControl;

    for (index = 0U; index < WEAK_COUNT; index++) {
        ZrCore_Value_ResetAsNull(&weakValues[index]);
        TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weakValues[index], &shared));
    }
    TEST_ASSERT_EQUAL_UINT32(WEAK_COUNT + 1U, control->weakRefCount);

    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    TEST_ASSERT_EQUAL_UINT32(0U, control->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(WEAK_COUNT, control->weakRefCount);
    TEST_ASSERT_NULL(control->object);
    TEST_ASSERT_FALSE(control->objectIsAlive);
    TEST_ASSERT_FALSE(control->dropInProgress);

    for (index = 0U; index < WEAK_COUNT; index++) {
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_WEAK,
                              weakValues[index].ownershipKind);
        TEST_ASSERT_EQUAL_PTR(control, weakValues[index].ownershipControl);
        TEST_ASSERT_TRUE(ZrCore_Ownership_WakeValue(
                g_state, &woken, &weakValues[index]));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(woken.type));
    }

    for (index = 0U; index < WEAK_COUNT; index++) {
        ZrCore_Ownership_ReleaseValue(g_state, &weakValues[index]);
    }
}

static void test_shared_and_weak_reject_a_different_isolation_domain(void) {
    SZrState *otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object = create_resource_object();
    SZrTypeValue unique;
    SZrTypeValue shared;
    SZrTypeValue weak;
    SZrTypeValue foreignShared;
    SZrTypeValue foreignWake;
    SZrOwnershipControl *control;

    TEST_ASSERT_NOT_NULL(otherState);
    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(&shared);
    ZrCore_Value_ResetAsNull(&weak);
    ZrCore_Value_ResetAsNull(&foreignShared);
    ZrCore_Value_ResetAsNull(&foreignWake);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, &shared, &unique));
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    control = shared.ownershipControl;

    ZrCore_Value_Copy(otherState, &foreignShared, &shared);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(foreignShared.type));
    TEST_ASSERT_TRUE(ZrCore_Ownership_WakeValue(
            otherState, &foreignWake, &weak));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(foreignWake.type));
    TEST_ASSERT_EQUAL_UINT32(1U, control->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(2U, control->weakRefCount);

    ZrTests_Runtime_State_Destroy(otherState);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
}

static void test_resource_shared_surface_runs_clone_wake_and_last_strong_drop(void) {
    SZrFunction *function = compile_source(
            "resource class Tracker {\n"
            "  pub static var dropCount: int = 0;\n"
            "  pub @destructor() { Tracker.dropCount = Tracker.dropCount + 1; }\n"
            "}\n"
            "fn run(): int {\n"
            "  var unique: Unique<Tracker> = own Tracker();\n"
            "  var shared: Shared<Tracker> = share(unique);\n"
            "  var clone: Shared<Tracker> = shared;\n"
            "  var watcher: Weak<Tracker> = degrade(shared);\n"
            "  var woken = wake(watcher);\n"
            "  drop(shared);\n"
            "  drop(clone);\n"
            "  drop(woken);\n"
            "  var expired = wake(watcher);\n"
            "  if (expired == null) { return Tracker.dropCount; }\n"
            "  return 99;\n"
            "}\n"
            "return run();\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_shared_cleanup_runs_on_throw(void) {
    SZrFunction *function = compile_source(
            "resource class Tracker {\n"
            "  pub static var dropCount: int = 0;\n"
            "  pub @destructor() { Tracker.dropCount = Tracker.dropCount + 1; }\n"
            "}\n"
            "fn explode() {\n"
            "  var unique: Unique<Tracker> = own Tracker();\n"
            "  var shared: Shared<Tracker> = share(unique);\n"
            "  var clone: Shared<Tracker> = shared;\n"
            "  var watcher: Weak<Tracker> = degrade(shared);\n"
            "  throw \"stop\";\n"
            "}\n"
            "try { explode(); } catch (error) {}\n"
            "return Tracker.dropCount;\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_shared_value_parameter_releases_its_copy(void) {
    SZrFunction *function = compile_source(
            "resource class Leaf {\n"
            "  pub static var dropCount: int = 0;\n"
            "  pub @destructor() { Leaf.dropCount = Leaf.dropCount + 1; }\n"
            "}\n"
            "fn observe(value: Shared<Leaf>) {}\n"
            "fn run(): int {\n"
            "  var unique: Unique<Leaf> = own Leaf();\n"
            "  var shared: Shared<Leaf> = share(unique);\n"
            "  observe(shared);\n"
            "  drop(shared);\n"
            "  return Leaf.dropCount;\n"
            "}\n"
            "return run();\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_nested_shared_and_weak_fields_release_in_order(void) {
    SZrFunction *function = compile_source(
            "resource class Leaf {\n"
            "  pub static var dropCount: int = 0;\n"
            "  pub @destructor() { Leaf.dropCount = Leaf.dropCount + 1; }\n"
            "}\n"
            "resource class Holder {\n"
            "  var owner: Shared<Leaf>;\n"
            "  var observer: Weak<Leaf>;\n"
            "  pub @constructor(owner: Shared<Leaf>) {\n"
            "    this.owner = owner;\n"
            "    this.observer = degrade(owner);\n"
            "  }\n"
            "}\n"
            "fn run(): int {\n"
            "  var unique: Unique<Leaf> = own Leaf();\n"
            "  var shared: Shared<Leaf> = share(unique);\n"
            "  { var holder: Unique<Holder> = own Holder(shared); }\n"
            "  drop(shared);\n"
            "  return Leaf.dropCount;\n"
            "}\n"
            "return run();\n");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_resource_drop_body_cannot_wake_weak_self(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "DropObserverResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0);
    SZrObject *object;
    SZrTypeValue unique;
    SZrTypeValue shared;
    SZrTypeValue weak;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(destructor);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    destructor->nativeFunction = observe_drop_time_wake;
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
    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(&shared);
    ZrCore_Value_ResetAsNull(&weak);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, &shared, &unique));
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));

    g_drop_time_weak = &weak;
    g_drop_time_wake_attempted = ZR_FALSE;
    g_drop_time_wake_failed = ZR_FALSE;
    ZrCore_Ownership_ReleaseValue(g_state, &shared);

    TEST_ASSERT_TRUE(g_drop_time_wake_attempted);
    TEST_ASSERT_TRUE(g_drop_time_wake_failed);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_WEAK, weak.ownershipKind);
    TEST_ASSERT_NOT_NULL(weak.ownershipControl);
    TEST_ASSERT_FALSE(weak.ownershipControl->objectIsAlive);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
    g_drop_time_weak = ZR_NULL;
}

static void test_resource_shared_field_cycles_publish_process_local_lints(void) {
    const TZrChar *selfCycle =
            "resource class Node {\n"
            "  var next: Shared<Node>;\n"
            "}\n";
    const TZrChar *mutualCycle =
            "resource class Parent {\n"
            "  var child: Shared<Child>;\n"
            "}\n"
            "resource class Child {\n"
            "  var parent: Shared<Parent>;\n"
            "}\n";
    const TZrChar *weakBackEdge =
            "resource class Parent {\n"
            "  var child: Shared<Child>;\n"
            "}\n"
            "resource class Child {\n"
            "  var parent: Weak<Parent>;\n"
            "}\n";

    TEST_ASSERT_EQUAL_UINT32(
            1u,
            (TZrUInt32)compile_and_count_diagnostics(
                    selfCycle,
                    "resource_shared_strong_cycle",
                    ZR_STRUCTURED_DIAGNOSTIC_WARNING));
    TEST_ASSERT_EQUAL_UINT32(
            1u,
            (TZrUInt32)compile_and_count_diagnostics(
                    mutualCycle,
                    "resource_shared_strong_cycle",
                    ZR_STRUCTURED_DIAGNOSTIC_WARNING));
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            (TZrUInt32)compile_and_count_diagnostics(
                    weakBackEdge,
                    "resource_shared_strong_cycle",
                    ZR_STRUCTURED_DIAGNOSTIC_WARNING));
}

#include "test_ownership_pending_control_cases.h"
#include "test_ownership_drop_failure_cases.h"
#include "test_ownership_aot_pending_cases.h"
#include "aot_c_ownership_member_result_cases.h"
#include "test_ownership_member_cache_null_cases.h"
#include "test_ownership_nested_finally_pending_cases.h"
#include "test_ownership_suspended_roots_cases.h"
#include "test_ownership_task_callback_cases.h"
#include "test_ownership_abrupt_roundtrip_cases.h"
#include "test_ownership_loop_finally_cases.h"
#include "test_ownership_receiver_guard_contract_cases.h"
#include "test_ownership_release_domain_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resource_share_creates_one_non_atomic_control_block);
    RUN_TEST(test_resource_shared_cannot_be_converted_into_gc_box);
    RUN_TEST(test_shared_clone_and_repeated_wake_account_strong_refs);
    RUN_TEST(test_many_weak_handles_survive_final_strong_and_wake_to_none);
    RUN_TEST(test_shared_and_weak_reject_a_different_isolation_domain);
    RUN_TEST(test_pending_shared_clear_releases_retention_once);
    RUN_TEST(test_pending_weak_clear_releases_retention_once);
    RUN_TEST(test_pending_valueless_transfer_releases_retention);
    RUN_TEST(test_pending_thread_reset_releases_retention);
    RUN_TEST(test_pending_thread_reset_reports_drop_failure_after_cleanup);
    RUN_TEST(test_pending_self_alias_and_replacement_preserve_value);
    RUN_TEST(test_pending_clear_detaches_before_reentrant_drop);
    RUN_TEST(test_pending_replacement_survives_reentrant_drop);
    RUN_TEST(test_pending_exception_survives_drop_nested_catch);
    RUN_TEST(test_pending_gc_result_survives_reentrant_collection);
    RUN_TEST(test_pending_throwing_drop_finishes_release_and_discards_replacement);
    RUN_TEST(test_drop_failure_releases_callback_pending_return);
    RUN_TEST(test_drop_success_preserves_caller_pending_return);
    RUN_TEST(test_drop_failure_preserves_caller_pending_return);
    RUN_TEST(test_aot_catch_refreshes_frame_after_pending_drop);
    RUN_TEST(test_aot_end_finally_refreshes_frame_after_discarded_pending_drop);
    RUN_TEST(test_aot_throw_normalizes_payload_before_pending_drop);
    RUN_TEST(test_aot_direct_shared_return_materializes_generated_slot);
    RUN_TEST(test_aot_direct_weak_return_materializes_generated_slot);
    RUN_TEST(test_aot_direct_shared_return_preserves_aliased_destination);
    RUN_TEST(test_aot_direct_weak_return_preserves_aliased_destination);
    RUN_TEST(test_aot_value_slot_copy_consumes_shared_and_weak_temporaries);
    RUN_TEST(test_aot_return_refreshes_frame_after_discarded_finally_drop);
    RUN_TEST(test_aot_member_result_outlives_registered_receiver);
    RUN_TEST(test_aot_registered_shared_owner_closes_all_matching_storage);
    RUN_TEST(test_aot_registered_shared_owner_drops_immediately);
    RUN_TEST(test_member_cache_cold_read_write_and_receiver_change);
    RUN_TEST(test_suspended_return_survives_inner_finally_break);
    RUN_TEST(test_suspended_return_survives_inner_finally_continue);
    RUN_TEST(test_suspended_return_runs_remaining_outer_finally);
    RUN_TEST(test_suspended_return_survives_caught_inner_exception);
    RUN_TEST(test_suspended_exception_survives_caught_inner_exception);
    RUN_TEST(test_suspended_shared_return_survives_inner_finally_break);
    RUN_TEST(test_suspended_shared_return_override_releases_original);
    RUN_TEST(test_pending_break_discards_exited_catch_handler);
    RUN_TEST(test_suspended_shared_and_weak_reset_release_retention);
    RUN_TEST(test_suspended_return_and_exception_are_gc_roots);
    RUN_TEST(test_suspended_roots_relocate_only_in_their_own_domain);
    RUN_TEST(test_suspended_drop_survives_handler_array_relocation);
    RUN_TEST(test_suspended_cleanup_keeps_original_exception_through_gc_and_multiple_drops);
    RUN_TEST(test_suspended_cleanup_keeps_first_drop_exception_through_gc);
    RUN_TEST(test_task_result_survives_suspended_owner_cleanup_gc);
    RUN_TEST(test_task_failure_unlinks_abandoned_callback_roots_before_drop);
    RUN_TEST(test_suspended_weak_return_survives_inner_finally_break);
    RUN_TEST(test_suspended_shared_return_throw_override_releases_original);
    RUN_TEST(test_suspended_handler_cleanup_preserves_original_host_failure);
    RUN_TEST(test_suspended_unwind_keeps_original_exception_when_drop_throws);
    RUN_TEST(test_ownership_abrupt_vm_and_binary_roundtrip);
    RUN_TEST(test_shared_call_result_is_not_retained_after_explicit_drop);
    RUN_TEST(test_shared_foreign_release_preserves_owner);
    RUN_TEST(test_weak_foreign_release_preserves_live_and_expired_handle);
    RUN_TEST(test_pending_shared_return_through_nested_finally);
    RUN_TEST(test_pending_return_preserves_value_before_finally_assignment);
    RUN_TEST(test_pending_weak_return_through_nested_finally);
    RUN_TEST(test_pending_return_replaced_by_finally_exception);
    RUN_TEST(test_resource_shared_surface_runs_clone_wake_and_last_strong_drop);
    RUN_TEST(test_resource_shared_cleanup_runs_on_throw);
    RUN_TEST(test_shared_value_parameter_releases_its_copy);
    RUN_TEST(test_resource_nested_shared_and_weak_fields_release_in_order);
    RUN_TEST(test_resource_drop_body_cannot_wake_weak_self);
    RUN_TEST(test_resource_shared_field_cycles_publish_process_local_lints);
    RUN_TEST(test_receiver_guard_lowering_rejects_fact_drift);
    RUN_TEST(test_receiver_guard_lowering_rejects_canonical_receiver_type_drift);
    RUN_TEST(test_receiver_guard_lowering_rejects_guarded_type_drift);
    RUN_TEST(test_receiver_guard_lowering_rejects_missing_nullable_callable_fact);
    RUN_TEST(test_receiver_guard_lowering_rejects_missing_nullable_callable_and_receiver_facts);
    RUN_TEST(test_receiver_guard_lowering_rejects_partial_chain_without_receiver_fact);
    RUN_TEST(test_direct_weak_guard_preserves_shared_result);
    RUN_TEST(test_mixed_weak_guards_preserve_shared_result);
    RUN_TEST(test_mixed_weak_optional_chain_boundaries);
    RUN_TEST(test_repeated_weak_live_expire_wake_transitions);
    RUN_TEST(test_pending_loop_local_transfers_do_not_exit_enclosing_try);
    RUN_TEST(test_loop_local_transfers_lower_without_pending_finally_exit);
    RUN_TEST(test_pending_nested_finally_break_keeps_outer_try_active);
    RUN_TEST(test_pending_nested_finally_continue_keeps_outer_try_active);
    RUN_TEST(test_pending_catch_local_transfers_do_not_exit_finally);
    RUN_TEST(test_pending_return_target_survives_instruction_compaction);
    RUN_TEST(test_pending_for_continue_resolves_existing_loop_label);
    RUN_TEST(test_pending_foreach_continue_resolves_existing_loop_label);
    return UNITY_END();
}
