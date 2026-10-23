#define _GNU_SOURCE
#include "host.h"
#include "runtime.h"
#include "zr_vm_core/gc.h"

#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef ZR_SWARM_GIT
#define ZR_SWARM_GIT "unknown"
#endif
#ifndef ZR_SWARM_BUILD
#define ZR_SWARM_BUILD "unknown"
#endif

#define FRAME_BUDGET_US UINT64_C(16667)

typedef struct Frame {
    uint64_t frame_us, tick_us, render_us, backend_us, gc_step_us;
    uint64_t minor, major, full, gc_total_us, gc_max_us, alloc_estimate;
    uint64_t managed_bytes, gc_steps, manual_requests;
    int64_t debt_bytes, rss_kb, hash, sprites;
} Frame;

static uint64_t difference(uint64_t after, uint64_t before) {
    return after >= before ? after - before : 0;
}

static uint64_t collection_time(const SZrGarbageCollectorStatsSnapshot *snapshot) {
    return snapshot->minorCollectionTotalDurationUs + snapshot->majorCollectionTotalDurationUs +
           snapshot->fullCollectionTotalDurationUs;
}

static uint64_t max_pause(const SZrGarbageCollectorStatsSnapshot *snapshot) {
    uint64_t maximum = snapshot->minorCollectionMaxDurationUs;
    if (snapshot->majorCollectionMaxDurationUs > maximum) maximum = snapshot->majorCollectionMaxDurationUs;
    if (snapshot->fullCollectionMaxDurationUs > maximum) maximum = snapshot->fullCollectionMaxDurationUs;
    return maximum;
}

static bool policy_step(ZrgHostVm *vm, const ZrgOptions *options, uint32_t frame,
                         uint64_t start, Frame *row) {
    if (!vm->global) return true; /* C owns no managed heap; GC policies are inert. */
    if (options->gc_policy == ZRG_GC_MANUAL) {
        if ((frame + 1) % options->minor_interval == 0) {
            if (!zrg_host_vm_call_int(vm, "gcMinor", NULL, 0, NULL)) return false;
            row->manual_requests = 1;
        }
    } else if (options->gc_policy == ZRG_GC_SLACK) {
        uint64_t now = zrg_now_us();
        uint64_t elapsed = difference(now, start);
        uint64_t available = elapsed < FRAME_BUDGET_US ? FRAME_BUDGET_US - elapsed : 0;
        if (available > options->gc_budget_us) available = options->gc_budget_us;
        uint64_t deadline = now + available;
        while (available && now < deadline) {
            uint64_t remaining = deadline - now;
            ZrCore_GarbageCollector_SetPauseBudgetUs(vm->global, remaining, remaining);
            if (!zrg_host_vm_gc_step(vm)) return false;
            ++row->gc_steps;
            SZrGarbageCollectorStatsSnapshot step;
            ZrCore_GarbageCollector_GetStatsSnapshot(vm->global, &step);
            if (!step.lastStepWork) break; /* Do not spin when there is no useful work. */
            now = zrg_now_us();
        }
        /* Automatic collections during the next tick retain the configured cap. */
        ZrCore_GarbageCollector_SetPauseBudgetUs(vm->global, options->gc_budget_us, options->gc_budget_us);
    }
    return true;
}

