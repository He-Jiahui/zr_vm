final class ObjectFieldHotCase {
    static final String NAME = "object_field_hot";
    static final String PASS_BANNER = "BENCH_OBJECT_FIELD_HOT_PASS";

    private ObjectFieldHotCase() {}

    static long run(int scale) {
        return BenchmarkSupport.objectFieldHot(scale);
    }
}
