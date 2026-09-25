#ifndef ZR_TEST_SSA_SOURCE_CFG_FAULTS_H
#define ZR_TEST_SSA_SOURCE_CFG_FAULTS_H

#include <stddef.h>
#include "zr_vm_parser/compiler.h"

ZR_PARSER_API void ssa_source_cfg_fail_allocation(size_t ordinal);
ZR_PARSER_API TZrBool ssa_source_cfg_allocation_failed(void);
ZR_PARSER_API size_t ssa_source_cfg_outstanding_allocations(void);
typedef enum ESsaSourceCfgPromotionFault {
    SSA_SOURCE_CFG_PROMOTION_NO_FAULT = 0,
    SSA_SOURCE_CFG_PROMOTION_AFTER_ACTIVATION,
    SSA_SOURCE_CFG_PROMOTION_AFTER_FINISH
} ESsaSourceCfgPromotionFault;
ZR_PARSER_API void ssa_source_cfg_fail_promotion(ESsaSourceCfgPromotionFault fault);
ZR_PARSER_API TZrBool ssa_source_cfg_finalize(SZrCompilerState *compiler);

#endif
