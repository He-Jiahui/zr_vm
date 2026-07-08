#include "test_lsp_bzms_private_not_mask_or_counterpart_deep_query_cases.h"

#include "lsp_bitwise_zero_minus_shift_supported_count_range_query_test_support.h"

TZrBool ZrVmTest_LspRunBzmsPrivateNotMaskOrCounterpartDeepQueries(SZrState *state) {
    TZrBool passed;

    passed = ZR_TRUE;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (((-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - (((-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (((-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - (((-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((zero - (zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_zm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (zero - (zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_rhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (zero - unit))",
                     2,
                     3) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sl_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero << (~((-(zero - ((~(span - span)) | (zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))))))) & negativeUnit))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;
    passed = ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
                     state,
                     "or_zl_sr_pnm_uzm_all1_or_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_um_zm_zm_bnot_zm_urhs",
                     "(zero + zero) | ((zero >> (~(negativeUnit & (-(zero - ((zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (-(zero - (zero - (~negativeFour)))))))))))))))) | (~(unit - unit))))))))) - (-(zero - unit)))",
                     -3,
                     -2) &&
             passed;

    return passed;
}
