#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "zr_vm_common/zr_ffi_contract.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/native_call_contract.h"

static void native_abi_make_scalar_contract(
        SZrNativeImportContract *contract,
        EZrFfiDirection direction) {
    SZrFfiParameterContract *parameter;
    SZrFfiCallableParameterContract *callableParameter;

    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_FFI_CONTRACT_SCHEMA_VERSION;
    strcpy(contract->libraryLocator, "fixture");
    strcpy(contract->entryPoint, "add");
    strcpy(contract->sourceMapping.document, "native_abi.zr");
    contract->sourceMapping.endOffset = 1u;
    contract->declaringModuleId = 1u;
    contract->availability = ZR_FFI_CONTRACT_AVAILABILITY_ALL;
    contract->requiredCapabilities = ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME;

    contract->signature.abi = ZR_FFI_CONTRACT_ABI_C;
    contract->signature.targetPointerSize = (TZrUInt32)sizeof(void *);
    contract->signature.targetEndianness = ZR_IO_IS_LITTLE_ENDIAN
                                                   ? ZR_FFI_CONTRACT_ENDIAN_LITTLE
                                                   : ZR_FFI_CONTRACT_ENDIAN_BIG;
    strcpy(contract->signature.targetTriple,
           ZrCommon_FfiContract_GetHostTargetTriple());
    contract->signature.targetAbiHash = ZrCommon_FfiContract_ComputeTargetAbiHash(
            contract->signature.abi,
            contract->signature.targetPointerSize,
            contract->signature.targetEndianness,
            contract->signature.targetTriple);
    contract->signature.charset = ZR_FFI_CONTRACT_CHARSET_NONE;
    contract->signature.errorPolicy = ZR_FFI_CONTRACT_ERROR_NONE;
    contract->signature.cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_NONE;
    contract->signature.callbackLifetime = ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE;
    contract->signature.callbackThreadPolicy = ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE;
    contract->signature.callbackExceptionPolicy = ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE;
    contract->signature.parameterCount = 1u;
    contract->signature.returnType.typeKind = ZR_FFI_CONTRACT_TYPE_I32;
    contract->signature.returnType.size = sizeof(TZrInt32);
    contract->signature.returnType.alignment = alignof(TZrInt32);
    contract->signature.returnType.canonicalTypeHash = 11u;
    contract->signature.returnType.flags = ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE;

    parameter = &contract->signature.parameters[0];
    parameter->type.typeKind = ZR_FFI_CONTRACT_TYPE_I32;
    parameter->type.size = sizeof(TZrInt32);
    parameter->type.alignment = alignof(TZrInt32);
    parameter->type.canonicalTypeHash = 12u;
    parameter->type.flags = ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE;
    parameter->direction = direction;
    parameter->marshalling = ZR_FFI_CONTRACT_MARSHALLING_DIRECT;
    parameter->ownership = ZR_FFI_CONTRACT_OWNERSHIP_BORROWED;

    contract->callable.parameterCount = 1u;
    callableParameter = &contract->callable.parameters[0];
    callableParameter->canonicalTypeHash = parameter->type.canonicalTypeHash;
    callableParameter->escapeUpperBound = ZR_FFI_CALLABLE_ESCAPE_FUNCTION;
    callableParameter->acceptsTemporary = direction == ZR_FFI_CONTRACT_DIRECTION_IN;
    if (direction == ZR_FFI_CONTRACT_DIRECTION_IN) {
        callableParameter->passingForm = ZR_FFI_CALLABLE_PASSING_VALUE;
        callableParameter->entryInitialization = ZR_FFI_CALLABLE_ENTRY_INITIALIZED;
        callableParameter->exitInitialization = ZR_FFI_CALLABLE_EXIT_UNCHANGED;
        callableParameter->callSiteMarker = ZR_FFI_CALLABLE_CALL_SITE_NONE;
    } else if (direction == ZR_FFI_CONTRACT_DIRECTION_REF) {
        callableParameter->passingForm = ZR_FFI_CALLABLE_PASSING_REF;
        callableParameter->entryInitialization = ZR_FFI_CALLABLE_ENTRY_INITIALIZED;
        callableParameter->exitInitialization = ZR_FFI_CALLABLE_EXIT_UNCHANGED;
        callableParameter->callSiteMarker = ZR_FFI_CALLABLE_CALL_SITE_REF;
    } else {
        callableParameter->passingForm = ZR_FFI_CALLABLE_PASSING_OUT;
        callableParameter->entryInitialization = ZR_FFI_CALLABLE_ENTRY_UNINITIALIZED;
        callableParameter->exitInitialization = ZR_FFI_CALLABLE_EXIT_DEFINITELY_INITIALIZED;
        callableParameter->callSiteMarker = ZR_FFI_CALLABLE_CALL_SITE_OUT;
    }
    contract->callable.returnTypeHash = contract->signature.returnType.canonicalTypeHash;
    contract->callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract->callable);
    contract->signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract->signature);
}

