#include "zr_vm_jit/backend.h"

#include <cstring>

namespace {

/* 多个验证入口共用同一可选诊断格式，成功调用先清除上一次错误。 */
void clear_diagnostic(SZrJitHostDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_JIT_HOST_STATUS_OK;
        diagnostic->coreStatus = ZR_HOST_JIT_STATUS_OK;
    }
}

/* 保留失败字段的完整上下文，让编译请求能够原样透传状态图拒绝原因。 */
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

/* 仅供单类注册证据检查；公开入口先拒绝未知位及缺失的必需位。 */
bool has_flag(TZrUInt32 flags, TZrUInt32 flag) {
    return (flags & flag) != 0u;
}

/* bit、条目数和 hash 需共同构成注册声明；该入口不查验实际平台图内容。 */
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

/* PrepareWithMaps 和 Compile 都依赖此入口拒绝不完整的图声明；它不读取实际图内容。 */
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