static int compare_u64(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static void sorted_values(const Frame *frames, size_t count, size_t offset, uint64_t *values) {
    for (size_t i = 0; i < count; ++i)
        memcpy(&values[i], (const unsigned char *)&frames[i] + offset, sizeof(values[i]));
    qsort(values, count, sizeof(values[0]), compare_u64);
}

static uint64_t percentile(const uint64_t *sorted, size_t count, size_t percent) {
    return sorted[(count * percent + 99) / 100 - 1];
}

static void timing(FILE *file, const char *name, const Frame *frames, size_t count, size_t offset) {
    uint64_t values[1800];
    sorted_values(frames, count, offset, values);
    double total = 0.0;
    for (size_t i = 0; i < count; ++i) total += (double)values[i];
    fprintf(file, "  \"%s\": {\"p50\": %" PRIu64 ", \"p95\": %" PRIu64
        ", \"p99\": %" PRIu64 ", \"max\": %" PRIu64 ", \"mean\": %.6f},\n",
        name, percentile(values, count, 50), percentile(values, count, 95),
        percentile(values, count, 99), values[count - 1], total / (double)count);
}

static bool summary_file(const char *path, const char *temporary, const ZrgOptions *options,
                           const Frame *frames, int64_t hash, const int64_t entities[3]) {
    FILE *file = fopen(temporary, "w");
    if (!file) return false;
    uint64_t minor = 0, major = 0, full = 0, total_us = 0, maximum = 0, steps = 0, requests = 0;
    uint64_t misses60 = 0, misses120 = 0, allocations[1800];
    int64_t rss = 0;
    for (size_t i = 0; i < options->frames; ++i) {
        const Frame *row = &frames[i];
        minor += row->minor; major += row->major; full += row->full;
        total_us += row->gc_total_us; steps += row->gc_steps; requests += row->manual_requests;
        if (row->gc_max_us > maximum) maximum = row->gc_max_us;
        if (row->rss_kb > rss) rss = row->rss_kb;
        if (row->frame_us > 16667) ++misses60;
        if (row->frame_us > 8333) ++misses120;
    }
    sorted_values(frames, options->frames, offsetof(Frame, alloc_estimate), allocations);
    fprintf(file, "{\n  \"schema\": \"zr-swarm-perf/1\",\n  \"git\": \"%s\", \"build\": \"%s\",\n",
        ZR_SWARM_GIT, ZR_SWARM_BUILD);
    int cpu = sched_getcpu();
    if (cpu < 0) fputs("  \"cpu\": null,", file);
    else fprintf(file, "  \"cpu\": %d,", cpu);
    double load[3];
    if (getloadavg(load, 3) == 3)
        fprintf(file, " \"load_avg\": [%.6f, %.6f, %.6f],\n", load[0], load[1], load[2]);
    else fputs(" \"load_avg\": null,\n", file);
    bool native = !strcmp(options->lane, "c");
    fprintf(file, "  \"scenario\": \"%s\", \"lane\": \"%s\", \"drive\": \"host\", \"exec\": \"%s\",\n"
        "  \"render\": \"null\", \"submit\": \"bulk\", \"gc_policy\": \"%s\", \"gc_budget_us\": %u,\n"
        "  \"seed\": %u, \"frames\": %u, \"warmup\": 0, \"hash\": %" PRId64 ",\n"
        "  \"status\": \"%s\",\n",
        options->scenario, options->lane, native ? "native" : "interp",
        zrg_gc_policy_name(options->gc_policy), options->gc_budget_us, options->seed,
        options->frames, hash, options->expect_hash >= 0 && hash != options->expect_hash ? "hash_mismatch" : "ok");
    timing(file, "frame_us", frames, options->frames, offsetof(Frame, frame_us));
    timing(file, "tick_us", frames, options->frames, offsetof(Frame, tick_us));
    timing(file, "render_us", frames, options->frames, offsetof(Frame, render_us));
    timing(file, "backend_us", frames, options->frames, offsetof(Frame, backend_us));
    fprintf(file, "  \"zones_us\": {},\n"
        "  \"gc\": {\"minor\": %" PRIu64 ", \"major\": %" PRIu64 ", \"full\": %" PRIu64
        ", \"total_us\": %" PRIu64 ", \"max_pause_us\": %" PRIu64 ", \"p99_pause_us\": null,\n"
        "    \"pause_source\": \"%s\",\n"
        "    \"slack_steps\": %" PRIu64 ", \"manual_requests\": %" PRIu64 ", \"manual_interval_frames\": %u},\n"
        "  \"alloc\": {\"bytes_per_frame_p50\": %" PRIu64 ", \"bytes_per_frame_max\": %" PRIu64 ", \"exact\": false,\n"
        "    \"source\": \"%s\"},\n"
        "  \"rss_kb_max\": %" PRId64 ",\n"
        "  \"budget_misses\": {\"16667\": %" PRIu64 ", \"8333\": %" PRIu64 "},\n"
        "  \"entities\": {\"enemies_max\": %" PRId64 ", \"bullets_max\": %" PRId64 ", \"particles_max\": %" PRId64 "},\n"
        "  \"measurement\": {\"clock\": \"CLOCK_MONOTONIC\", \"percentiles\": \"nearest-rank, all frames\",\n"
        "    \"frame_scope\": \"poll through GC policy; metrics and artifact I/O excluded\",\n"
        "    \"entities_scope\": \"live counts at end of tick\", \"cpu_scope\": \"sample at summary; unpinned\"}\n}\n",
        minor, major, full, total_us, maximum,
        native ? "C lane has no managed heap" : "owner-thread snapshots; max includes boot; exact pause samples unavailable",
        steps, requests, options->minor_interval,
        percentile(allocations, options->frames, 50), allocations[options->frames - 1],
        native ? "C native allocations are not measured" : "positive managedMemoryBytes frame deltas; collections can mask allocations",
        rss, misses60, misses120, entities[0], entities[1], entities[2]);
    bool ok = !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) (void)unlink(temporary);
    return ok;
}