static void native_abi_make_struct_contract(SZrNativeImportContract *contract) {
    SZrFfiParameterContract *parameter;
    SZrFfiAggregateFieldContract *field;

    native_abi_make_scalar_contract(contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    parameter = &contract->signature.parameters[0];
    parameter->type.typeKind = ZR_FFI_CONTRACT_TYPE_STRUCT;
    parameter->type.size = 8u;
    parameter->type.alignment = alignof(TZrInt32);
    parameter->type.canonicalTypeHash = 31u;
    parameter->type.layoutHash = 32u;
    parameter->type.flags = ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE;
    parameter->type.aggregateFieldStart = 0u;
    parameter->type.aggregateFieldCount = 2u;
    contract->signature.aggregateFieldCount = 2u;

    field = &contract->signature.aggregateFields[0];
    strcpy(field->name, "left");
    field->typeKind = ZR_FFI_CONTRACT_TYPE_I32;
    field->size = sizeof(TZrInt32);
    field->alignment = alignof(TZrInt32);
    field->offset = 0u;
    field = &contract->signature.aggregateFields[1];
    strcpy(field->name, "right");
    field->typeKind = ZR_FFI_CONTRACT_TYPE_I32;
    field->size = sizeof(TZrInt32);
    field->alignment = alignof(TZrInt32);
    field->offset = sizeof(TZrInt32);

    contract->callable.parameters[0].canonicalTypeHash =
            parameter->type.canonicalTypeHash;
    contract->callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract->callable);
    contract->signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract->signature);
}

