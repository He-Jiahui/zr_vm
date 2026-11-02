#ifndef ZRG_BACKEND_H
#define ZRG_BACKEND_H

#include "runtime.h"

#define ZRG_RENDER_WIDTH 640u
#define ZRG_RENDER_HEIGHT 360u

typedef struct ZrgBackendDesc {
    uint32_t width, height;
    bool headless;
} ZrgBackendDesc;

/* One owner thread; platform supplies a current context before GL init and
 * keeps it alive until shutdown. read_rgba8 returns top-to-bottom RGBA8. */
typedef struct ZrgBackend {
    const char *name;
    int (*init)(struct ZrgBackend *, const ZrgBackendDesc *);
    void (*begin)(struct ZrgBackend *, float camera_x, float camera_y);
    void (*submit)(struct ZrgBackend *, const ZrgSprite *, uint32_t, const ZrgText *, uint32_t);
    void (*present)(struct ZrgBackend *);
    int (*read_rgba8)(struct ZrgBackend *, uint8_t *, uint32_t, uint32_t);
    void (*shutdown)(struct ZrgBackend *);
    bool failed;
} ZrgBackend;

ZrgBackend *zrg_backend_null(void);
ZrgBackend *zrg_backend_soft(void);
#ifdef ZRG_HAS_GL33
ZrgBackend *zrg_backend_gl33(void);
#endif
uint32_t zrg_batch_hash(const ZrgBatch *batch);
bool zrg_backend_open(const char *name, const ZrgBackendDesc *desc);
void zrg_backend_close(void);
void zrg_backend_present(void);
bool zrg_backend_ok(void);
bool zrg_write_ppm(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height);
bool zrg_write_png(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height);

#endif
