//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

void emit_object_field_assignment_from_expression(SZrCompilerState *cs,
                                                         TZrUInt32 objectSlot,
                                                         SZrString *fieldName,
                                                         SZrAstNode *expression) {
    if (cs == ZR_NULL || fieldName == ZR_NULL || expression == ZR_NULL || cs->hasError) {
        return;
    }

    ZrParser_Expression_Compile(cs, expression);
    if (cs->hasError || cs->stackSlotCount == 0) {
        return;
    }

    TZrUInt32 valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    TZrUInt32 keySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, keySlot, fieldName);

    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)valueSlot,
                                                       (TZrUInt16)objectSlot, (TZrUInt16)keySlot);
    emit_instruction(cs, setTableInst);
}

void emit_class_static_field_initializers(SZrCompilerState *cs, SZrAstNode *classNode) {
    if (cs == ZR_NULL || classNode == ZR_NULL || classNode->type != ZR_AST_CLASS_DECLARATION || cs->hasError) {
        return;
    }

    SZrClassDeclaration *classDecl = &classNode->data.classDeclaration;
    if (classDecl->name == ZR_NULL || classDecl->name->name == ZR_NULL || classDecl->members == ZR_NULL) {
        return;
    }

    TZrUInt32 prototypeSlot = (TZrUInt32)-1;
    for (TZrSize i = 0; i < classDecl->members->count; i++) {
        SZrAstNode *member = classDecl->members->nodes[i];
        if (member == ZR_NULL || member->type != ZR_AST_CLASS_FIELD) {
            continue;
        }

        SZrClassField *field = &member->data.classField;
        if (!field->isStatic || field->init == ZR_NULL || field->name == ZR_NULL || field->name->name == ZR_NULL) {
            continue;
        }

        if (prototypeSlot == (TZrUInt32)-1) {
            prototypeSlot = emit_load_global_identifier(cs, classDecl->name->name);
            if (prototypeSlot == (TZrUInt32)-1 || cs->hasError) {
                return;
            }
        }

        emit_object_field_assignment_from_expression(cs, prototypeSlot, field->name->name, field->init);
        if (cs->hasError) {
            return;
        }
    }
}

SZrString *compiler_create_hidden_property_accessor_name(SZrCompilerState *cs, SZrString *propertyName,
                                                         TZrBool isSetter) {
    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    TZrSize prefixLength = strlen(prefix);
    TZrSize propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                         ? propertyName->shortStringLength
                                         : propertyName->longStringLength;
    TZrSize bufferSize = prefixLength + propertyNameLength + 1;
    TZrChar *buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    SZrString *result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

void emit_super_constructor_call(SZrCompilerState *cs, SZrString *superTypeName, SZrAstNodeArray *superArgs) {
    if (cs == ZR_NULL || superTypeName == ZR_NULL || cs->hasError) {
        return;
    }

    TZrUInt32 prototypeSlot = emit_load_global_identifier(cs, superTypeName);
    if (prototypeSlot == (TZrUInt32)-1 || cs->hasError) {
        return;
    }

    SZrString *constructorName = ZrCore_String_CreateFromNative(cs->state, "__constructor");
    if (constructorName == ZR_NULL) {
        SZrFileRange location = cs->currentFunctionNode != ZR_NULL ? cs->currentFunctionNode->location
                                                                   : cs->errorLocation;
        ZrParser_Compiler_Error(cs, "Failed to create super constructor lookup key", location);
        return;
    }

    TZrUInt32 constructorKeySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, constructorKeySlot, constructorName);

    TZrUInt32 functionSlot = allocate_stack_slot(cs);
    TZrInstruction getConstructorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)functionSlot,
                                                             (TZrUInt16)prototypeSlot,
                                                             (TZrUInt16)constructorKeySlot);
    emit_instruction(cs, getConstructorInst);

    TZrUInt32 receiverSlot = allocate_stack_slot(cs);
    TZrInstruction copyReceiverInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)receiverSlot, 0);
    emit_instruction(cs, copyReceiverInst);

    TZrUInt32 argCount = 1;
    if (superArgs != ZR_NULL) {
        for (TZrSize i = 0; i < superArgs->count; i++) {
            SZrAstNode *argNode = superArgs->nodes[i];
            if (argNode == ZR_NULL) {
                continue;
            }

            TZrUInt32 targetSlot = allocate_stack_slot(cs);
            ZrParser_Expression_Compile(cs, argNode);
            if (cs->hasError || cs->stackSlotCount == 0) {
                return;
            }

            TZrUInt32 valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
            if (valueSlot != targetSlot) {
                TZrInstruction moveArgInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                  (TZrUInt16)targetSlot, (TZrInt32)valueSlot);
                emit_instruction(cs, moveArgInst);
            }
            argCount++;
        }
    }

    TZrInstruction callSuperInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), (TZrUInt16)functionSlot,
                                                        (TZrUInt16)functionSlot, (TZrUInt16)argCount);
    emit_instruction(cs, callSuperInst);
}

