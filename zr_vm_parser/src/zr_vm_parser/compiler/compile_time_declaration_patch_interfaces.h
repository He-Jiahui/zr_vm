#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_INTERFACES_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_INTERFACES_H

#include "compiler_internal.h"

typedef struct SZrParserCompileTimePatchInterfaceAdds {
    TZrTypeId *typeIds;
    SZrString **typeNames;
    TZrSize count;
} SZrParserCompileTimePatchInterfaceAdds;

ZR_PARSER_API TZrBool ZrParser_CompileTime_PreparePatchInterfaceAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *interfaceAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchInterfaceAdds *result);
ZR_PARSER_API void ZrParser_CompileTime_FreePatchInterfaceAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchInterfaceAdds *interfaceAdds);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_INTERFACES_H