static void test_prepare_and_marshal_lanes(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallMarshalRequest marshalRequest;
    SZrNativeCallMarshalResult marshalResult;
    SZrNativeCallDiagnostic diagnostic;
    TZrInt32 source = 41;
    TZrInt32 destination = 0;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.expectedSignatureHash = contract.signature.signatureHash;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.sourceId == contract.declaringModuleId);
    assert(plan.abi == contract.signature.abi);
    assert(plan.charset == contract.signature.charset);
    assert(plan.directCompatible);
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_DIRECT);

    memset(&marshalRequest, 0, sizeof(marshalRequest));
    marshalRequest.source = &source;
    marshalRequest.sourceSize = sizeof(source);
    marshalRequest.sourceInitialized = ZR_TRUE;
    assert(ZrCore_NativeCall_MarshalArgument(
            &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
    assert(marshalResult.pointer == &source);
    assert(!marshalResult.usedTemporary);

    request.allowDirect = ZR_FALSE;
    request.nativeAddressStable = ZR_FALSE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(!plan.directCompatible);
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);
    marshalRequest.destination = &destination;
    marshalRequest.destinationCapacity = sizeof(destination);
    assert(ZrCore_NativeCall_MarshalArgument(
            &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
    assert(marshalResult.pointer == &destination);
    assert(marshalResult.usedTemporary);
    assert(destination == source);

    marshalRequest.destination = ZR_NULL;
    marshalRequest.destinationCapacity = 0u;
    assert(!ZrCore_NativeCall_MarshalArgument(
            &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_NULL_ARGUMENT);
    assert(diagnostic.sourceId == plan.sourceId);
}

static void test_zero_parameter_variadic_plan_stays_on_bridge_lane(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    contract.signature.parameterCount = 0u;
    contract.signature.isVariadic = ZR_TRUE;
    contract.callable.parameterCount = 0u;
    contract.callable.isVariadic = ZR_TRUE;
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);

    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.parameterCount == 0u);
    assert(plan.isVariadic == ZR_TRUE);
    assert(plan.directCompatible == ZR_FALSE);
    assert(plan.requiresBridge == ZR_TRUE);
    assert((plan.flags & ZR_NATIVE_CALL_PLAN_FLAG_HAS_VARARGS) != 0u);
    assert((plan.flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE) != 0u);
    assert(ZrCore_NativeCall_ValidatePlan(&plan, &diagnostic));
}

static void test_ref_out_copy_and_writeback(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallMarshalRequest marshalRequest;
    SZrNativeCallMarshalResult marshalResult;
    SZrNativeCallDiagnostic diagnostic;
    TZrInt32 source = 7;
    TZrInt32 temporary = 0;
    TZrInt32 destination = 0;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_REF);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].abiClass == ZR_NATIVE_CALL_ABI_CLASS_REF_OUT);
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);
    assert((plan.flags & ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_WRITEBACK) != 0u);

    memset(&marshalRequest, 0, sizeof(marshalRequest));
    marshalRequest.source = &source;
    marshalRequest.sourceSize = sizeof(source);
    marshalRequest.destination = &temporary;
    marshalRequest.destinationCapacity = sizeof(temporary);
    marshalRequest.sourceInitialized = ZR_TRUE;
    assert(ZrCore_NativeCall_MarshalArgument(
            &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
    temporary = 99;
    assert(ZrCore_NativeCall_WriteBackArgument(
            &plan, 0u, &temporary, sizeof(temporary), &destination,
            sizeof(destination), &diagnostic));
    assert(destination == 99);
}

static void test_struct_direct_and_copy_lanes(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_struct_contract(&contract);
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].abiClass ==
           ZR_NATIVE_CALL_ABI_CLASS_INLINE_STRUCT);
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_DIRECT);

    contract.signature.parameters[0].marshalling =
            ZR_FFI_CONTRACT_MARSHALLING_COPY;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);
    assert(!plan.directCompatible);
}

static void test_callback_unregister_waits_for_in_flight(void) {
    SZrNativeCallbackSlot slot;
    SZrNativeCallDiagnostic diagnostic;

    memset(&slot, 0, sizeof(slot));
    assert(ZrCore_NativeCallback_Register(&slot, 0xabc, 3u, &diagnostic));
    assert(ZrCore_NativeCallback_Enter(&slot, 0xabc, &diagnostic));
    assert(ZrCore_NativeCallback_BeginUnregister(&slot, 0xabc, &diagnostic));
    assert(!ZrCore_NativeCallback_FinishUnregister(&slot, 0xabc, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT);
    ZrCore_NativeCallback_Leave(&slot);
    assert(ZrCore_NativeCallback_FinishUnregister(&slot, 0xabc, &diagnostic));
    assert(!ZrCore_NativeCallback_Enter(&slot, 0xabc, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_CALLBACK_REJECTED);
}

static void test_callback_plan_requires_identity_and_root(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;
    SZrFfiParameterContract *parameter;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    parameter = &contract.signature.parameters[0];
    parameter->type.typeKind = ZR_FFI_CONTRACT_TYPE_CALLBACK;
    parameter->type.size = (TZrUInt32)sizeof(TZrPtr);
    parameter->type.alignment = (TZrUInt32)alignof(TZrPtr);
    parameter->type.canonicalTypeHash = 41u;
    parameter->type.layoutHash = 42u;
    contract.signature.callbackLifetime =
            ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL;
    contract.signature.callbackThreadPolicy =
            ZR_FFI_CONTRACT_CALLBACK_THREAD_CALLER;
    contract.signature.callbackExceptionPolicy =
            ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_RETURN_DEFAULT;
    contract.callable.parameters[0].canonicalTypeHash =
            parameter->type.canonicalTypeHash;
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));

    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.callbackId = 0xfeedu;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].abiClass == ZR_NATIVE_CALL_ABI_CLASS_CALLBACK);
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_BRIDGE);
    assert((plan.marshalOps[0].flags &
            ZR_NATIVE_CALL_MARSHAL_FLAG_REQUIRES_ROOT) != 0u);
    assert(plan.rootCount == 1u);
    assert((plan.flags & ZR_NATIVE_CALL_PLAN_FLAG_HAS_CALLBACK) != 0u);
    assert((plan.flags & ZR_NATIVE_CALL_PLAN_FLAG_CALLBACK_RELOAD) != 0u);
    assert(ZrCore_NativeCall_ValidatePlan(&plan, &diagnostic));

    request.callbackId = 0u;
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_CALLBACK_UNRESOLVED);
}

