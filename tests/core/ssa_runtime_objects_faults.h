#ifndef ZR_TEST_SSA_RUNTIME_OBJECTS_FAULTS_H
#define ZR_TEST_SSA_RUNTIME_OBJECTS_FAULTS_H
#include <stddef.h>
#include "zr_vm_core/exec_ir_runtime.h"
ZR_CORE_API void ssa_runtime_objects_faults(TZrUInt32 failObject, TZrBool throwOom,
                                TZrBool collectEachObject, TZrUInt32 failReserve);
ZR_CORE_API TZrUInt32 ssa_runtime_objects_allocation_count(void);
ZR_CORE_API void ssa_runtime_objects_fail_native_allocation(size_t ordinal);
ZR_CORE_API TZrBool ssa_runtime_objects_native_allocation_failed(void);
ZR_CORE_API void ssa_runtime_objects_fail_remembered_reservation(void);
ZR_CORE_API void ssa_runtime_objects_fail_pause(void);
ZR_CORE_API void ssa_runtime_objects_set_barrier_hook(void (*hook)(SZrState *, void *), void *context);
ZR_CORE_API TZrBool ssa_runtime_objects_materialize(const SZrExecIrObjectMaterializationRequest *request,
                                       SZrExecIrDiagnostic *diagnostic);
#endif
