#ifndef ZR_VM_LIB_TESTING_MODULE_H
#define ZR_VM_LIB_TESTING_MODULE_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_TESTING_API ZR_API
#define ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY 256U
#define ZR_VM_LIB_TESTING_MESSAGE_CAPACITY 256U

typedef enum EZrTestingAssertionKind {
    ZR_TESTING_ASSERTION_KIND_ASSERT = 1,
    ZR_TESTING_ASSERTION_KIND_EQUAL = 2,
    ZR_TESTING_ASSERTION_KIND_THROWS = 3
} EZrTestingAssertionKind;

typedef struct SZrTestingValueSnapshot {
    TZrChar text[ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY + 1U];
    TZrBool truncated;
    TZrBool formatterFaulted;
} SZrTestingValueSnapshot;

typedef struct SZrTestingAssertionFailure {
    EZrTestingAssertionKind assertionKind;
    TZrUInt32 sourceLine;
    TZrChar message[ZR_VM_LIB_TESTING_MESSAGE_CAPACITY + 1U];
    SZrTestingValueSnapshot expected;
    SZrTestingValueSnapshot actual;
    SZrTestingValueSnapshot exception;
} SZrTestingAssertionFailure;

ZR_VM_LIB_TESTING_API const ZrLibModuleDescriptor *ZrVmLibTesting_GetModuleDescriptor(void);
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Register(SZrGlobalState *global);
ZR_VM_LIB_TESTING_API void ZrVmLibTesting_ClearLastFailure(void);
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_GetLastFailure(SZrTestingAssertionFailure *outFailure);
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Assert(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Equal(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Throws(ZrLibCallContext *context, SZrTypeValue *result);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_VM_LIB_TESTING_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif
