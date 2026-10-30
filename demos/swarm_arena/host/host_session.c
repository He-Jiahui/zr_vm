#include "host.h"

#include <stdio.h>

#if defined(ZRG_HAS_SESSION)
#include "runtime.h"
#include "zr_vm_rust_binding.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#define SESSION_FRAME_US UINT64_C(16667)
#define CHECKPOINT_FRAME 600u
#define REWIND_FRAME 720u

typedef struct SessionHost {
    const ZrgOptions *options;
    ZrRustBindingRuntime *runtime;
    ZrRustBindingProjectWorkspace *workspace;
    ZrRustBindingProjectSession *session;
    ZrRustBindingProjectSessionCheckpoint *checkpoint;
    FILE *csv;
    const char *operation;
    ZrRustBindingStatus binding_status;
    uint32_t completed, replayed, calls, gc_steps;
    uint64_t instructions, elapsed_us, gc_us, peak_heap, native_calls, step_pause_us;
    ZrRustBindingTermination termination;
    int64_t hash600, restored600, hash720, replay720, final_hash;
    int64_t replay_hashes[REWIND_FRAME - CHECKPOINT_FRAME];
    bool booted, checkpoint_created, rolled_back, equal;
} SessionHost;

static bool check(SessionHost *host, ZrRustBindingStatus status, const char *operation) {
    if (status == ZR_RUST_BINDING_STATUS_OK) return true;
    ZrRustBindingErrorInfo error;
    ZrRustBinding_GetLastErrorInfo(&error); /* Read before freeing any handles. */
    host->operation = operation;
    host->binding_status = status;
    fprintf(stderr, "SWARM_ERROR session.%s status=%d %s\n", operation, (int)status, error.message);
    return false;
}

static bool call_int(SessionHost *host, const char *name, int64_t *out) {
    ZrRustBindingValue *result = NULL;
    bool ok = check(host, ZrRustBinding_ProjectSession_CallModuleExport(
        host->session, "game", name, NULL, 0, &result), name);
    if (ok) ok = check(host, ZrRustBinding_Value_ReadInt(result, out), name);
    (void)ZrRustBinding_Value_Free(result);
    return ok;
}

static bool boot(SessionHost *host) {
    ZrRustBindingValue *args[4] = {NULL}, *result = NULL;
    const ZrgOptions *options = host->options;
    int64_t value = -1;
    bool ok = check(host, ZrRustBinding_Value_NewString(options->scenario, &args[0]), "boot.scenario") &&
        check(host, ZrRustBinding_Value_NewString("dod", &args[1]), "boot.lane") &&
        check(host, ZrRustBinding_Value_NewInt(options->seed, &args[2]), "boot.seed") &&
        check(host, ZrRustBinding_Value_NewInt(options->frames, &args[3]), "boot.frames") &&
        check(host, ZrRustBinding_ProjectSession_CallModuleExport(
            host->session, "game", "boot", args, 4, &result), "boot");
    if (ok) {
        host->booted = true;
        ok = check(host, ZrRustBinding_Value_ReadInt(result, &value), "boot.result");
        if (ok && value != 0) {
            host->operation = "boot.result";
            fprintf(stderr, "SWARM_ERROR session.boot returned %" PRId64 "\n", value);
            ok = false;
        }
    }
    (void)ZrRustBinding_Value_Free(result);
    for (size_t i = 0; i < 4; ++i) (void)ZrRustBinding_Value_Free(args[i]);
    return ok;
}

static bool shutdown(SessionHost *host, char *summary, size_t capacity) {
    ZrRustBindingValue *result = NULL;
    bool ok = check(host, ZrRustBinding_ProjectSession_CallModuleExport(
        host->session, "game", "shutdown", NULL, 0, &result), "shutdown");
    if (ok) {
        host->booted = false;
        if (summary) ok = check(host, ZrRustBinding_Value_ReadString(result, summary, capacity), "shutdown.result");
    }
    (void)ZrRustBinding_Value_Free(result);
    return ok;
}

