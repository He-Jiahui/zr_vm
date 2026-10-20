#ifndef ZRG_BACKEND_H
#define ZRG_BACKEND_H

#include "runtime.h"

/* The null backend consumes an immutable frame; other backends are later phases. */
typedef struct ZrgBackend {
    const char *name;
    uint32_t (*submit)(const ZrgBatch *batch);
} ZrgBackend;

const ZrgBackend *zrg_backend_null(void);

#endif
