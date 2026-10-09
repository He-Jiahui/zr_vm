#ifndef ZR_VM_CORE_FUNCTION_AOT_IR_H
#define ZR_VM_CORE_FUNCTION_AOT_IR_H

#include "zr_vm_core/aot_ir.h"
#include "zr_vm_core/function.h"

/**
 * @brief Associate an independent immutable Core metadata snapshot with a Function.
 * @pre Every non-NULL Function is live, current-sized, initialized by the current
 *      runtime and rebuilt with current headers. Mutation and lifecycle calls
 *      require external serialization. Source and all its arrays remain readable
 *      and unchanged during the call; diagnostic does not overlap source, the
 *      Function owner slot or the old arena.
 * @note Successful replacement releases the previous unique owner after cloning;
 *       failure preserves it. NULL Function/source returns INVALID_ARGUMENT.
 *       Source may be the current borrowed view. This association neither checks
 *       executable identity nor installs execution entries or managed references.
 *       The owner is runtime-only; artifact schema6 and AOT ABI17 are unchanged.
 */
ZR_CORE_API EZrAotIrStatus ZrCore_Function_SnapshotAotIr(
        SZrFunction *function, const SZrAotIrModule *source,
        SZrAotIrDiagnostic *diagnostic);

/**
 * @brief Borrow immutable metadata; NULL or an unattached Function returns NULL.
 * @pre Non-NULL Function satisfies the current-runtime requirements above.
 * @note The borrow ends on successful replacement, destruction or a record move.
 *       Acquire the destination view after a move. No concurrent borrow lease or
 *       compatibility with older, smaller Function records is provided.
 */
ZR_CORE_API const SZrAotIrModule *ZrCore_Function_GetAotIr(
        const SZrFunction *function);

/**
 * @brief Clear owner aliases in freshly copied immediate child shells only.
 * @pre copiedChildren names count current-sized, freshly shallow-copied shells
 *      whose owner pointers still alias the source owners, before publication or
 *      destruction. This operation must never be applied to actual unique owners.
 * @note NULL storage is a no-op. It does not free, allocate or recurse into shared
 *       descendants, and does not change the original records. Nested storage
 *       remains borrowed from the original graph and requires its lifetime.
 *       Other copied buffers remain borrowed: arbitrary copied shells cannot be
 *       independently destroyed through Function_Free.
 */
ZR_CORE_API void ZrCore_Function_ForgetCopiedAotIrOwnerAliases(
        SZrFunction *copiedChildren, TZrUInt32 count);

#endif /* ZR_VM_CORE_FUNCTION_AOT_IR_H */
