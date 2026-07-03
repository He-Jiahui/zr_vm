#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUTS_H

#include <stdio.h>

#include "backend_aot_function_table.h"
#include "zr_vm_core/type_layout.h"

void backend_aot_write_c_type_layout_declarations(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotFunctionTable *table,
                                                  const TZrUInt32 *typeLayoutRoots,
                                                  TZrUInt32 typeLayoutRootCount);
TZrBool backend_aot_c_type_layout_collect_dynamic_dependency_roots(SZrState *state,
                                                                   const SZrAotFunctionTable *table,
                                                                   const TZrByte *metadataBlob,
                                                                   TZrSize metadataBlobLength,
                                                                   TZrUInt32 *typeLayoutRoots,
                                                                   TZrUInt32 rootCapacity,
                                                                   TZrUInt32 *outRootCount);
TZrUInt32 backend_aot_c_type_layout_count_referenced(SZrState *state,
                                                     const SZrAotFunctionTable *table,
                                                     const TZrUInt32 *typeLayoutRoots,
                                                     TZrUInt32 typeLayoutRootCount);
unsigned long long backend_aot_c_type_layout_payload_bytes_referenced(SZrState *state,
                                                                      const SZrAotFunctionTable *table,
                                                                      const TZrUInt32 *typeLayoutRoots,
                                                                      TZrUInt32 typeLayoutRootCount);
unsigned long long backend_aot_c_type_layout_generated_bytes_referenced(SZrState *state,
                                                                        const SZrAotFunctionTable *table,
                                                                        const TZrUInt32 *typeLayoutRoots,
                                                                        TZrUInt32 typeLayoutRootCount);
const SZrTypeLayout *backend_aot_c_type_layout_resolve_from_table(SZrState *state,
                                                                  const SZrAotFunctionTable *table,
                                                                  TZrUInt32 typeLayoutId);
TZrUInt32 backend_aot_c_type_layout_index_space(SZrState *state,
                                                const SZrAotFunctionTable *table,
                                                const TZrUInt32 *typeLayoutRoots,
                                                TZrUInt32 typeLayoutRootCount);
TZrUInt32 backend_aot_c_type_layout_gc_descriptor_index_space(SZrState *state,
                                                              const SZrAotFunctionTable *table,
                                                              const TZrUInt32 *typeLayoutRoots,
                                                              TZrUInt32 typeLayoutRootCount);
void backend_aot_write_c_type_layout_gc_descriptor_table(FILE *file,
                                                         SZrState *state,
                                                         const SZrAotFunctionTable *table,
                                                         const TZrUInt32 *typeLayoutRoots,
                                                         TZrUInt32 typeLayoutRootCount,
                                                         TZrUInt32 descriptorIndexSpace);
void backend_aot_write_c_type_layout_registration_table(FILE *file,
                                                        SZrState *state,
                                                        const SZrAotFunctionTable *table,
                                                        const TZrUInt32 *typeLayoutRoots,
                                                        TZrUInt32 typeLayoutRootCount,
                                                        TZrUInt32 typeLayoutIndexSpace);
void backend_aot_write_c_type_layout_token_table(FILE *file,
                                                 SZrState *state,
                                                 const SZrAotFunctionTable *table,
                                                 const TZrUInt32 *typeLayoutRoots,
                                                 TZrUInt32 typeLayoutRootCount,
                                                 const TZrByte *metadataBlob,
                                                 TZrSize metadataBlobLength,
                                                 TZrUInt32 typeLayoutIndexSpace);

#endif
