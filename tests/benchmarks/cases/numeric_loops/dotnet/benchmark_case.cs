namespace BenchmarkRunner;

internal static class NumericLoopsCase
{
    public const string Name = "numeric_loops";
    public const string PassBanner = "BENCH_NUMERIC_LOOPS_PASS";

    public static long Run(int scale) => BenchmarkSupport.NumericLoops(scale);
}
