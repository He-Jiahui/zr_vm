#ifndef ZR_VM_PARSER_COMPILER_DECORATOR_CONTRACT_H
#define ZR_VM_PARSER_COMPILER_DECORATOR_CONTRACT_H

#include "zr_vm_parser/compiler.h"

const TZrChar *ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
        SZrAstNode *decoratorNode,
        TZrBool *outHasCall);

TZrBool ZrParser_DecoratorContract_IsBuiltinFfiWrapper(
        SZrAstNode *decoratorNode);

TZrBool ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        TZrBool allowBuiltinFfiWrapper);

#endif // ZR_VM_PARSER_COMPILER_DECORATOR_CONTRACT_H
