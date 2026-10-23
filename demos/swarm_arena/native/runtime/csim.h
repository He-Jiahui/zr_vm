#ifndef ZRG_CSIM_H
#define ZRG_CSIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Independent native reference for rules gd-c5-v1/1. No VM or managed heap. */
typedef struct ZrgCSim ZrgCSim;

ZrgCSim *zrg_csim_new(const char *scenario, uint32_t seed, uint32_t frames);
void zrg_csim_free(ZrgCSim *sim);
int64_t zrg_csim_tick(ZrgCSim *sim, int64_t frame);
/* Leaves one shared bulk batch active; the host owns end_frame/present. */
int64_t zrg_csim_render(ZrgCSim *sim, int64_t frame);
int64_t zrg_csim_full_hash(const ZrgCSim *sim);
int64_t zrg_csim_entity_max(const ZrgCSim *sim, int64_t kind);
bool zrg_csim_summary(const ZrgCSim *sim, char *summary, size_t capacity);

#endif
