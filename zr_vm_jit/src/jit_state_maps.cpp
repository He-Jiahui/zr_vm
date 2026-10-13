/**
 * @file
 * @brief 校验 JIT facade 的四类状态图声明。
 *
 * ## Summary
 * 该翻译单元只检查调用方提交的 schema、登记位、条目数和摘要之间是否自洽，
 * 为 PrepareWithMaps 与 Compile 前置校验共享同一判断路径。
 *
 * ## Constraints and lifecycle
 * 不持有图内存、不调用平台注册 API、不分配资源；facts 与可选 diagnostic
 * 只在同步调用期间借用。纯验证入口可并发调用。
 *
 * ## References
 * @see ZrJit_Host_ValidateStateMaps
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_ValidateCompileRequest
 */
#include "zr_vm_jit/backend.h"

#include <cstring>

namespace {

/**
 * ## Summary
 * 为一次状态图校验清除可选诊断，避免成功后沿用前次错误。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空时清零全部字段并将 facade/core 状态设为 OK。
 *
 * ## Returns
 * 无返回值；空指针是 no-op。
 *
 * ## Constraints and lifecycle
 * 仅写调用方诊断存储，不拥有该对象，也不访问 singleton。
 *
 * ## References
 * @see fail
 * @see ZrJit_Host_ValidateStateMaps
 */
void clear_diagnostic(SZrJitHostDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_JIT_HOST_STATUS_OK;
        diagnostic->coreStatus = ZR_HOST_JIT_STATUS_OK;
    }
}

/**
 * ## Summary
 * 写入单次状态图校验的 facade 失败码和可定位差异字段。
 *
 * ## Parameters
 * @param[out] diagnostic 可空的诊断输出；空时只返回 status。
 * @param[in] status 本次校验的具体拒绝原因。
 * @param[in] expected @p actual 对应的 32 位期望值。
 * @param[in] actual 被拒绝的实际 32 位值。
 * @param[in] expectedHash 可选的 64 位期望摘要。
 * @param[in] actualHash 实际 64 位摘要。
 * @param[in] sourceIndex 所属图类别索引，供调用方定位失败来源。
 *
 * ## Returns
 * @return 原样返回 @p status；诊断为空不会改变返回状态。
 *
 * ## Constraints and lifecycle
 * 不分配资源；写入范围仅限 diagnostic，调用者负责其有效期。
 *
 * ## References
 * @see validate_entry
 * @see ZrJit_Host_ValidateStateMaps
 */
EZrJitHostStatus fail(SZrJitHostDiagnostic *diagnostic,
                      EZrJitHostStatus status,
                      TZrUInt32 expected,
                      TZrUInt32 actual,
                      TZrUInt64 expectedHash,
                      TZrUInt64 actualHash,
                      TZrUInt32 sourceIndex) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
        diagnostic->expectedHash = expectedHash;
        diagnostic->actualHash = actualHash;
        diagnostic->sourceIndex = sourceIndex;
    }
    return status;
}

/**
 * ## Summary
 * 判断登记掩码是否含有单类状态图的指定 bit。
 *
 * ## Parameters
 * @param[in] flags 已由公开入口验证为已知位集合的登记掩码。
 * @param[in] flag 当前检查的单个图类别位。
 *
 * ## Returns
 * @return bit 存在时为 true，否则为 false。
 *
 * ## Constraints and lifecycle
 * 纯位运算；本 helper 不检查未知位或四类全量完整性。
 *
 * ## References
 * @see validate_entry
 */
bool has_flag(TZrUInt32 flags, TZrUInt32 flag) {
    return (flags & flag) != 0u;
}

/**
 * ## Summary
 * 检查单类图的登记 bit、条目数和 hash 是否成套出现。
 *
 * ## Parameters
 * @param[in] flag 当前图类别的登记 bit。
 * @param[in] count 该类别声明的条目数。
 * @param[in] hash 该类别声明的摘要。
 * @param[in] sourceIndex 对应的诊断类别编号。
 * @param[in] facts 已通过公开入口的 schema/flags 前置检查的状态图事实。
 * @param[out] diagnostic 可空；失败时写入类别、条目数和摘要。
 *
 * ## Returns
 * @return bit 已设置且 count/hash 非零时返回 OK；缺少 count/hash 返回 STATE_MAP_INVALID；
 * bit 未设置但仍提交 count/hash 时返回 REGISTRATION_INCOMPLETE。
 *
 * ## Constraints and lifecycle
 * facts 不为空且 registrationFlags 已过全量检查是调用前提。本函数不读取图数据；
 * 未设置 bit 的防线为单类校验复用保留，当前公开入口先要求四个位全部设置。
 *
 * ## References
 * @see has_flag
 * @see fail
 * @see ZrJit_Host_ValidateStateMaps
 */
