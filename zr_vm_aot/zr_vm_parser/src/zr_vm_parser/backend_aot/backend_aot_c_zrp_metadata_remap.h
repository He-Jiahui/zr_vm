#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_REMAP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_REMAP_H

#include "backend_aot_function_table.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

TZrBool backend_aot_c_zrp_method_def_row_is_retained(const SZrZrpMetadataMethodDefRow *row,
                                                     const SZrAotFunctionTable *functionTable);
TZrUInt32 backend_aot_c_zrp_count_retained_method_defs(const SZrZrpMetadataMethodDefRow *rows,
                                                       TZrUInt32 count,
                                                       const SZrAotFunctionTable *functionTable);
TZrMetadataToken backend_aot_c_zrp_compacted_method_def_token(const SZrZrpMetadataMethodDefRow *methodRows,
                                                             TZrUInt32 methodCount,
                                                             TZrUInt32 methodIndex,
                                                             const SZrAotFunctionTable *functionTable);
TZrMetadataToken backend_aot_c_zrp_compacted_field_def_token(TZrUInt32 retainedMethodDefCount,
                                                            TZrUInt32 fieldIndex);
TZrBool backend_aot_c_zrp_remap_token_record(SZrMetadataTokenRecord *record,
                                             const SZrZrpMetadataMethodDefRow *methodRows,
                                             TZrUInt32 methodCount,
                                             const SZrZrpMetadataFieldDefRow *fieldRows,
                                             TZrUInt32 fieldCount,
                                             const SZrAotFunctionTable *functionTable,
                                             TZrUInt32 retainedMethodDefCount);
TZrBool backend_aot_c_zrp_remap_export_member_token(TZrMetadataToken *token,
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrZrpMetadataFieldDefRow *fieldRows,
                                                    TZrUInt32 fieldCount,
                                                    const SZrAotFunctionTable *functionTable,
                                                    TZrUInt32 retainedMethodDefCount);
TZrUInt32 backend_aot_c_zrp_count_retained_token_records(const SZrMetadataTokenRecord *records,
                                                         TZrUInt32 count,
                                                         const SZrZrpMetadataMethodDefRow *methodRows,
                                                         TZrUInt32 methodCount,
                                                         const SZrZrpMetadataFieldDefRow *fieldRows,
                                                         TZrUInt32 fieldCount,
                                                         const SZrAotFunctionTable *functionTable,
                                                         TZrUInt32 retainedMethodDefCount);
TZrBool backend_aot_c_zrp_remap_method_spec_row(SZrZrpMetadataMethodSpecRow *row,
                                                const SZrZrpMetadataMethodDefRow *methodRows,
                                                TZrUInt32 methodCount,
                                                const SZrZrpMetadataFieldDefRow *fieldRows,
                                                TZrUInt32 fieldCount,
                                                const SZrAotFunctionTable *functionTable,
                                                TZrUInt32 retainedMethodDefCount);
TZrUInt32 backend_aot_c_zrp_count_retained_method_specs(const SZrZrpMetadataMethodSpecRow *rows,
                                                        TZrUInt32 count,
                                                        const SZrZrpMetadataMethodDefRow *methodRows,
                                                        TZrUInt32 methodCount,
                                                        const SZrZrpMetadataFieldDefRow *fieldRows,
                                                        TZrUInt32 fieldCount,
                                                        const SZrAotFunctionTable *functionTable,
                                                        TZrUInt32 retainedMethodDefCount);
TZrBool backend_aot_c_zrp_remap_generic_param_owner_token(TZrMetadataToken *token,
                                                          const SZrZrpMetadataMethodDefRow *methodRows,
                                                          TZrUInt32 methodCount,
                                                          const SZrZrpMetadataFieldDefRow *fieldRows,
                                                          TZrUInt32 fieldCount,
                                                          const SZrAotFunctionTable *functionTable,
                                                          TZrUInt32 retainedMethodDefCount);
TZrBool backend_aot_c_zrp_generic_param_row_is_retained(const SZrZrpMetadataGenericParamRow *row,
                                                        const SZrZrpMetadataMethodDefRow *methodRows,
                                                        TZrUInt32 methodCount,
                                                        const SZrZrpMetadataFieldDefRow *fieldRows,
                                                        TZrUInt32 fieldCount,
                                                        const SZrAotFunctionTable *functionTable,
                                                        TZrUInt32 retainedMethodDefCount);
TZrUInt32 backend_aot_c_zrp_count_retained_generic_params(const SZrZrpMetadataGenericParamRow *rows,
                                                          TZrUInt32 count,
                                                          const SZrZrpMetadataMethodDefRow *methodRows,
                                                          TZrUInt32 methodCount,
                                                          const SZrZrpMetadataFieldDefRow *fieldRows,
                                                          TZrUInt32 fieldCount,
                                                          const SZrAotFunctionTable *functionTable,
                                                          TZrUInt32 retainedMethodDefCount);
TZrBool backend_aot_c_zrp_remap_generic_param_constraint_row(
        SZrZrpMetadataGenericParamConstraintRow *row,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);
TZrUInt32 backend_aot_c_zrp_count_retained_generic_param_constraints(
        const SZrZrpMetadataGenericParamConstraintRow *rows,
        TZrUInt32 count,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);
void backend_aot_c_zrp_adjust_generic_param_range(TZrUInt32 *firstGenericParamIndex,
                                                  TZrUInt32 *genericParamCount,
                                                  const SZrZrpMetadataGenericParamRow *genericParamRows,
                                                  TZrUInt32 genericParamRowCount,
                                                  const SZrZrpMetadataMethodDefRow *methodRows,
                                                  TZrUInt32 methodCount,
                                                  const SZrZrpMetadataFieldDefRow *fieldRows,
                                                  TZrUInt32 fieldCount,
                                                  const SZrAotFunctionTable *functionTable,
                                                  TZrUInt32 retainedMethodDefCount);
void backend_aot_c_zrp_adjust_generic_param_constraint_range(
        TZrUInt32 *firstConstraintIndex,
        TZrUInt32 *constraintCount,
        const SZrZrpMetadataGenericParamConstraintRow *constraintRows,
        TZrUInt32 constraintRowCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount);
void backend_aot_c_zrp_adjust_type_def_method_range(SZrZrpMetadataTypeDefRow *row,
                                                    const SZrZrpMetadataMethodDefRow *methodRows,
                                                    TZrUInt32 methodCount,
                                                    const SZrAotFunctionTable *functionTable);

#endif
