namespace BenchmarkRunner;

internal static class MapObjectAccessCase
{
    public const string Name = "map_object_access";
    public const string PassBanner = "BENCH_MAP_OBJECT_ACCESS_PASS";

    public static long Run(int scale) => BenchmarkSupport.MapObjectAccess(scale);
}
