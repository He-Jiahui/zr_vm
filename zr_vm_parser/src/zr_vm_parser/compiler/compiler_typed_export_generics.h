#ifndef ZR_VM_PARSER_COMPILER_TYPED_EXPORT_GENERICS_H
#define ZR_VM_PARSER_COMPILER_TYPED_EXPORT_GENERICS_H

#include "compiler_internal.h"

TZrBool compiler_typed_export_generic_contract_copy_from_declaration(
        SZrCompilerState *cs,
        const SZrGenericDeclaration *genericDeclaration,
        SZrFunctionTypedExportSymbol *outSymbol);

TZrBool compiler_typed_export_generic_contract_copy_from_infos(
        SZrCompilerState *cs,
        const SZrArray *genericParameters,
        SZrFunctionTypedExportSymbol *outSymbol);

void compiler_typed_export_generic_contract_free(
        SZrState *state,
        SZrFunctionTypedExportSymbol *symbol);

#endif
