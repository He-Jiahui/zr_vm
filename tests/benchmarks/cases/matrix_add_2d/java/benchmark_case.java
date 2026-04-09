final class MatrixAdd2dCase {
    static final String NAME = "matrix_add_2d";
    static final String PASS_BANNER = "BENCH_MATRIX_ADD_2D_PASS";

    private MatrixAdd2dCase() {}

    static long run(int scale) {
        return BenchmarkSupport.matrixAdd2d(scale);
    }
}
