//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

TZrBool extern_compiler_emit_get_member_to_slot(SZrCompilerState *cs,
                                                       TZrUInt32 destSlot,
                                                       TZrUInt32 objectSlot,
                                                       SZrString *memberName) {
    TZrUInt32 memberId;

    if (cs == ZR_NULL || memberName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    memberId = compiler_get_or_add_member_entry(cs, memberName);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                          (TZrUInt16)destSlot,
                                          (TZrUInt16)objectSlot,
                                          (TZrUInt16)memberId));
    return ZR_TRUE;
}

TZrBool extern_compiler_emit_import_module_to_local(SZrCompilerState *cs,
                                                           SZrString *moduleName,
                                                           TZrUInt32 localSlot,
                                                           SZrFileRange location) {
    TZrUInt32 importSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    importSlot = ZrParser_Compiler_EmitImportModuleExpression(cs, moduleName, location);
    if (importSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }

    if (importSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)importSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

TZrBool extern_compiler_emit_module_function_call_to_local(SZrCompilerState *cs,
                                                                  TZrUInt32 moduleSlot,
                                                                  SZrString *functionName,
                                                                  const SZrTypeValue *argumentValues,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 localSlot,
                                                                  SZrFileRange location) {
    TZrUInt32 functionSlot;

    if (cs == ZR_NULL || functionName == ZR_NULL || (argumentCount > 0 && argumentValues == ZR_NULL) || cs->hasError) {
        return ZR_FALSE;
    }

    functionSlot = allocate_stack_slot(cs);
    if (!extern_compiler_emit_get_member_to_slot(cs, functionSlot, moduleSlot, functionName)) {
        ZrParser_Compiler_Error(cs, "failed to resolve extern ffi module function", location);
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 argumentSlot = allocate_stack_slot(cs);
        emit_constant_to_slot(cs, argumentSlot, &argumentValues[index]);
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)argumentCount));
    if (functionSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)functionSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

TZrBool extern_compiler_emit_method_call_to_local(SZrCompilerState *cs,
                                                         TZrUInt32 receiverSlot,
                                                         SZrString *methodName,
                                                         const SZrTypeValue *argumentValues,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt32 localSlot,
                                                         SZrFileRange location) {
    TZrUInt32 functionSlot;
    TZrUInt32 selfSlot;

    if (cs == ZR_NULL || methodName == ZR_NULL || (argumentCount > 0 && argumentValues == ZR_NULL) || cs->hasError) {
        return ZR_FALSE;
    }

    functionSlot = allocate_stack_slot(cs);
    if (!extern_compiler_emit_get_member_to_slot(cs, functionSlot, receiverSlot, methodName)) {
        ZrParser_Compiler_Error(cs, "failed to resolve extern ffi receiver method", location);
        return ZR_FALSE;
    }

    selfSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)selfSlot,
                                          (TZrInt32)receiverSlot));
    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 argumentSlot = allocate_stack_slot(cs);
        emit_constant_to_slot(cs, argumentSlot, &argumentValues[index]);
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)(argumentCount + 1)));
    if (functionSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)functionSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

// 进入新作用域
