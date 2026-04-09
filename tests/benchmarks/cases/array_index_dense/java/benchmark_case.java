final class ArrayIndexDenseCase {
    static final String NAME = "array_index_dense";
    static final String PASS_BANNER = "BENCH_ARRAY_INDEX_DENSE_PASS";

    private ArrayIndexDenseCase() {}

    static long run(int scale) {
        return BenchmarkSupport.arrayIndexDense(scale);
    }
}