EZrJitHostStatus validate_entry(TZrUInt32 flag,
                                TZrUInt32 count,
                                TZrUInt64 hash,
                                TZrUInt32 sourceIndex,
                                const SZrJitStateMapFacts *facts,
                                SZrJitHostDiagnostic *diagnostic) {
    if (has_flag(facts->registrationFlags, flag)) {
        if (count == 0u || hash == 0u) {
            return fail(diagnostic, ZR_JIT_HOST_STATUS_STATE_MAP_INVALID,
                        1u, count, 1u, hash, sourceIndex);
        }
    } else if (count != 0u || hash != 0u) {
        /* 当前公开校验先要求四个 bit 全部设置；保留该防线供单项校验复用。 */
        return fail(diagnostic, ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
                    flag, facts->registrationFlags, 0u, hash, sourceIndex);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

}  // namespace

/**
 * @brief 验证四类状态图登记声明的完整性。
 *
 * ## Parameters
 * @param[in] facts 必须非空；schema 版本、登记位、布局摘要及四类 count/hash
 *                  必须分别满足当前完整声明规则。
 * @param[out] diagnostic 可空；失败时保留具体状态、图类别及预期/实际字段。
 *
 * ## Returns
 * @return 全部声明自洽时返回 OK；参数、schema、flags、布局或某类登记缺失时返回相应状态。
 *
 * ## Constraints and lifecycle
 * 只验证调用方声明，不读取真实图、不确认平台注册、不复制 facts 或分配资源。
 * 诊断只在本次同步调用中写入；空 diagnostic 不会使具体返回码退化。
 *
 * ## References
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_ValidateCompileRequest
 * @see tests/core/test_ssa_host_jit_optional.c
 */
extern "C" EZrJitHostStatus ZrJit_Host_ValidateStateMaps(
        const SZrJitStateMapFacts *facts,
        SZrJitHostDiagnostic *diagnostic) {
    EZrJitHostStatus status;
    clear_diagnostic(diagnostic);
    if (facts == ZR_NULL) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
                    1u, 0u, 0u, 0u, 0u);
    }
    if (facts->schemaVersion != ZR_JIT_HOST_SCHEMA_VERSION) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH,
                    ZR_JIT_HOST_SCHEMA_VERSION, facts->schemaVersion,
                    0u, 0u, 0u);
    }
    if ((facts->registrationFlags & ~ZR_JIT_HOST_MAP_KNOWN_MASK) != 0u) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_INVALID_FLAGS,
                    ZR_JIT_HOST_MAP_KNOWN_MASK, facts->registrationFlags,
                    0u, 0u, 0u);
    }
    if (facts->frameLayoutHash == 0u) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_STATE_MAP_INVALID,
                    1u, 0u, 1u, 0u, 0u);
    }
    if ((facts->registrationFlags & ZR_JIT_HOST_MAP_KNOWN_MASK) !=
        ZR_JIT_HOST_MAP_KNOWN_MASK) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
                    ZR_JIT_HOST_MAP_KNOWN_MASK, facts->registrationFlags,
                    0u, 0u, 0u);
    }

    status = validate_entry(ZR_JIT_HOST_MAP_ROOTS, facts->rootEntryCount,
                            facts->rootMapHash, 0u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    status = validate_entry(ZR_JIT_HOST_MAP_UNWIND, facts->unwindEntryCount,
                            facts->unwindMapHash, 1u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    status = validate_entry(ZR_JIT_HOST_MAP_DEBUG, facts->debugEntryCount,
                            facts->debugMapHash, 2u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    status = validate_entry(ZR_JIT_HOST_MAP_DEOPT, facts->deoptEntryCount,
                            facts->deoptMapHash, 3u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    return ZR_JIT_HOST_STATUS_OK;
}