static bool frame(SessionHost *host, uint32_t index, bool replay) {
    ZrRustBindingValue *argument = NULL, *result = NULL;
    ZrRustBindingCallUsage usage = {0};
    ZrRustBindingGcStepResult gc = {0};
    ZrRustBindingCallBudget budget = {.hasInstructionLimit = ZR_TRUE,
        .maxInstructions = host->options->instruction_budget};
    uint64_t start = zrg_now_us(), slack = 0;
    bool requested = false;
    int64_t hash = -1, sprites = -1;
    if (!zrg_platform_poll() ||
        !check(host, ZrRustBinding_Value_NewInt(index, &argument), "frame.argument")) return false;
    bool ok = check(host, ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
        host->session, "game", "frameStep", &argument, 1, &budget, &usage, &result), "frameStep");
    ++host->calls;
    if (ok) ok = check(host, ZrRustBinding_Value_ReadInt(result, &hash), "frame.result");
    /* No session-backed Value handles may survive the frame boundary/checkpoint. */
    (void)ZrRustBinding_Value_Free(result);
    (void)ZrRustBinding_Value_Free(argument);
    if (ok) {
        sprites = zrg_end_frame();
        if (sprites < 0) {
            host->operation = "backend";
            fprintf(stderr, "SWARM_ERROR session.backend missing completed batch\n");
            ok = false;
        }
        zrg_platform_present();
    }
    if (ok) {
        uint64_t elapsed = zrg_now_us() - start;
        slack = elapsed < SESSION_FRAME_US ? SESSION_FRAME_US - elapsed : 0;
        /* Zero is never passed as a pause allowance: some collectors treat it
         * as an unbounded/default budget. No remaining slack means no step. */
        if (slack) {
            requested = true;
            ok = check(host, ZrRustBinding_ProjectSession_GcStep(host->session, slack, &gc), "GcStep");
            if (ok) ++host->gc_steps;
        }
    }
    host->instructions += usage.executedInstructions;
    host->elapsed_us += usage.elapsedMicros;
    host->gc_us += usage.gcMicros;
    host->native_calls += usage.nativeCalls;
    host->step_pause_us += gc.pauseMicros;
    if (usage.peakHeapBytes > host->peak_heap) host->peak_heap = usage.peakHeapBytes;
    host->termination = usage.termination;
    fprintf(host->csv, "%s,%u,%" PRId64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%d,%" PRIu64 ",%" PRIu64 ",%d,%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%d\n", replay ? "replay" : "first", index + 1,
        hash, sprites, usage.executedInstructions, usage.elapsedMicros, usage.gcMicros,
        usage.peakHeapBytes, usage.nativeCalls, (int)usage.termination, zrg_now_us() - start,
        slack, requested, gc.pauseMicros, gc.rootCount, gc.crossBoundaryReferenceCount, ok);
    if (ferror(host->csv)) {
        host->operation = "frames.write";
        fprintf(stderr, "SWARM_ERROR session.frames.csv write failed\n");
        return false;
    }
    if (!ok) return false;
    if (replay) {
        ++host->replayed;
        if (hash != host->replay_hashes[index - CHECKPOINT_FRAME]) {
            host->operation = "rewind.frame_hash";
            fprintf(stderr, "SWARM_ERROR session.rewind frame=%u hash mismatch\n", index + 1);
            return false;
        }
    } else {
        ++host->completed;
        if (index >= CHECKPOINT_FRAME && index < REWIND_FRAME)
            host->replay_hashes[index - CHECKPOINT_FRAME] = hash;
    }
    return true;
}

static bool rewind_session(SessionHost *host) {
    if (!call_int(host, "fullHash", &host->hash720) ||
        !check(host, ZrRustBinding_ProjectSession_Rollback(host->session, host->checkpoint), "Rollback"))
        return false;
    host->rolled_back = true;
    if (!call_int(host, "fullHash", &host->restored600)) return false;
    if (host->restored600 != host->hash600) {
        host->operation = "rewind.checkpoint_hash";
        fprintf(stderr, "SWARM_ERROR session.rewind restored frame 600 hash mismatch\n");
        return false;
    }
    for (uint32_t f = CHECKPOINT_FRAME; f < REWIND_FRAME; ++f)
        if (!frame(host, f, true)) return false;
    if (!call_int(host, "fullHash", &host->replay720)) return false;
    host->equal = host->hash720 == host->replay720;
    if (!host->equal) {
        host->operation = "rewind.final_hash";
        fprintf(stderr, "SWARM_ERROR session.rewind frame 720 hash mismatch\n");
    }
    return host->equal;
}

