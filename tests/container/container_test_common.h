#ifndef ZR_VM_TESTS_CONTAINER_TEST_COMMON_H
#define ZR_VM_TESTS_CONTAINER_TEST_COMMON_H

#include "zr_vm_core/module.h"
#include "zr_vm_parser/compiler.h"

SZrState *ZrContainerTests_CreateState(void);

void ZrContainerTests_DestroyState(SZrState *state);

SZrCompilerState *ZrContainerTests_CreateCompilerState(SZrState *state);

void ZrContainerTests_DestroyCompilerState(SZrCompilerState *cs);

void ZrContainerTests_CompileTopLevelStatement(SZrCompilerState *cs, SZrAstNode *node);

const SZrTypePrototypeInfo *ZrContainerTests_FindTypePrototype(const SZrCompilerState *cs, const char *typeName);

TZrSize ZrContainerTests_CountTypePrototypes(const SZrCompilerState *cs, const char *typeName);

const SZrTypeMemberInfo *ZrContainerTests_FindTypeMemberByName(const SZrTypePrototypeInfo *prototype,
                                                               const char *memberName);

const SZrTypeMemberInfo *ZrContainerTests_FindMetaMember(const SZrTypePrototypeInfo *prototype,
                                                         EZrMetaType metaType);

SZrObjectModule *ZrContainerTests_ImportNativeModule(SZrState *state, const TZrChar *moduleName);

const SZrTypeValue *ZrContainerTests_GetModuleExportValue(SZrState *state,
                                                          SZrObjectModule *module,
                                                          const TZrChar *exportName);

const SZrTypeValue *ZrContainerTests_GetObjectFieldValue(SZrState *state,
                                                         SZrObject *object,
                                                         const TZrChar *fieldName);

TZrSize ZrContainerTests_GetArrayLength(SZrObject *array);

SZrObject *ZrContainerTests_GetArrayEntryObject(SZrState *state, SZrObject *array, TZrSize index);

const SZrTypeValue *ZrContainerTests_GetArrayEntryValue(SZrState *state, SZrObject *array, TZrSize index);

SZrObject *ZrContainerTests_FindNamedEntryInArray(SZrState *state,
                                                  SZrObject *array,
                                                  const TZrChar *fieldName,
                                                  const TZrChar *expectedValue);

TZrBool ZrContainerTests_StringEqualsCString(SZrString *value, const TZrChar *expected);

#endif
