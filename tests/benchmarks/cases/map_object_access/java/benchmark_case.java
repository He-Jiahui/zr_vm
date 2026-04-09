final class MapObjectAccessCase {
    static final String NAME = "map_object_access";
    static final String PASS_BANNER = "BENCH_MAP_OBJECT_ACCESS_PASS";

    private MapObjectAccessCase() {}

    static long run(int scale) {
        return BenchmarkSupport.mapObjectAccess(scale);
    }
}
