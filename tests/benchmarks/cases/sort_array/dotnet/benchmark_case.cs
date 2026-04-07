namespace BenchmarkRunner;

internal static class SortArrayCase
{
    public const string Name = "sort_array";
    public const string PassBanner = "BENCH_SORT_ARRAY_PASS";

    public static long Run(int scale) => BenchmarkSupport.SortArray(scale);
}
