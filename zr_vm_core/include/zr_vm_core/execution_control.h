#ifndef ZR_VM_CORE_EXECUTION_CONTROL_H
#define ZR_VM_CORE_EXECUTION_CONTROL_H

#include "zr_vm_core/state.h"

ZR_CORE_API TZrBool execution_push_exception_handler(SZrState *state, SZrCallInfo *callInfo, TZrUInt32 handlerIndex);

ZR_CORE_API SZrVmExceptionHandlerState *execution_find_handler_state(SZrState *state,
                                                                     SZrCallInfo *callInfo,
                                                                     TZrUInt32 handlerIndex);

ZR_CORE_API void execution_pop_exception_handler(SZrState *state, SZrVmExceptionHandlerState *handlerState);

ZR_CORE_API void execution_discard_exception_handlers_for_callinfo(SZrState *state, SZrCallInfo *callInfo);

ZR_CORE_API TZrBool execution_jump_to_instruction_offset(SZrState *state,
                                                         SZrCallInfo **ioCallInfo,
                                                         SZrCallInfo *targetCallInfo,
                                                         TZrMemoryOffset instructionOffset);

ZR_CORE_API void execution_clear_pending_control(SZrState *state);

ZR_CORE_API void execution_set_pending_control(SZrState *state,
                                               EZrVmPendingControlKind kind,
                                               SZrCallInfo *callInfo,
                                               TZrMemoryOffset targetInstructionOffset,
                                               TZrUInt32 valueSlot,
                                               const SZrTypeValue *value);

ZR_CORE_API TZrBool execution_resume_pending_via_outer_finally(SZrState *state, SZrCallInfo **ioCallInfo);

ZR_CORE_API TZrBool execution_unwind_exception_to_handler(SZrState *state, SZrCallInfo **ioCallInfo);

#endif // ZR_VM_CORE_EXECUTION_CONTROL_H
