//
// Stdio server deterministic teardown contract tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdio_frame_reader.h"
#include "stdio_lifecycle.h"
#include "stdio_request_registry.h"
#include "stdio_server.h"

static int g_failures = 0;

static void expect_true(TZrBool condition, const char *message) {
    if (!condition) {
        printf("Fail - %s\n", message);
        g_failures++;
    }
}

static void test_lifecycle_state_transitions(void) {
    SZrStdioLifecycle lifecycle;

    ZrLanguageServer_StdioLifecycle_Init(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_NEW,
                "lifecycle must start in NEW");
    expect_true(!lifecycle.initializedNotificationReceived,
                "initialized notification must start unset");
    expect_true(!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "NEW lifecycle must reject ordinary requests");

    ZrLanguageServer_StdioLifecycle_MarkInitialized(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_NEW,
                "initialized before initialize must be ignored");
    expect_true(!lifecycle.initializedNotificationReceived,
                "early initialized notification must stay unset");

    expect_true(ZrLanguageServer_StdioLifecycle_BeginInitialize(&lifecycle),
                "NEW lifecycle must enter INITIALIZING");
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_INITIALIZING,
                "initialize must enter INITIALIZING");
    expect_true(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "INITIALIZING lifecycle must accept requests");

    ZrLanguageServer_StdioLifecycle_MarkInitialized(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_RUNNING,
                "initialized must enter RUNNING");
    expect_true(lifecycle.initializedNotificationReceived,
                "initialized notification must be recorded");
    ZrLanguageServer_StdioLifecycle_MarkInitialized(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_RUNNING,
                "duplicate initialized must be ignored");

    expect_true(ZrLanguageServer_StdioLifecycle_BeginShutdown(&lifecycle),
                "RUNNING lifecycle must enter SHUTDOWN");
    expect_true(ZrLanguageServer_StdioLifecycle_IsShutdown(&lifecycle),
                "shutdown state must be observable");
    expect_true(!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "SHUTDOWN lifecycle must reject ordinary requests");
    expect_true(!ZrLanguageServer_StdioLifecycle_BeginInitialize(&lifecycle),
                "SHUTDOWN lifecycle must reject reinitialize");
    expect_true(ZrLanguageServer_StdioLifecycle_Exit(&lifecycle) == 0,
                "exit after shutdown must return zero");
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_EXITED,
                "exit must enter EXITED");
    expect_true(!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "EXITED lifecycle must reject ordinary requests");
    expect_true(ZrLanguageServer_StdioLifecycle_Exit(&lifecycle) == 1,
                "repeated exit must return failure code");

    ZrLanguageServer_StdioLifecycle_Init(&lifecycle);
    expect_true(ZrLanguageServer_StdioLifecycle_Exit(&lifecycle) == 1,
                "exit before shutdown must return failure code");
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_EXITED,
                "failed exit must still enter EXITED");
}

static void test_request_registry_identity_and_cancellation(void) {
    SZrStdioRequestRegistry *registry =
            ZrLanguageServer_StdioRequestRegistry_New();
    cJSON *numericId = cJSON_CreateNumber(1.0);
    cJSON *stringId = cJSON_CreateString("1");
    cJSON *unknownId = cJSON_CreateString("unknown");
    cJSON *booleanId = cJSON_CreateBool(1);

    expect_true(registry != NULL, "request registry must construct");
    if (registry == NULL) {
        cJSON_Delete(numericId);
        cJSON_Delete(stringId);
        cJSON_Delete(unknownId);
        cJSON_Delete(booleanId);
        return;
    }

    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, numericId) ==
                        ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                "numeric request id must reserve");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, numericId) ==
                        ZR_STDIO_REQUEST_RESERVATION_DUPLICATE,
                "active numeric request id must be rejected as duplicate");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, stringId) ==
                        ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                "string request id must not collide with numeric id");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, stringId) ==
                        ZR_STDIO_REQUEST_RESERVATION_DUPLICATE,
                "active string request id must be rejected as duplicate");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, numericId),
                "new numeric request must not be cancelled");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, stringId),
                "new string request must not be cancelled");

    expect_true(ZrLanguageServer_StdioRequestRegistry_Cancel(registry, numericId),
                "known numeric request id must be cancellable");
    expect_true(ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, numericId),
                "cancellation must be retained for the matching numeric id");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, stringId),
                "cancellation must not cross numeric/string id types");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_Cancel(registry, unknownId),
                "unknown request cancellation must be a no-op");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, booleanId) ==
                        ZR_STDIO_REQUEST_RESERVATION_FAILED,
                "structured request id must not enter the registry");

    ZrLanguageServer_StdioRequestRegistry_Complete(registry, numericId);
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, numericId) ==
                        ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                "completed request id must be reusable");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, numericId),
                "reused request id must start with a fresh cancellation state");

    ZrLanguageServer_StdioRequestRegistry_Complete(registry, numericId);
    ZrLanguageServer_StdioRequestRegistry_Complete(registry, stringId);
    ZrLanguageServer_StdioRequestRegistry_Free(registry);
    cJSON_Delete(numericId);
    cJSON_Delete(stringId);
    cJSON_Delete(unknownId);
    cJSON_Delete(booleanId);
}