static void test_invalid_abi_is_rejected(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.targetAbiHash = contract.signature.targetAbiHash ^ UINT64_C(1);
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_ABI_MISMATCH);
}

static void test_source_identity_is_full_width_on_request_failures(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;
    const TZrUInt64 symbolId = UINT64_C(0x100000001);

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    contract.symbolId = symbolId;
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.allowDirect = 2u; /* malformed request-side boolean */
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT);
    assert(diagnostic.sourceId == symbolId);
}

static void test_callable_and_signature_hashes_must_agree(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    contract.callable.parameters[0].canonicalTypeHash ^= UINT64_C(1);
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    /* The common contract validator authenticates each hash independently;
     * the native boundary must also compare the two views of each slot. */
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_INVALID_CONTRACT);
    assert(diagnostic.parameterIndex == 0u);
}

static void test_escape_ownership_requires_identity_bearing_type(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    contract.signature.parameters[0].ownership =
            ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_UNSUPPORTED_MARSHALLING);
}

static void test_scalar_width_is_rejected_even_when_hashes_match(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    contract.signature.parameters[0].type.typeKind =
            ZR_FFI_CONTRACT_TYPE_I64;
    /* Deliberately use a bad width and recompute the producer hashes.  The
     * common contract validator checks shape/hashes, while the native ABI
     * validator must still reject this impossible C representation. */
    contract.signature.parameters[0].type.size = 4u;
    contract.signature.parameters[0].type.alignment = 4u;
    contract.signature.parameters[0].type.canonicalTypeHash = 17u;
    contract.callable.parameters[0].canonicalTypeHash = 17u;
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));

    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_ABI_MISMATCH);
    assert(diagnostic.sourceId == (TZrUInt32)contract.declaringModuleId);
    assert(plan.magic == 0u && plan.valid == ZR_FALSE);
}

static void test_layout_and_class_contracts_are_checked(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));

    request.expectedLayoutHash = plan.layoutHash ^ UINT64_C(1);
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_LAYOUT_MISMATCH);

    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.parameterClasses[0] = ZR_NATIVE_CALL_ABI_CLASS_CONTIGUOUS_VIEW;
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_UNSUPPORTED_TYPE);
    assert(plan.magic == 0u && plan.valid == ZR_FALSE);

    /* The cached representation is self-authenticating; neither its hash nor
     * its derived bridge flags may be changed after publication. */
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    plan.planHash ^= UINT64_C(1);
    assert(!ZrCore_NativeCall_ValidatePlan(&plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_INVALID_PLAN);

    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    plan.flags |= ZR_NATIVE_CALL_PLAN_FLAG_REQUIRES_BRIDGE;
    plan.planHash = 0u;
    assert(!ZrCore_NativeCall_ValidatePlan(&plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_INVALID_PLAN);
}

static void test_aggregate_layout_is_checked(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_struct_contract(&contract);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;

    /* The common contract intentionally permits backend-specific aggregate
     * layouts.  The shared native boundary must still reject overlapping or
     * misaligned C fields before publishing a cached plan. */
    contract.signature.aggregateFields[1].offset = 3u;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_ABI_MISMATCH);
    assert(plan.magic == 0u && plan.valid == ZR_FALSE);

    /* A packed-but-well-ordered aggregate remains marshalable, but its
     * natural field alignment is not sufficient for a direct lane. */
    native_abi_make_struct_contract(&contract);
    contract.signature.parameters[0].type.alignment = 1u;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    request.contract = &contract;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);

    /* Lowering the field alignment is another valid packed representation:
     * it must stay on the explicit copy lane rather than being rejected as
     * malformed or presented to a C by-value call with an unaligned field. */
    contract.signature.aggregateFields[0].alignment = 1u;
    contract.signature.aggregateFields[1].alignment = 1u;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);

}

