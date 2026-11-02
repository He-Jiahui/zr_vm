#include "backend.h"
#include "platform.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define BYTES ((size_t)ZRG_RENDER_WIDTH * ZRG_RENDER_HEIGHT * 4)
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "GL_TEST_ERROR line=%d %s\n", __LINE__, #condition); ok = false; goto done; \
} } while (0)

static bool pixel(const uint8_t *image, unsigned x, unsigned y, unsigned r, unsigned g, unsigned b) {
    const uint8_t *p = image + ((size_t)y * ZRG_RENDER_WIDTH + x) * 4;
    return abs((int)p[0] - (int)r) <= 1 && abs((int)p[1] - (int)g) <= 1 &&
           abs((int)p[2] - (int)b) <= 1 && p[3] == 255;
}

static bool frame(unsigned index, float dx, float dy) {
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
        ZrgSprite sprite = sprites[i]; sprite.x += dx; sprite.y += dy;
        if (!zrg_sprite(sprite)) return false;
    }
    static const char text[] = "GL33 120:OK\nWASD";
    if (!zrg_text(16, 16, text, sizeof(text) - 1)) return false;
    return zrg_end_frame() == 7;
}

static int keyboard(void) {
    if (!zrg_platform_open(true, false, true)) return 77;
    int64_t last[2] = {99, 99};
    uint64_t deadline = zrg_now_us() + UINT64_C(15000000);
    while (zrg_now_us() < deadline) {
        if (!zrg_platform_poll()) {
            puts("KEYS_CLOSED"); fflush(stdout);
            zrg_platform_close();
            return 0;
        }
        int64_t axes[2];
        zrg_platform_input(axes);
        if (axes[0] != last[0] || axes[1] != last[1]) {
            printf("KEYS %" PRId64 " %" PRId64 "\n", axes[0], axes[1]); fflush(stdout);
            last[0] = axes[0]; last[1] = axes[1];
        }
        zrg_platform_present();
    }
    zrg_platform_close();
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "--keys")) return keyboard();
    /* Only context availability is skippable; shader/render/test failures fail. */
    if (!zrg_platform_open(true, true, false)) return 77;
    if (!strcmp(argv[1], "--probe")) { zrg_platform_close(); return 0; }
    bool ok = true;
    uint8_t *first = malloc(BYTES), *second = malloc(BYTES);
    ZrgBackendDesc desc = {ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT, true};
    CHECK(first && second);
    CHECK(zrg_backend_open("gl", &desc));
    CHECK(!zrg_snapshot_write(argv[1])); /* No completed frame yet. */
    CHECK(frame(0, 0, 0));
    uint32_t image_hash = zrg_snapshot("");
    CHECK(image_hash != zrg_batch_hash(zrg_batch()));
    CHECK(zrg_backend_gl33()->read_rgba8(zrg_backend_gl33(), first, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    CHECK(pixel(first, 5, 5, 16, 20, 28));
    CHECK(pixel(first, 100, 50, 255, 64, 32));
    CHECK(pixel(first, 100, 300, 64, 128, 255));
    CHECK(pixel(first, 300, 150, 48, 144, 176));
    zrg_platform_present();
    CHECK(zrg_snapshot_write(argv[1])); /* Readback after swapping remains valid. */
    CHECK(frame(1, 32, 64));
    CHECK(zrg_snapshot("") == image_hash);
    CHECK(zrg_backend_gl33()->read_rgba8(zrg_backend_gl33(), second, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    CHECK(memcmp(first, second, BYTES) == 0); /* Camera translation invariance. */
    CHECK(!zrg_backend_gl33()->read_rgba8(zrg_backend_gl33(), second, 1, 1));
    zrg_begin_frame(2);
    CHECK(zrg_end_frame() == 0);
    CHECK(zrg_backend_gl33()->read_rgba8(zrg_backend_gl33(), second, ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT));
    for (size_t i = 0; i < BYTES; i += 4)
        CHECK(second[i] == 16 && second[i + 1] == 20 && second[i + 2] == 28 && second[i + 3] == 255);
done:
    zrg_backend_close();
    zrg_platform_close();
    free(first); free(second);
    if (ok) puts("GL_TEST_OK camera rgba alpha rotation clipping text empty readback");
    return ok ? 0 : 1;
}