static bool output_path(char *buffer, size_t capacity, const char *directory, const char *name) {
    int length = snprintf(buffer, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
}

int zrg_host_loop(ZrgHostVm *vm, const ZrgOptions *options) {
    char csv_path[4096], json_path[4096], temporary[4096], summary[2048];
    Frame frames[1800] = {{0}};
    int64_t hash, entities[3];
    int status = 2;
    if (!zrg_platform_make_directory(options->out) ||
        !output_path(csv_path, sizeof(csv_path), options->out, "frames.csv") ||
        !output_path(json_path, sizeof(json_path), options->out, "summary.json") ||
        !output_path(temporary, sizeof(temporary), options->out, "summary.json.tmp")) {
        fprintf(stderr, "SWARM_ERROR --out cannot create output directory\n");
        return 2;
    }
    /* Reusing --out must not leave a successful summary from a previous failed run. */
    if (unlink(json_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "SWARM_ERROR --out cannot replace summary.json\n");
        return 2;
    }
    FILE *csv = fopen(csv_path, "w");
    if (!csv) {
        fprintf(stderr, "SWARM_ERROR --out cannot open frames.csv\n");
        return 2;
    }
    fputs("frame,hash,sprites,frame_us,tick_us,render_us,backend_us,gc_step_us,gc_minor,gc_major,gc_full,"
          "gc_total_us,gc_max_pause_us,managed_bytes,gc_debt_bytes,alloc_bytes_estimate,rss_kb,gc_steps,manual_minor_requests\n", csv);
    for (uint32_t f = 0; f < options->frames; ++f) {
        Frame *row = &frames[f];
        SZrGarbageCollectorStatsSnapshot before = {0}, after = {0};
        if (vm->global) ZrCore_GarbageCollector_GetStatsSnapshot(vm->global, &before);
        uint64_t t0 = zrg_now_us();
        int64_t argument = f;
        if (!zrg_platform_poll() || !zrg_host_vm_call_int(vm, "tick", &argument, 1, &row->hash)) goto done;
        uint64_t t1 = zrg_now_us();
        if (!zrg_host_vm_call_int(vm, "render", &argument, 1, &row->sprites)) goto done;
        uint64_t t2 = zrg_now_us();
        /* Both lanes leave the shared bulk batch active for this owner. */
        if (zrg_end_frame() != row->sprites) {
            fprintf(stderr, "SWARM_ERROR backend sprite count mismatch\n");
            goto done;
        }
        zrg_platform_present();
        uint64_t t3 = zrg_now_us();
        if (!policy_step(vm, options, f, t0, row)) goto done;
        uint64_t t4 = zrg_now_us();
        if (vm->global) ZrCore_GarbageCollector_GetStatsSnapshot(vm->global, &after);
        row->frame_us = difference(t4, t0); row->tick_us = difference(t1, t0);
        row->render_us = difference(t2, t1); row->backend_us = difference(t3, t2);
        row->gc_step_us = difference(t4, t3);
        row->minor = difference(after.minorCollectionCount, before.minorCollectionCount);
        row->major = difference(after.majorCollectionCount, before.majorCollectionCount);
        row->full = difference(after.fullCollectionCount, before.fullCollectionCount);
        row->gc_total_us = difference(collection_time(&after), collection_time(&before));
        row->gc_max_us = max_pause(&after);
        row->alloc_estimate = difference(after.managedMemoryBytes, before.managedMemoryBytes);
        row->managed_bytes = after.managedMemoryBytes; row->debt_bytes = after.gcDebtBytes;
        row->rss_kb = zrg_rss_kb();
        fprintf(csv, "%u,%" PRId64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRId64 ",%" PRIu64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64 "\n",
            f, row->hash, row->sprites, row->frame_us, row->tick_us, row->render_us, row->backend_us,
            row->gc_step_us, row->minor, row->major, row->full, row->gc_total_us, row->gc_max_us,
            row->managed_bytes, row->debt_bytes, row->alloc_estimate, row->rss_kb, row->gc_steps, row->manual_requests);
        if (ferror(csv)) {
            fprintf(stderr, "SWARM_ERROR --out frames.csv write failed\n");
            goto done;
        }
    }
    if (!zrg_host_vm_call_int(vm, "fullHash", NULL, 0, &hash)) goto done;
    for (int64_t kind = 0; kind < 3; ++kind)
        if (!zrg_host_vm_call_int(vm, "entityMax", &kind, 1, &entities[kind])) goto done;
    if (!zrg_host_vm_shutdown(vm, summary, sizeof(summary))) goto done;
    if (fclose(csv) != 0) {
        csv = NULL;
        fprintf(stderr, "SWARM_ERROR --out frames.csv close failed\n");
        goto done;
    }
    csv = NULL;
    if (!summary_file(json_path, temporary, options, frames, hash, entities)) {
        fprintf(stderr, "SWARM_ERROR --out summary.json write failed\n");
        goto done;
    }
    if (options->expect_hash >= 0 && hash != options->expect_hash) {
        fprintf(stderr, "SWARM_ERROR --expect-hash mismatch: expected %" PRId64 " got %" PRId64 "\n", options->expect_hash, hash);
        goto done;
    }
    puts(summary);
    status = 0;
done:
    if (csv && fclose(csv) != 0) fprintf(stderr, "SWARM_ERROR --out frames.csv close failed\n");
    return status;
}