static EZrStdioFrameReadStatus read_frame_from_memory(const void *bytes,
                                                       size_t length,
                                                       const SZrStdioFrameReaderLimits *limits,
                                                       char **outPayload,
                                                       TZrSize *outLength) {
    FILE *input = tmpfile();
    EZrStdioFrameReadStatus status;

    if (input == NULL) {
        if (outPayload != NULL) {
            *outPayload = NULL;
        }
        if (outLength != NULL) {
            *outLength = 0;
        }
        return ZR_STDIO_FRAME_READ_IO_ERROR;
    }
    if (length > 0) {
        fwrite(bytes, 1, length, input);
    }
    rewind(input);
    status = ZrLanguageServer_StdioFrameReader_Read(input, limits, outPayload, outLength);
    fclose(input);
    return status;
}

static void test_frame_reader_status_and_limits(void) {
    static const char validFrame[] = "Content-Length: 2\r\n\r\n{}";
    static const char missingLengthFrame[] =
            "Content-Type: application/vscode-jsonrpc\r\n\r\n";
    static const char truncatedFrame[] = "Content-Length: 4\r\n\r\n{}";
    static const char wrongNewlineFrame[] = "Content-Length: 2\n\n{}";
    static const char nulFrame[] = "Content-Length: 2\0junk\r\n\r\n{}";
    static const char limitedMessageFrame[] = "Content-Length: 3\r\n\r\nabc";
    static const char limitedHeaderFrame[] =
            "X-Test: value\r\nContent-Length: 2\r\n\r\n{}";
    SZrStdioFrameReaderLimits limits;
    char *payload = NULL;
    TZrSize length = 0;

    expect_true(read_frame_from_memory(validFrame,
                                       strlen(validFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_OK,
                "valid frame must be accepted by the reader");
    expect_true(length == 2 && payload != NULL && memcmp(payload, "{}", 2) == 0,
                "valid frame payload must be returned exactly");
    free(payload);

    payload = NULL;
    length = 0;
    expect_true(read_frame_from_memory(missingLengthFrame,
                                       strlen(missingLengthFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
                "missing content length must be malformed");
    expect_true(payload == NULL && length == 0,
                "malformed frame must not return a payload");

    expect_true(read_frame_from_memory(truncatedFrame,
                                       strlen(truncatedFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_PAYLOAD_TRUNCATED,
                "short payload must be classified as truncated");
    expect_true(payload == NULL && length == 0,
                "truncated frame must not return a payload");

    expect_true(read_frame_from_memory(wrongNewlineFrame,
                                       strlen(wrongNewlineFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
                "LF-only framing must be malformed");

    expect_true(read_frame_from_memory(nulFrame,
                                       sizeof(nulFrame) - 1U,
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
                "NUL in a header must be malformed");

    memset(&limits, 0, sizeof(limits));
    limits.maxMessageBytes = 2;
    expect_true(read_frame_from_memory(limitedMessageFrame,
                                       strlen(limitedMessageFrame),
                                       &limits,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_TOO_LARGE,
                "injected message limit must classify an oversized frame");
    expect_true(payload == NULL && length == 0,
                "oversized frame must not allocate a payload");

    memset(&limits, 0, sizeof(limits));
    limits.maxHeaderCount = 1;
    expect_true(read_frame_from_memory(limitedHeaderFrame,
                                       strlen(limitedHeaderFrame),
                                       &limits,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_TOO_LARGE,
                "injected header count limit must be enforced");

    expect_true(read_frame_from_memory(NULL,
                                       0,
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_EOF,
                "clean empty input must be classified as EOF");
}

static void test_repeated_server_lifecycle(void) {
    int iteration;

    for (iteration = 0; iteration < 100; iteration++) {
        FILE *input = tmpfile();
        SZrStdioServerOptions options;
        SZrStdioServer *server;

        expect_true(input != NULL, "lifecycle test input must be available");
        if (input == NULL) {
            continue;
        }
        memset(&options, 0, sizeof(options));
        options.input = input;
        server = ZrLanguageServer_StdioServer_New(&options);
        expect_true(server != NULL, "server construction must succeed repeatedly");
        if (server != NULL) {
            expect_true(ZrLanguageServer_StdioServer_Start(server),
                        "reader start must succeed repeatedly");
            ZrLanguageServer_StdioServer_Shutdown(server);
            ZrLanguageServer_StdioServer_Free(server);
        }
        fclose(input);
    }
}

static void test_exit_notification_stops_the_reader(void) {
    static const char payload[] = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}";
    FILE *input = tmpfile();
    SZrStdioServerOptions options;
    SZrStdioServer *server;

    expect_true(input != NULL, "exit-frame input must be available");
    if (input == NULL) {
        return;
    }
    fprintf(input, "Content-Length: %zu\r\n\r\n%s", strlen(payload), payload);
    rewind(input);
    memset(&options, 0, sizeof(options));
    options.input = input;
    server = ZrLanguageServer_StdioServer_New(&options);
    expect_true(server != NULL, "server must construct for an exit frame");
    if (server != NULL) {
        expect_true(ZrLanguageServer_StdioServer_Start(server),
                    "reader must accept an exit notification frame");
        ZrLanguageServer_StdioServer_Shutdown(server);
        ZrLanguageServer_StdioServer_Free(server);
    }
    fclose(input);
}

static void test_startup_failure_uses_the_same_teardown_path(void) {
    const EZrStdioServerFaultPoint newFaults[] = {
            ZR_STDIO_SERVER_FAULT_AFTER_GLOBAL,
            ZR_STDIO_SERVER_FAULT_AFTER_CONTEXT,
            ZR_STDIO_SERVER_FAULT_AFTER_INPUT_INIT,
    };
    size_t index;

    for (index = 0; index < sizeof(newFaults) / sizeof(newFaults[0]); index++) {
        FILE *input = tmpfile();
        SZrStdioServerOptions options;

        expect_true(input != NULL, "fault-injection input must be available");
        if (input == NULL) {
            continue;
        }
        memset(&options, 0, sizeof(options));
        options.input = input;
        options.faultPoint = newFaults[index];
        expect_true(ZrLanguageServer_StdioServer_New(&options) == NULL,
                    "construction fault must release every initialized dependency");
        fclose(input);
    }

    {
        FILE *input = tmpfile();
        SZrStdioServerOptions options;
        SZrStdioServer *server;

        expect_true(input != NULL, "reader-start fault input must be available");
        if (input == NULL) {
            return;
        }
        memset(&options, 0, sizeof(options));
        options.input = input;
        options.faultPoint = ZR_STDIO_SERVER_FAULT_AFTER_READER_START;
        server = ZrLanguageServer_StdioServer_New(&options);
        expect_true(server != NULL, "server must reach the reader-start fault point");
        if (server != NULL) {
            expect_true(!ZrLanguageServer_StdioServer_Start(server),
                        "reader-start fault must be reported to the caller");
            ZrLanguageServer_StdioServer_Free(server);
        }
        fclose(input);
    }
}

int main(void) {
    test_lifecycle_state_transitions();
    test_request_registry_identity_and_cancellation();
    test_frame_reader_status_and_limits();
    test_repeated_server_lifecycle();
    test_exit_notification_stops_the_reader();
    test_startup_failure_uses_the_same_teardown_path();

    if (g_failures != 0) {
        printf("Fail - stdio server lifecycle: %d failures\n", g_failures);
        return 1;
    }

    printf("Pass - stdio server lifecycle\n");
    return 0;
}
