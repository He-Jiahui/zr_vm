#include "backend.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTES ((size_t)ZRG_RENDER_WIDTH * ZRG_RENDER_HEIGHT * 4)
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "SOFT_TEST_ERROR line=%d %s\n", __LINE__, #condition); ok = false; goto done; \
} } while (0)

static bool pixel(const uint8_t *image, unsigned x, unsigned y, unsigned r, unsigned g, unsigned b) {
    const uint8_t *p = image + ((size_t)y * ZRG_RENDER_WIDTH + x) * 4;
    return p[0] == r && p[1] == g && p[2] == b && p[3] == 255;
}

static bool frame(unsigned index, float dx, float dy) {
    /* Same world geometry and HUD as the M1 fixture. Soft's zero-rotation
     * instances are circles; the three nonzero instances remain squares. */
    static const ZrgSprite sprites[] = {
        {560, 1740, 96, 0, UINT32_C(0xff4020ff)},
        {960, 1940, 128, 0.45f, UINT32_C(0x20c060ff)},
        {960, 1940, 64, -0.25f, UINT32_C(0x4060ff80)},
        {1640, 2220, 120, 0.3f, UINT32_C(0xffd040ff)},
        {560, 2240, 40, 0, UINT32_C(0x4080ffff)},
        {1360, 1840, 0, 0, UINT32_C(0xffffffff)},
        {3000, 3000, 16, 0, UINT32_C(0xffffffff)}
    };
    zrg_begin_frame(index);
    if (!zrg_camera(1000 + dx, 2000 + dy)) return false;
    for (size_t i = 0; i < sizeof(sprites) / sizeof(*sprites); ++i) {
        ZrgSprite instance = sprites[i]; instance.x += dx; instance.y += dy;
        if (!zrg_sprite(instance)) return false;
    }
    static const char text[] = "GL33 120:OK\nWASD";
    return zrg_text(16, 16, text, sizeof(text) - 1) && zrg_end_frame() == 7;
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    bool ok = true;
    uint8_t *first = malloc(BYTES), *second = malloc(BYTES);
    ZrgBackend *backend = zrg_backend_soft();
    ZrgBackendDesc desc = {ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT, true};
    CHECK(first && second);
    ZrgBackendDesc invalid = {1, 1, true};
    CHECK(!zrg_backend_open("soft", &invalid));
    CHECK(zrg_backend_open("soft", &desc));
    CHECK(!zrg_snapshot_write(argv[1]));
    CHECK(frame(0, 0, 0));
    uint32_t hash = zrg_snapshot("");
    CHECK(hash != zrg_batch_hash(zrg_batch()));
    CHECK(backend->read_rgba8(backend, first, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    CHECK(pixel(first, 5, 5, 16, 20, 28));
    CHECK(pixel(first, 100, 50, 255, 64, 32));
    CHECK(pixel(first, 77, 40, 16, 20, 28)); /* Inside square, outside circle. */
    CHECK(pixel(first, 100, 300, 64, 128, 255));
    CHECK(pixel(first, 300, 150, 48, 144, 176)); /* Ordered alpha overlap. */
    CHECK(pixel(first, 330, 150, 32, 192, 96)); /* Rotated square only. */
    CHECK(pixel(first, 335, 110, 16, 20, 28));
    CHECK(pixel(first, 17, 16, 232, 237, 245)); /* G's first lit pixel. */
    zrg_backend_present();
    CHECK(zrg_snapshot_write(argv[1]));
    CHECK(zrg_snapshot_write(argv[2]));
    CHECK(!zrg_snapshot_write("unsupported.jpg"));
    CHECK(frame(1, 32, 64));
    CHECK(zrg_snapshot("") == hash);
    CHECK(backend->read_rgba8(backend, second, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    CHECK(memcmp(first, second, BYTES) == 0);
    CHECK(!backend->read_rgba8(backend, second, 1, 1));
    CHECK(!backend->read_rgba8(backend, NULL, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    zrg_begin_frame(2);
    CHECK(zrg_camera(0, 0));
    CHECK(zrg_sprite((ZrgSprite){0, 0, 100, 0, 0})); /* Alpha zero. */
    CHECK(zrg_sprite((ZrgSprite){FLT_MAX, -FLT_MAX, 16, FLT_MAX, UINT32_MAX}));
    CHECK(zrg_text(FLT_MAX, -FLT_MAX, "CLIPPED", 7));
    CHECK(zrg_end_frame() == 2);
    CHECK(backend->read_rgba8(backend, second, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    for (unsigned y = 0; y < ZRG_RENDER_HEIGHT; ++y)
        for (unsigned x = 0; x < ZRG_RENDER_WIDTH; ++x)
            CHECK(pixel(second, x, y, 16, 20, 28));
    /* Huge covering circle must not overflow the bounds or squared distance. */
    zrg_begin_frame(3);
    CHECK(zrg_sprite((ZrgSprite){0, 0, FLT_MAX, 0, UINT32_C(0x010203ff)}));
    CHECK(zrg_end_frame() == 1);
    CHECK(backend->read_rgba8(backend, second, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    CHECK(pixel(second, 0, 0, 1, 2, 3) && pixel(second, 639, 359, 1, 2, 3));
    zrg_begin_frame(4);
    CHECK(zrg_end_frame() == 0);
    CHECK(backend->read_rgba8(backend, second, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    CHECK(pixel(second, 320, 180, 16, 20, 28));
    zrg_backend_close();
    CHECK(zrg_backend_open("soft", &desc));
    CHECK(!zrg_snapshot_write(argv[1])); /* Reopen cannot expose an old frame. */
done:
    zrg_backend_close();
    free(first); free(second);
    if (ok) puts("SOFT_TEST_OK circles rotation camera rgba alpha clipping font empty readback");
    return ok ? 0 : 1;
}
