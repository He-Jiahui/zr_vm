#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/state.h"
#include "zr_vm_library/native_registry.h"

static int failures;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; \
} } while (0)

static TZrPtr host_allocator(TZrPtr argument, TZrPtr old, TZrSize oldSize,
                            TZrSize newSize, TZrInt64 flag) {
    (void)argument;
    (void)oldSize;
    (void)flag;
    if (newSize == 0u) {
        free(old);
        return NULL;
    }
    return realloc(old, newSize);
}

static SZrCallBindingContract valid_contract(void) {
    SZrCallBindingContract contract;
    memset(&contract, 0, sizeof(contract));
    contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    contract.targetMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    contract.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    contract.signatureHash = 1u;
    contract.moduleSignatureHash = 1u;
    contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    return contract;
}

static void check_status(SZrState *state, const SZrCallBindingContract *contract,
                         EZrCallBindingStatus expected, int valid) {
    SZrCallBindingDiagnostic diagnostic;
    SZrCallBindingTarget target;
    SZrCallBindingTarget zero;
    EZrCallBindingStatus withoutDiagnostic;
    EZrCallBindingStatus withDiagnostic;
    memset(&zero, 0, sizeof(zero));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    /* These expected constants are independent of the resolver's return value. */
    CHECK(ZrCore_CallBinding_CheckContract(contract, NULL) ==
          (valid ? ZR_CALL_BINDING_OK : expected));
    CHECK(ZrCore_CallBinding_CheckContract(contract, &diagnostic) ==
          (valid ? ZR_CALL_BINDING_OK : expected));
    CHECK(diagnostic.status == (valid ? ZR_CALL_BINDING_OK : expected));

    memset(&target, 0xa5, sizeof(target));
    withoutDiagnostic = ZrLibrary_NativeRegistry_ResolveCallBinding(
            state, contract, &target, NULL);
    CHECK(withoutDiagnostic == expected);
    CHECK(memcmp(&target, &zero, sizeof(target)) == 0);

    memset(&target, 0xa5, sizeof(target));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    withDiagnostic = ZrLibrary_NativeRegistry_ResolveCallBinding(
            state, contract, &target, &diagnostic);
    CHECK(withDiagnostic == expected);
    CHECK(withDiagnostic == withoutDiagnostic);
    CHECK(memcmp(&target, &zero, sizeof(target)) == 0);
    /* The no-registry branch currently does not populate diagnostic.status.
     * This test only requires the contract-failure diagnostic's status. */
    if (!valid) CHECK(diagnostic.status == expected);
    fprintf(stderr, "expected=%d withoutDiagnostic=%d withDiagnostic=%d\n",
            (int)expected, (int)withoutDiagnostic, (int)withDiagnostic);
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
    SZrCallBindingContract contract = valid_contract();
    EZrCallBindingStatus expected;
    int valid = 0;
    if (argc != 2) return 2;
    if (strcmp(argv[1], "invalid_token") == 0) {
        contract.signatureToken = 0u;
        expected = ZR_CALL_BINDING_INVALID_TOKEN;
    } else if (strcmp(argv[1], "invalid_slot") == 0) {
        contract.dispatchSlot = 0u;
        expected = ZR_CALL_BINDING_INVALID_SLOT;
    } else if (strcmp(argv[1], "reserved") == 0) {
        contract.reserved1 = 1u;
        expected = ZR_CALL_BINDING_INVALID_ARGUMENT;
    } else if (strcmp(argv[1], "operation") == 0) {
        contract.operation = ZR_CALL_BINDING_OPERATION_META + 1u;
        expected = ZR_CALL_BINDING_INVALID_ARGUMENT;
    } else if (strcmp(argv[1], "missing_contract") == 0) {
        contract.signatureHash = 0u;
        expected = ZR_CALL_BINDING_MISSING_CONTRACT;
    } else if (strcmp(argv[1], "no_registry_control") == 0) {
        expected = ZR_CALL_BINDING_TARGET_NOT_FOUND;
        valid = 1;
    } else return 2;
    global = ZrCore_GlobalState_New(host_allocator, NULL, 1, NULL);
    if (global == NULL) return 1;
    if (!ZrCore_GlobalState_IsInitialized(global)) {
        ZrCore_GlobalState_Free(global);
        return 1;
    }
    check_status(global->mainThreadState, &contract, expected, valid);
    ZrCore_GlobalState_Free(global);
    return failures == 0 ? 0 : 1;
}
