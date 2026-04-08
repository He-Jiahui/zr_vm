#ifndef ZR_VM_CORE_OBJECT_SUPER_ARRAY_INTERNAL_H
#define ZR_VM_CORE_OBJECT_SUPER_ARRAY_INTERNAL_H

#include "object_internal.h"

#define ZR_OBJECT_HIDDEN_ITEMS_FIELD "__zr_items"
#define ZR_OBJECT_ARRAY_LENGTH_FIELD "length"
#define ZR_OBJECT_ARRAY_CAPACITY_FIELD "capacity"
#define ZR_OBJECT_ARRAY_ADD_MEMBER "add"
#define ZR_OBJECT_LITERAL_CACHE_CAPACITY 8U
#define ZR_OBJECT_SUPER_ARRAY_INITIAL_CAPACITY 4U
#define ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR 2U

SZrString *ZrCore_Object_CachedKnownFieldString(SZrState *state, const TZrChar *literal);

TZrBool ZrCore_Object_SuperArrayTryGetIntFast(SZrState *state,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *key,
                                              SZrTypeValue *result,
                                              TZrBool *outApplicable);

TZrBool ZrCore_Object_SuperArrayTrySetIntFast(SZrState *state,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *key,
                                              const SZrTypeValue *value,
                                              TZrBool *outApplicable);

#endif
