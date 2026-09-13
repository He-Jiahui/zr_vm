#ifndef ZR_VM_PARSER_EXEC_IR_ALIAS_H
#define ZR_VM_PARSER_EXEC_IR_ALIAS_H

#include "zr_vm_parser/conf.h"

/*
 * Alias facts are parser-side evidence.  They deliberately do not use host
 * pointers: a fact can be retained in an analysis cache or serialized into a
 * diagnostic without exposing runtime addresses.
 */
typedef enum EZrExecIrAliasRelation {
    ZR_EXEC_IR_ALIAS_UNKNOWN = 0,
    ZR_EXEC_IR_ALIAS_DISJOINT,
    ZR_EXEC_IR_ALIAS_MAY_ALIAS,
    ZR_EXEC_IR_ALIAS_MUST_ALIAS
} EZrExecIrAliasRelation;

typedef enum EZrExecIrAliasBaseKind {
    ZR_EXEC_IR_ALIAS_BASE_UNKNOWN = 0,
    ZR_EXEC_IR_ALIAS_BASE_ALLOCATION,
    ZR_EXEC_IR_ALIAS_BASE_STACK,
    ZR_EXEC_IR_ALIAS_BASE_PARAMETER,
    ZR_EXEC_IR_ALIAS_BASE_GLOBAL,
    ZR_EXEC_IR_ALIAS_BASE_EXTERNAL
} EZrExecIrAliasBaseKind;

/* A stable base/projection identity supplied by lowering or place analysis. */
typedef struct SZrExecIrAliasLocation {
    EZrExecIrAliasBaseKind baseKind;
    TZrUInt64 baseId;
    TZrUInt64 projectionId;
    TZrUInt32 layoutId;
    TZrUInt64 generation;
    TZrBool hasStableBase;
    TZrBool escaped;
    TZrBool unknownWrite;
    /* Set only when layout metadata proves two distinct projections cannot
     * overlap.  Different projection IDs alone are not such a proof. */
    TZrBool projectionDisjoint;
} SZrExecIrAliasLocation;

ZR_PARSER_API EZrExecIrAliasRelation ZrParser_ExecIr_AliasQuery(
        const SZrExecIrAliasLocation *left,
        const SZrExecIrAliasLocation *right);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_AliasRelationName(
        EZrExecIrAliasRelation relation);

#endif /* ZR_VM_PARSER_EXEC_IR_ALIAS_H */