static void test_out_argument_does_not_read_uninitialized_source(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallMarshalRequest marshalRequest;
    SZrNativeCallMarshalResult marshalResult;
    SZrNativeCallDiagnostic diagnostic;
    TZrInt32 source = 123;
    TZrInt32 temporary = 7;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_OUT);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    memset(&marshalRequest, 0, sizeof(marshalRequest));
    marshalRequest.source = &source;
    /* OUT must not inspect or copy an input slot, even when a caller happens
     * to provide one. */
    marshalRequest.sourceSize = 0u;
    marshalRequest.sourceInitialized = ZR_TRUE;
    marshalRequest.destination = &temporary;
    marshalRequest.destinationCapacity = sizeof(temporary);
    assert(ZrCore_NativeCall_MarshalArgument(
            &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
    assert(marshalResult.usedTemporary);
    assert(marshalResult.requiresWriteback);
    assert(temporary == 0);
}

static void test_alignment_is_checked_before_direct_or_copy(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallMarshalRequest marshalRequest;
    SZrNativeCallMarshalResult marshalResult;
    SZrNativeCallDiagnostic diagnostic;
    TZrByte bytes[sizeof(TZrInt32) + 2u];
    void *misaligned;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    memset(&marshalRequest, 0, sizeof(marshalRequest));
    misaligned = (uintptr_t)bytes % alignof(TZrInt32) == 0u
                         ? (void *)(bytes + 1u)
                         : (void *)bytes;
    marshalRequest.source = misaligned;
    marshalRequest.sourceSize = sizeof(TZrInt32);
    marshalRequest.sourceInitialized = ZR_TRUE;
    assert(!ZrCore_NativeCall_MarshalArgument(
            &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_ALIGNMENT);
}

static void test_non_natural_parameter_alignment_uses_copy_lane(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    /* The value width is still a valid i32 ABI witness; only the serialized
     * storage alignment is reduced.  An aligned temporary repairs this for a
     * native call, so preparation must downgrade rather than reject it. */
    contract.signature.parameters[0].type.alignment = 1u;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);
    assert(plan.marshalOps[0].byteAlignment == alignof(TZrInt32));
    assert(!plan.directCompatible);
}

static void test_unregister_zero_spin_succeeds_when_quiescent(void) {
    SZrNativeCallbackSlot slot;
    SZrNativeCallDiagnostic diagnostic;

    memset(&slot, 0, sizeof(slot));
    assert(ZrCore_NativeCallback_Register(&slot, 0x1234u, 1u, &diagnostic));
    assert(ZrCore_NativeCallback_Unregister(
            &slot, 0x1234u, 0u, &diagnostic));
    assert(slot.state == ZR_NATIVE_CALLBACK_SLOT_UNREGISTERED);
}

static void test_pointer_and_registered_lanes_are_conservative(void) {
    SZrNativeImportContract contract;
    SZrFfiParameterContract *parameter;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_IN);
    parameter = &contract.signature.parameters[0];
    parameter->type.typeKind = ZR_FFI_CONTRACT_TYPE_POINTER;
    parameter->type.size = (TZrUInt32)sizeof(void *);
    parameter->type.alignment = (TZrUInt32)alignof(void *);
    parameter->type.canonicalTypeHash = 21u;
    parameter->type.layoutHash = 22u;
    parameter->marshalling = ZR_FFI_CONTRACT_MARSHALLING_PIN;
    contract.callable.parameters[0].canonicalTypeHash =
            parameter->type.canonicalTypeHash;
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCommon_NativeImportContract_Validate(&contract));

    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.allowDirect = ZR_TRUE;
    request.nativeAddressStable = ZR_TRUE;
    request.parameterClasses[0] = ZR_NATIVE_CALL_ABI_CLASS_TYPED_ARRAY;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind ==
           ZR_NATIVE_CALL_MARSHAL_PINNED_DIRECT);
    assert(plan.pinCount == 1u);

    parameter->marshalling = ZR_FFI_CONTRACT_MARSHALLING_REGISTERED;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_REGISTERED);
    assert(plan.rootCount == 1u);
    assert(plan.requiresBridge);
    {
        TZrPtr registeredStorage = (TZrPtr)(uintptr_t)0x1234u;
        SZrNativeCallMarshalRequest marshalRequest;
        SZrNativeCallMarshalResult marshalResult;

        memset(&marshalRequest, 0, sizeof(marshalRequest));
        marshalRequest.source = &registeredStorage;
        marshalRequest.sourceSize = sizeof(registeredStorage);
        marshalRequest.sourceInitialized = ZR_TRUE;
        assert(ZrCore_NativeCall_MarshalArgument(
                &plan, 0u, &marshalRequest, &marshalResult, &diagnostic));
        assert(marshalResult.pointer == &registeredStorage);
        assert(!marshalResult.usedTemporary);
    }

    parameter->marshalling = ZR_FFI_CONTRACT_MARSHALLING_DIRECT;
    parameter->ownership = ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_COPY);
    assert(plan.pinCount == 0u && plan.rootCount == 1u);

    contract.signature.isVariadic = ZR_TRUE;
    contract.callable.isVariadic = ZR_TRUE;
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    parameter->ownership = ZR_FFI_CONTRACT_OWNERSHIP_BORROWED;
    parameter->marshalling = ZR_FFI_CONTRACT_MARSHALLING_DIRECT;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    request.parameterClasses[0] = ZR_NATIVE_CALL_ABI_CLASS_AUTO;
    assert(ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(plan.marshalOps[0].abiClass == ZR_NATIVE_CALL_ABI_CLASS_VARARGS);
    assert(plan.marshalOps[0].marshalKind == ZR_NATIVE_CALL_MARSHAL_BRIDGE);
    assert(plan.rootCount == 1u && plan.requiresBridge);
}

