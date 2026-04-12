#ifndef ZR_VM_PARSER_BACKEND_AOT_CALLABLE_PROVENANCE_H
#define ZR_VM_PARSER_BACKEND_AOT_CALLABLE_PROVENANCE_H

#include "backend_aot_function_table.h"

TZrUInt32 *backend_aot_allocate_callable_slot_function_indices(SZrState *state, const SZrFunction *function);
void backend_aot_release_callable_slot_function_indices(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrUInt32 *slotFunctionIndices);
TZrUInt32 backend_aot_get_callable_slot_function_index(const TZrUInt32 *slotFunctionIndices,
                                                       const SZrFunction *function,
                                                       TZrUInt32 slotIndex);
void backend_aot_set_callable_slot_function_index(TZrUInt32 *slotFunctionIndices,
                                                  const SZrFunction *function,
                                                  TZrUInt32 slotIndex,
                                                  TZrUInt32 functionIndex);
TZrUInt32 backend_aot_resolve_callable_slot_function_index_before_instruction(const SZrAotFunctionTable *table,
                                                                              SZrState *state,
                                                                              const SZrFunction *function,
                                                                              TZrUInt32 instructionLimit,
                                                                              TZrUInt32 slotIndex,
                                                                              TZrUInt32 recursionDepth);

#endif
