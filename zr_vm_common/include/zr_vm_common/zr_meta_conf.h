//
// Created by HeJiahui on 2025/6/25.
//

#ifndef ZR_META_CONF_H
#define ZR_META_CONF_H
#include "zr_vm_common/zr_common_conf.h"
static TNativeString const CZrMetaName[] = {
    "constructor", "destructor",
    "add", "sub", "mul", "div", "mod", "neg",
    "compare",
    "to_bool", "to_string", "to_int", "to_float",
    "call",
    "getter", "setter",
    "shift_left", "shift_right",
    "bit_and", "bit_or", "bit_xor", "bit_not",
    "close"
};

enum EZrMetaType {
    ZR_META_CONSTRUCTOR,
    ZR_META_DESTRUCTOR,
    ZR_META_ADD,
    ZR_META_SUB,
    ZR_META_MUL,
    ZR_META_DIV,
    ZR_META_MOD,
    ZR_META_NEG,
    ZR_META_COMPARE,
    ZR_META_TO_BOOL,
    ZR_META_TO_STRING,
    ZR_META_TO_INT,
    ZR_META_TO_FLOAT,
    ZR_META_CALL,
    ZR_META_GETTER,
    ZR_META_SETTER,

    ZR_META_SHIFT_LEFT,
    ZR_META_SHIFT_RIGHT,

    ZR_META_BIT_AND,
    ZR_META_BIT_OR,
    ZR_META_BIT_XOR,
    ZR_META_BIT_NOT,

    ZR_META_CLOSE,

    ZR_META_ENUM_MAX
};

typedef enum EZrMetaType EZrMetaType;

#endif //ZR_META_CONF_H
