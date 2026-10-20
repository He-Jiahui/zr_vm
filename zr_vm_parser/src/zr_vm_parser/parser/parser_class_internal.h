#ifndef ZR_VM_PARSER_CLASS_INTERNAL_H
#define ZR_VM_PARSER_CLASS_INTERNAL_H

#include "parser_internal.h"

/* 分类试探与正式成员解析共用修饰符集合；字段分支另行拒绝其中不适用的标志。 */
static TZrUInt32 class_member_allowed_modifier_flags(void) {
    return ZR_DECLARATION_MODIFIER_ABSTRACT |
           ZR_DECLARATION_MODIFIER_VIRTUAL |
           ZR_DECLARATION_MODIFIER_OVERRIDE |
           ZR_DECLARATION_MODIFIER_FINAL |
           ZR_DECLARATION_MODIFIER_SHADOW;
}

#endif