static bool path(char *out, size_t capacity, const char *directory, const char *name) {
    int n = snprintf(out, capacity, "%s/%s", directory, name);
    return n >= 0 && (size_t)n < capacity;
}

static void hash_field(FILE *file, const char *key, int64_t hash) {
    fprintf(file, "\"%s\": ", key);
    if (hash >= 0) fprintf(file, "%" PRId64, hash);
    else fputs("null", file);
}

static bool write_summary(SessionHost *host, const char *destination, const char *temporary, bool ok) {
    FILE *file = fopen(temporary, "w");
    if (!file) return false;
    fprintf(file, "{\n  \"schema\": \"zr-swarm-session/1\", \"status\": \"%s\",\n"
        "  \"host_api\": \"session\", \"scenario\": \"%s\", \"lane\": \"dod\",\n"
        "  \"seed\": %u, \"frames\": %u, \"completed_frames\": %u, \"replayed_frames\": %u,\n"
        "  \"instruction_budget\": %u, \"budgeted_calls\": %u,\n"
        "  \"executedInstructions\": %" PRIu64 ", \"elapsedMicros\": %" PRIu64 ",\n"
        "  \"gcMicros\": %" PRIu64 ", \"peakHeapBytes\": %" PRIu64 ", \"nativeCalls\": %" PRIu64 ",\n"
        "  \"gc_steps\": %u, \"gc_step_pause_micros\": %" PRIu64 ", \"termination\": %d,\n"
        "  \"failure_operation\": \"%s\", \"binding_status\": %d,\n  ",
        ok ? "OK" : "FAIL", host->options->scenario, host->options->seed, host->options->frames,
        host->completed, host->replayed, host->options->instruction_budget, host->calls,
        host->instructions, host->elapsed_us, host->gc_us, host->peak_heap, host->native_calls,
        host->gc_steps, host->step_pause_us, (int)host->termination,
        ok ? "" : host->operation, (int)host->binding_status);
    hash_field(file, "hash", host->final_hash);
    fprintf(file, ",\n  \"rewind\": {\"requested\": %s, \"checkpoint_frame\": 600, \"target_frame\": 720,\n"
        "    \"checkpoint_created\": %s, \"rollback_succeeded\": %s, \"equal\": %s,\n    ",
        host->options->rewind ? "true" : "false", host->checkpoint_created ? "true" : "false",
        host->rolled_back ? "true" : "false", host->equal ? "true" : "false");
    hash_field(file, "checkpoint_hash", host->hash600); fputs(", ", file);
    hash_field(file, "restored_hash", host->restored600); fputs(",\n    ", file);
    hash_field(file, "first_hash", host->hash720); fputs(", ", file);
    hash_field(file, "replay_hash", host->replay720);
    fputs("},\n  \"measurement\": {\"call_scope\": \"game.frameStep: tick and render preparation\",\n"
        "    \"gc_step_scope\": \"outside call usage; maxPauseMicros equals remaining 16667us frame slack\",\n"
        "    \"peak_heap_scope\": \"VM allocator live requested bytes, including preexisting allocations\",\n"
        "    \"limits\": \"cooperative instruction cap; native callbacks and GC may overshoot time slack\"}\n}\n", file);
    bool saved = !ferror(file);
    if (fclose(file) != 0) saved = false;
    if (saved) saved = rename(temporary, destination) == 0;
    if (!saved) (void)unlink(temporary);
    return saved;
}

