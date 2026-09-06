//
// Stdio server deterministic teardown contract tests.
//

#include <stdio.h>
#include <string.h>

#include "stdio_lifecycle.h"
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
