#define _POSIX_C_SOURCE 200809L
#include "runtime.h"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static bool host_driven;
static ZrgZoneStats zones[ZRG_ZONE_CAPACITY];
static struct { uint32_t id; uint64_t start; } zone_stack[ZRG_ZONE_STACK_CAPACITY];
static uint32_t zone_depth;

uint64_t zrg_now_us(void) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return 0;
    return (uint64_t)time.tv_sec * UINT64_C(1000000) + (uint64_t)time.tv_nsec / 1000u;
}

int64_t zrg_rss_kb(void) {
    unsigned long pages = 0;
    unsigned long resident = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    FILE *file = fopen("/proc/self/statm", "r");
    if (!file) return 0;
    int read = fscanf(file, "%lu %lu", &pages, &resident);
    fclose(file);
    return read == 2 && page_size > 0 ? (int64_t)((uint64_t)resident * (uint64_t)page_size / 1024u) : 0;
}

bool zrg_is_host_driven(void) { return host_driven; }
void zrg_set_host_driven(bool value) { host_driven = value; }

void zrg_zones_reset(void) {
    for (uint32_t i = 0; i < ZRG_ZONE_CAPACITY; ++i) zones[i] = (ZrgZoneStats){0};
    zone_depth = 0;
}

bool zrg_zone_begin_at(uint32_t id, uint64_t now) {
    if (id >= ZRG_ZONE_CAPACITY || zone_depth == ZRG_ZONE_STACK_CAPACITY) return false;
    zone_stack[zone_depth].id = id;
    zone_stack[zone_depth++].start = now;
    return true;
}

bool zrg_zone_end_at(uint32_t id, uint64_t now) {
    if (!zone_depth || zone_stack[zone_depth - 1].id != id || now < zone_stack[zone_depth - 1].start) return false;
    uint64_t elapsed = now - zone_stack[--zone_depth].start;
    ZrgZoneStats *zone = &zones[id];
    ++zone->count;
    zone->total_us += elapsed;
    if (elapsed > zone->max_us) zone->max_us = elapsed;
    return true;
}

bool zrg_zone_begin(uint32_t id) { return zrg_zone_begin_at(id, zrg_now_us()); }
bool zrg_zone_end(uint32_t id) { return zrg_zone_end_at(id, zrg_now_us()); }

bool zrg_zone_stats(uint32_t id, ZrgZoneStats *out) {
    if (id >= ZRG_ZONE_CAPACITY || !out) return false;
    *out = zones[id];
    return true;
}

bool zrg_zone_report(char *out, size_t capacity) {
    size_t used = 0;
    bool first = true;
    if (!out || capacity < sizeof("{\"zones\":[]}")) return false;
    int size = snprintf(out, capacity, "{\"zones\":[");
    used = (size_t)size;
    for (uint32_t i = 0; i < ZRG_ZONE_CAPACITY; ++i) {
        const ZrgZoneStats *zone = &zones[i];
        if (!zone->count) continue;
        size = snprintf(out + used, capacity - used,
            "%s{\"id\":%u,\"count\":%" PRIu64 ",\"total_us\":%" PRIu64 ",\"max_us\":%" PRIu64 "}",
            first ? "" : ",", i, zone->count, zone->total_us, zone->max_us);
        if (size < 0 || (size_t)size >= capacity - used) return false;
        used += (size_t)size;
        first = false;
    }
    return snprintf(out + used, capacity - used, "]}") == 2 && capacity - used > 2;
}