int zrg_session_run(const ZrgOptions *options) {
    SessionHost host = {.options = options, .operation = "open", .hash600 = -1,
        .restored600 = -1, .hash720 = -1, .replay720 = -1, .final_hash = -1};
    ZrRustBindingRuntimeOptions runtime_options = {.pauseBudgetUs = options->gc_budget_us,
        .remarkBudgetUs = options->gc_budget_us, .workerCount = 1};
    ZrRustBindingRunOptions run_options = {.executionMode = ZR_RUST_BINDING_EXECUTION_MODE_INTERP};
    char csv_path[4096], json_path[4096], temporary[4096], summary[2048];
    bool ok = false;
    if (!zrg_platform_make_directory(options->out) ||
        !path(csv_path, sizeof(csv_path), options->out, "session-frames.csv") ||
        !path(json_path, sizeof(json_path), options->out, "session.json") ||
        !path(temporary, sizeof(temporary), options->out, "session.json.tmp") ||
        (unlink(json_path) != 0 && errno != ENOENT) || !(host.csv = fopen(csv_path, "w"))) {
        fprintf(stderr, "SWARM_ERROR --out cannot create session artifacts\n");
        return 2;
    }
    fputs("pass,frame,hash,sprites,executedInstructions,elapsedMicros,gcMicros,peakHeapBytes,nativeCalls,"
        "termination,frameMicros,slackMicros,gcStepRequested,gcStepPauseMicros,gcStepRoots,"
        "gcStepCrossBoundaryReferences,success\n", host.csv);
    zrg_set_host_driven(true);
    zrg_zones_reset();
    if (!check(&host, ZrRustBinding_Runtime_NewStandard(&runtime_options, &host.runtime), "Runtime_NewStandard") ||
        !check(&host, ZrRustBinding_Project_Open(options->project, &host.workspace), "Project_Open") ||
        !check(&host, ZrRustBinding_ProjectSession_Start(host.runtime, host.workspace, &run_options, &host.session), "Start") ||
        !boot(&host)) goto done;
    for (uint32_t f = 0; f < options->frames; ++f) {
        if (!frame(&host, f, false)) goto done;
        if (options->rewind && f + 1 == CHECKPOINT_FRAME) {
            if (!call_int(&host, "fullHash", &host.hash600) ||
                !check(&host, ZrRustBinding_ProjectSession_Checkpoint(host.session, &host.checkpoint), "Checkpoint"))
                goto done;
            host.checkpoint_created = true;
        }
        if (options->rewind && f + 1 == REWIND_FRAME && !rewind_session(&host)) goto done;
    }
    if (!call_int(&host, "fullHash", &host.final_hash) || !shutdown(&host, summary, sizeof(summary))) goto done;
    if (options->expect_hash >= 0 && host.final_hash != options->expect_hash) {
        host.operation = "expect_hash";
        fprintf(stderr, "SWARM_ERROR --expect-hash mismatch: expected %" PRId64 " got %" PRId64 "\n",
            options->expect_hash, host.final_hash);
        goto done;
    }
    ok = true;
done:
    if (host.booted) {
        const char *failure_operation = host.operation;
        ZrRustBindingStatus failure_status = host.binding_status;
        (void)shutdown(&host, NULL, 0);
        /* Cleanup diagnostics must not replace the operation that failed. */
        if (!ok) {
            host.operation = failure_operation;
            host.binding_status = failure_status;
        }
    }
    (void)ZrRustBinding_ProjectSessionCheckpoint_Free(host.checkpoint);
    (void)ZrRustBinding_ProjectSession_Free(host.session);
    (void)ZrRustBinding_ProjectWorkspace_Free(host.workspace);
    (void)ZrRustBinding_Runtime_Free(host.runtime);
    zrg_set_host_driven(false);
    if (fclose(host.csv) != 0) { host.operation = "frames.close"; ok = false; }
    if (!write_summary(&host, json_path, temporary, ok)) {
        fprintf(stderr, "SWARM_ERROR --out cannot write session.json\n");
        return 2;
    }
    if (!ok) return 2;
    if (options->rewind)
        printf("SWARM_REWIND checkpoint=600 target=720 first_hash=%" PRId64 " replay_hash=%" PRId64 " equal=true\n",
            host.hash720, host.replay720);
    puts(summary);
    return 0;
}
#else
int zrg_session_run(const ZrgOptions *options) {
    (void)options;
    fprintf(stderr, "SWARM_ERROR --host-api session requires BUILD_RUST_BINDING=ON and BUILD_NETWORK_LIB=ON\n");
    return 2;
}
#endif
