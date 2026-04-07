namespace BenchmarkRunner;

internal static class StringBuildCase
{
    public const string Name = "string_build";
    public const string PassBanner = "BENCH_STRING_BUILD_PASS";

    public static long Run(int scale) => BenchmarkSupport.StringBuild(scale);
}