static void test_native_safepoint_policy_rejects_managed_lanes(void) {
    SZrNativeImportContract contract;
    SZrNativeCallRequest request;
    SZrNativeCallPlan plan;
    SZrNativeCallDiagnostic diagnostic;

    native_abi_make_scalar_contract(&contract, ZR_FFI_CONTRACT_DIRECTION_REF);
    memset(&request, 0, sizeof(request));
    request.contract = &contract;
    request.flags = ZR_NATIVE_CALL_REQUEST_FLAG_BLOCKING_DETACHED;
    assert(!ZrCore_NativeCall_Prepare(&request, &plan, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_THREAD_POLICY);
}

static void test_lease_requires_an_active_domain(void) {
    SZrNativeCallLease lease;
    SZrNativeCallDiagnostic diagnostic;

    memset(&lease, 0, sizeof(lease));
    assert(!ZrCore_NativeCall_LeasePinValue(
            &lease, ZR_NULL, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_NOT_ACTIVE);
    assert(!ZrCore_NativeCall_LeaseRootObject(
            &lease, ZR_NULL, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_NOT_ACTIVE);
    ZrCore_NativeCall_LeaseEnd(&lease, &diagnostic);
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_OK);
    assert(!ZrCore_NativeCall_Attach(ZR_NULL, ZR_NULL, &diagnostic));
    assert(diagnostic.status == ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT);
}

int main(void) {
    test_prepare_and_marshal_lanes();
    test_zero_parameter_variadic_plan_stays_on_bridge_lane();
    test_ref_out_copy_and_writeback();
    test_struct_direct_and_copy_lanes();
    test_callback_unregister_waits_for_in_flight();
    test_callback_plan_requires_identity_and_root();
    test_invalid_abi_is_rejected();
    test_source_identity_is_full_width_on_request_failures();
    test_callable_and_signature_hashes_must_agree();
    test_escape_ownership_requires_identity_bearing_type();
    test_scalar_width_is_rejected_even_when_hashes_match();
    test_layout_and_class_contracts_are_checked();
    test_aggregate_layout_is_checked();
    test_out_argument_does_not_read_uninitialized_source();
    test_alignment_is_checked_before_direct_or_copy();
    test_non_natural_parameter_alignment_uses_copy_lane();
    test_unregister_zero_spin_succeeds_when_quiescent();
    test_pointer_and_registered_lanes_are_conservative();
    test_native_safepoint_policy_rejects_managed_lanes();
    test_lease_requires_an_active_domain();
    return 0;
}
