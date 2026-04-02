#include "container_test_common.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/compiler.h"

static TZrPtr zr_container_test_allocator(TZrPtr userData,
                                          TZrPtr pointer,
                                          TZrSize originalSize,
                                          TZrSize newSize,
                                          TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

SZrState *ZrContainerTests_CreateState(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(zr_container_test_allocator, ZR_NULL, 12345, &callbacks);
    SZrState *mainState;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    mainState = global->mainThreadState;
    if (mainState != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        ZrVmLibContainer_Register(global);
        ZrVmLibMath_Register(global);
        ZrVmLibSystem_Register(global);
        ZrVmLibFfi_Register(global);
    }

    return mainState;
}

void ZrContainerTests_DestroyState(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    ZrCore_GlobalState_Free(state->global);
}

SZrCompilerState *ZrContainerTests_CreateCompilerState(SZrState *state) {
    SZrCompilerState *cs;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(cs, state);
    return cs;
}

void ZrContainerTests_DestroyCompilerState(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

void ZrContainerTests_CompileTopLevelStatement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_INTERFACE_DECLARATION:
            ZrParser_Compiler_CompileInterfaceDeclaration(cs, node);
            break;
        case ZR_AST_CLASS_DECLARATION:
            ZrParser_Compiler_CompileClassDeclaration(cs, node);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            ZrParser_Compiler_CompileStructDeclaration(cs, node);
            break;
        default:
            ZrParser_Statement_Compile(cs, node);
            break;
    }
}

const SZrTypePrototypeInfo *ZrContainerTests_FindTypePrototype(const SZrCompilerState *cs, const char *typeName) {
    TZrSize index;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cs->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *info =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get((SZrArray *)&cs->typePrototypes, index);
        if (info != ZR_NULL &&
            info->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(info->name), typeName) == 0) {
            return info;
        }
    }

    return ZR_NULL;
}

TZrSize ZrContainerTests_CountTypePrototypes(const SZrCompilerState *cs, const char *typeName) {
    TZrSize count = 0;
    TZrSize index;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < cs->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *info =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get((SZrArray *)&cs->typePrototypes, index);
        if (info != ZR_NULL &&
            info->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(info->name), typeName) == 0) {
            count++;
        }
    }

    return count;
}

const SZrTypeMemberInfo *ZrContainerTests_FindTypeMemberByName(const SZrTypePrototypeInfo *prototype,
                                                               const char *memberName) {
    TZrSize index;

    if (prototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&prototype->members, index);
        if (member != ZR_NULL &&
            member->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(member->name), memberName) == 0) {
            return member;
        }
    }

    return ZR_NULL;
}

const SZrTypeMemberInfo *ZrContainerTests_FindMetaMember(const SZrTypePrototypeInfo *prototype,
                                                         EZrMetaType metaType) {
    TZrSize index;

    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&prototype->members, index);
        if (member != ZR_NULL && member->isMetaMethod && member->metaType == metaType) {
            return member;
        }
    }

    return ZR_NULL;
}

SZrObjectModule *ZrContainerTests_ImportNativeModule(SZrState *state, const TZrChar *moduleName) {
    SZrString *modulePath;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    modulePath = ZrCore_String_Create(state, (TZrNativeString)moduleName, strlen(moduleName));
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_ImportByPath(state, modulePath);
}

const SZrTypeValue *ZrContainerTests_GetModuleExportValue(SZrState *state,
                                                          SZrObjectModule *module,
                                                          const TZrChar *exportName) {
    SZrString *exportNameString;

    if (state == ZR_NULL || module == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    exportNameString = ZrCore_String_Create(state, (TZrNativeString)exportName, strlen(exportName));
    if (exportNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportNameString);
}

const SZrTypeValue *ZrContainerTests_GetObjectFieldValue(SZrState *state,
                                                         SZrObject *object,
                                                         const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

TZrSize ZrContainerTests_GetArrayLength(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }

    return array->nodeMap.elementCount;
}

SZrObject *ZrContainerTests_GetArrayEntryObject(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    entryValue = ZrCore_Object_GetValue(state, array, &key);
    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

const SZrTypeValue *ZrContainerTests_GetArrayEntryValue(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

SZrObject *ZrContainerTests_FindNamedEntryInArray(SZrState *state,
                                                  SZrObject *array,
                                                  const TZrChar *fieldName,
                                                  const TZrChar *expectedValue) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || fieldName == ZR_NULL || expectedValue == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < ZrContainerTests_GetArrayLength(array); index++) {
        SZrObject *entry = ZrContainerTests_GetArrayEntryObject(state, array, index);
        const SZrTypeValue *fieldValue = ZrContainerTests_GetObjectFieldValue(state, entry, fieldName);
        if (fieldValue != ZR_NULL &&
            fieldValue->type == ZR_VALUE_TYPE_STRING &&
            ZrContainerTests_StringEqualsCString(ZR_CAST_STRING(state, fieldValue->value.object), expectedValue)) {
            return entry;
        }
    }

    return ZR_NULL;
}

TZrBool ZrContainerTests_StringEqualsCString(SZrString *value, const TZrChar *expected) {
    const TZrChar *nativeString;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(value);
    return nativeString != ZR_NULL && strcmp(nativeString, expected) == 0;
}
