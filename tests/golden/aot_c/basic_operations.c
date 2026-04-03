/* ZR AOT C Backend */
/* SemIR -> AOTIR textual lowering. */
typedef int TZrBool;
struct SZrState;
struct SZrFunction;
struct SZrTypeValue;

static TZrBool zr_aot_entry(struct SZrState *state, struct SZrFunction *function) {
    (void)state;
    (void)function;
    return 1;
}
