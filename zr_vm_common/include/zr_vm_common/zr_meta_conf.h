//
// Created by HeJiahui on 2025/6/25.
//

#ifndef ZR_META_CONF_H
#define ZR_META_CONF_H
#include "zr_vm_common/zr_common_conf.h"

#define ZR_META_DECLARE(Z)                                                                                             \
    Z(CONSTRUCTOR)                                                                                                     \
    Z(DESTRUCTOR)                                                                                                      \
    Z(ADD)                                                                                                             \
    Z(SUB)                                                                                                             \
    Z(MUL)                                                                                                             \
    Z(DIV)                                                                                                             \
    Z(MOD)                                                                                                             \
    Z(POW)                                                                                                             \
    Z(NEG)                                                                                                             \
    Z(COMPARE)                                                                                                         \
    Z(TO_BOOL)                                                                                                         \
    Z(TO_STRING)                                                                                                       \
    Z(TO_INT)                                                                                                          \
    Z(TO_UINT)                                                                                                         \
    Z(TO_FLOAT)                                                                                                        \
    Z(CALL)                                                                                                            \
    Z(GETTER)                                                                                                          \
    Z(SETTER)                                                                                                          \
    Z(SHIFT_LEFT)                                                                                                      \
    Z(SHIFT_RIGHT)                                                                                                     \
    Z(BIT_AND)                                                                                                         \
    Z(BIT_OR)                                                                                                          \
    Z(BIT_XOR)                                                                                                         \
    Z(BIT_NOT)                                                                                                         \
    Z(CLOSE)


#define ZR_META_ENUM(META) ZR_META_##META

#define ZR_META_ENUM_WRAP(...) ZR_MACRO_REGISTER_WRAP(enum EZrMetaType{, ZR_META_ENUM(ENUM_MAX)}, __VA_ARGS__)

#define ZR_META_ENUM_DECLARE(META) ZR_META_ENUM(META),

#define ZR_META_CONSTANT(META) #META

#define ZR_META_CONSTANT_WRAP(...)                                                                                     \
    ZR_MACRO_REGISTER_WRAP(                                                                                            \
            {                                                                                                          \
                    ,                                                                                                  \
            },                                                                                                         \
            __VA_ARGS__)

#define ZR_META_CONSTANT_DECLARE(META) ZR_META_CONSTANT(META),


ZR_META_ENUM_WRAP(ZR_META_DECLARE(ZR_META_ENUM_DECLARE));

typedef enum EZrMetaType EZrMetaType;

static TNativeString const CZrMetaName[] = ZR_META_CONSTANT_WRAP(ZR_META_DECLARE(ZR_META_CONSTANT_DECLARE));


#endif // ZR_META_CONF_H
