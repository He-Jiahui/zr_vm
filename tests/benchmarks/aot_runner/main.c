#include "aot_runner.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void zr_aot_runner_print_usage(const char *executable) {
    fprintf(stderr,
            "Usage: %s --backend <c|llvm> --entry-token <positive-integer> "
            "[--checksum <integer>] [--allow-interpreter-fallback]\n",
            executable != ZR_NULL ? executable : "aot_runner");
}

static TZrBool zr_aot_runner_parse_u64(const char *text, TZrUInt64 *value) {
    char *end = ZR_NULL;
    unsigned long long parsed;

    if (text == ZR_NULL || *text == '\0' || value == ZR_NULL) {
        return ZR_FALSE;
    }
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno == ERANGE || end == text || end == ZR_NULL || *end != '\0' ||
        (TZrUInt64)parsed != parsed) {
        return ZR_FALSE;
    }
    *value = (TZrUInt64)parsed;
    return ZR_TRUE;
}

static TZrBool zr_aot_runner_parse_backend(const char *text, EZrAotBackend *backend) {
    if (text == ZR_NULL || backend == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(text, "c") == 0 || strcmp(text, "aot_c") == 0) {
        *backend = ZR_AOT_BACKEND_C;
        return ZR_TRUE;
    }
    if (strcmp(text, "llvm") == 0 || strcmp(text, "aot_llvm") == 0) {
        *backend = ZR_AOT_BACKEND_LLVM;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static void zr_aot_runner_print_result(const SZrAotRunnerResult *result) {
    if (result == ZR_NULL) {
        return;
    }
    printf("AOT_RUNNER status=%s requested=%s actual=%s entry_token=%llu"
           " checksum=%llu coverage=%s native_sites=%llu"
           " native_helper_sites=%llu interpreter_sites=%llu"
           " semantic_sites=%llu deopt=%llu reason=%s\n",
           ZrTests_AotRunner_StatusName(result->status),
           ZrTests_AotRunner_BackendName(result->requestedBackend),
           ZrTests_AotRunner_BackendName(result->actualBackend),
           (unsigned long long)result->entryToken,
           (unsigned long long)result->checksum,
           ZrTests_AotCoverage_StatusName(result->coverage.status),
           (unsigned long long)result->coverage.nativeSites,
           (unsigned long long)result->coverage.nativeHelperSites,
           (unsigned long long)result->coverage.interpreterSites,
           (unsigned long long)result->coverage.semanticSites,
           (unsigned long long)result->coverage.deoptCount,
           ZrTests_AotRunner_FailureName(result->failure));
}

int ZrTests_AotRunner_Main(int argc,
                           char **argv,
                           FZrAotRunnerRegisterCompiledEntries registerEntries) {
    SZrAotRunner runner;
    SZrAotRunnerRequest request;
    SZrAotRunnerResult result;
    EZrAotBackend backend = ZR_AOT_BACKEND_UNKNOWN;
    TZrUInt64 entryToken = 0u;
    TZrUInt64 expectedChecksum = 0u;
    TZrBool checksumRequired = ZR_FALSE;
    TZrBool allowInterpreterFallback = ZR_FALSE;
    int index;

    memset(&request, 0, sizeof(request));
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--backend") == 0 && index + 1 < argc) {
            if (!zr_aot_runner_parse_backend(argv[++index], &backend)) {
                zr_aot_runner_print_usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[index], "--entry-token") == 0 && index + 1 < argc) {
            if (!zr_aot_runner_parse_u64(argv[++index], &entryToken) || entryToken == 0u) {
                zr_aot_runner_print_usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[index], "--checksum") == 0 && index + 1 < argc) {
            if (!zr_aot_runner_parse_u64(argv[++index], &expectedChecksum)) {
                zr_aot_runner_print_usage(argv[0]);
                return 2;
            }
            checksumRequired = ZR_TRUE;
        } else if (strcmp(argv[index], "--allow-interpreter-fallback") == 0) {
            allowInterpreterFallback = ZR_TRUE;
        } else {
            zr_aot_runner_print_usage(argv[0]);
            return 2;
        }
    }
    if (backend == ZR_AOT_BACKEND_UNKNOWN || entryToken == 0u) {
        zr_aot_runner_print_usage(argv[0]);
        return 2;
    }

    ZrTests_AotRunner_Init(&runner);
    if (registerEntries != ZR_NULL && registerEntries(&runner) == ZR_FALSE) {
        /* Keep any partial registry out of the result: a failed artifact load
         * must not be relabeled as a runnable AOT entry. */
        ZrTests_AotRunner_Init(&runner);
    }
    request.requestedBackend = backend;
    request.entryToken = entryToken;
    request.expectedChecksum = expectedChecksum;
    request.checksumRequired = checksumRequired;
    request.allowInterpreterFallback = allowInterpreterFallback;
    (void)ZrTests_AotRunner_Run(&runner, &request, &result);
    zr_aot_runner_print_result(&result);

    if (result.status == ZR_AOT_RUNNER_STATUS_RAN) {
        return 0;
    }
    if (result.status == ZR_AOT_RUNNER_STATUS_FALLBACK) {
        /* The process completed, but callers must reject this row for a pure
         * AOT gate by inspecting the explicit status/actual backend fields. */
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    return ZrTests_AotRunner_Main(argc, argv, ZR_NULL);
}
