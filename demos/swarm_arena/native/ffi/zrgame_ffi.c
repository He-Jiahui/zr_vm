#include <stdint.h>
double zrg_noise2(double x, double y);
int64_t zrg_add_i64(int64_t a, int64_t b);
void zrg_nop(void);
double zrg_noise2(double x, double y) { return x * 0.25 + y * 0.75; }
int64_t zrg_add_i64(int64_t a, int64_t b) { return a + b; }
void zrg_nop(void) { }
