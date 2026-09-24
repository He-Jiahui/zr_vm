#ifndef ZR_TEST_SSA_RUNTIME_OBJECTS_CONCURRENCY_H
#define ZR_TEST_SSA_RUNTIME_OBJECTS_CONCURRENCY_H

#include "zr_vm_core/exec_ir_runtime.h"

/* Keep caller storage rooted while another attached state requests collection
 * at the first attachment barrier. Require collection after materialization
 * returns, and return only after the worker has been joined. */
TZrBool ssa_runtime_objects_competing_collector(
        const SZrExecIrObjectMaterializationRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#endif
