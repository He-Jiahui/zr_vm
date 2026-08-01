#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_ATTRIBUTES_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_ATTRIBUTES_H

#include "compiler_internal.h"

typedef struct SZrParserCompileTimePatchAttributeAdd {
    SZrParserAttributeData data;
    const SZrCompilerAttributeSchemaBinding *schema;
    SZrTypeValue *values;
} SZrParserCompileTimePatchAttributeAdd;

typedef struct SZrParserCompileTimePatchAttributeAdds {
    SZrParserCompileTimePatchAttributeAdd *entries;
    SZrParserAttributeData *contractData;
    TZrSize count;
} SZrParserCompileTimePatchAttributeAdds;

TZrBool ZrParser_CompileTime_PreparePatchAttributeAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *attributeAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchAttributeAdds *result);
TZrBool ZrParser_CompileTime_ApplyPatchAttributeAdds(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds);
void ZrParser_CompileTime_FreePatchAttributeAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchAttributeAdds *attributeAdds);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_ATTRIBUTES_H
