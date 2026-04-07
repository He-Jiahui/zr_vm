using System.Collections.Generic;

namespace BenchmarkRunner;

internal readonly record struct BenchmarkCaseDescriptor(string Name, string PassBanner, Func<int, long> Run);

internal static class BenchmarkSupport
{
    public const long Mod = 1_000_000_007L;

    public static int ParseScale(string[] args, out string? error)
    {
        var tier = "core";
        error = null;

        for (var index = 0; index < args.Length; index++)
        {
            if (args[index] == "--tier")
            {
                if (index + 1 >= args.Length)
                {
                    error = "--tier requires smoke, core, or stress";
                    return 0;
                }

                tier = args[++index];
                continue;
            }

            error = $"unknown argument: {args[index]}";
            return 0;
        }

        if (tier == "smoke")
        {
            return 1;
        }

        if (tier == "core")
        {
            return 4;
        }

        if (tier == "stress")
        {
            return 16;
        }

        error = $"unsupported tier: {tier}";
        return 0;
    }

    private static long ModReduce(long value)
    {
        var reduced = value % Mod;
        return reduced < 0 ? reduced + Mod : reduced;
    }

    public static long NumericLoops(int scale)
    {
        var outerLimit = 24 * scale;
        var innerLimit = 3000 * scale;
        long value = 17;
        long checksum = 0;

        for (var outer = 0; outer < outerLimit; outer++)
        {
            for (var inner = 0; inner < innerLimit; inner++)
            {
                value = (value * 1103 + 97 + outer + inner) % 65521;
                if (value % 7 == 0)
                {
                    checksum += value / 7;
                }
                else if (value % 5 == 0)
                {
                    checksum += value % 97;
                }
                else
                {
                    checksum += value % 31;
                }

                checksum = ModReduce(checksum);
            }

            checksum = ModReduce(checksum + outer * 17L + value % 13);
        }

        return checksum;
    }

    private interface IDispatchWorker
    {
        long Step(long delta);
        long Read();
    }

    private sealed class MultiplyWorker(long seed) : IDispatchWorker
    {
        private long _state = seed;

        public long Step(long delta)
        {
            _state = (_state * 13 + delta + 7) % 10007;
            return _state;
        }

        public long Read() => _state;
    }

    private sealed class ScaleWorker(long seed) : IDispatchWorker
    {
        private long _state = seed;

        public long Step(long delta)
        {
            _state = (_state * 17 + delta * 3 + 11) % 10009;
            return _state;
        }

        public long Read() => _state;
    }

    private sealed class XorWorker(long seed) : IDispatchWorker
    {
        private long _state = seed;

        public long Step(long delta)
        {
            _state = ((_state ^ (delta + 31)) + delta * 5 + 19) % 10037;
            return _state;
        }

        public long Read() => _state;
    }

    private sealed class DriftWorker(long seed) : IDispatchWorker
    {
        private long _state = seed;

        public long Step(long delta)
        {
            _state = (_state + delta * delta + 23) % 10039;
            return _state;
        }

        public long Read() => _state;
    }

    private static long Dispatch(IDispatchWorker worker, long delta) => worker.Step(delta);

    public static long DispatchLoops(int scale)
    {
        var workers = new IDispatchWorker[]
        {
            new MultiplyWorker(17),
            new ScaleWorker(29),
            new XorWorker(43),
            new DriftWorker(61)
        };
        var outerLimit = 120 * scale;
        var innerLimit = 320 * scale;
        long checksum = 0;

        for (var outer = 0; outer < outerLimit; outer++)
        {
            for (var inner = 0; inner < innerLimit; inner++)
            {
                var index = (outer + inner) & 3;
                var delta = outer * 7L + inner * 11L + index;
                var value = Dispatch(workers[index], delta);
                checksum = ModReduce(checksum + value * (index + 1L) + delta % 29);
            }

            checksum = ModReduce(checksum + workers[outer & 3].Read() * (outer + 1L));
        }

        return checksum;
    }

    private static string ContainerLabel(long value) =>
        value % 2 == 0 ? "even" : value > 128 ? "odd_hi" : "odd_lo";

    public static long ContainerPipeline(int scale)
    {
        var total = 1024 * scale;
        var queue = new Queue<(string Label, long Value)>();
        var seen = new HashSet<(string Label, long Value)>();
        var buckets = new Dictionary<string, List<long>>(StringComparer.Ordinal);
        long seed = 41;

        for (var index = 0; index < total; index++)
        {
            seed = (seed * 29 + 17 + index) % 257;
            queue.Enqueue((ContainerLabel(seed), seed * scale + (index % 13)));
        }

        while (queue.Count > 0)
        {
            seen.Add(queue.Dequeue());
        }

        foreach (var (label, value) in seen)
        {
            if (!buckets.TryGetValue(label, out var items))
            {
                items = new List<long>();
                buckets[label] = items;
            }

            items.Add(value);
        }

        long oddLoSum = buckets.TryGetValue("odd_lo", out var oddLo) ? oddLo.Sum() : 0;
        long oddHiSum = buckets.TryGetValue("odd_hi", out var oddHi) ? oddHi.Sum() : 0;
        long evenSum = buckets.TryGetValue("even", out var even) ? even.Sum() : 0;
        return ModReduce(evenSum * 100000 + oddHiSum * 100 + oddLoSum + seen.Count);
    }

    private static void InsertionSort(List<long> values)
    {
        for (var index = 1; index < values.Count; index++)
        {
            var key = values[index];
            var cursor = index - 1;
            while (cursor >= 0 && values[cursor] > key)
            {
                values[cursor + 1] = values[cursor];
                cursor--;
            }

            values[cursor + 1] = key;
        }
    }

    private static List<long> BuildSortPattern(int pattern, int length)
    {
        var values = new List<long>(length);
        long seed = 97;

        if (pattern == 0)
        {
            for (var index = 0; index < length; index++)
            {
                seed = (seed * 1103515245 + 12345 + index) % 2147483647;
                values.Add(seed % 100000);
            }

            return values;
        }

        if (pattern == 1)
        {
            for (var index = 0; index < length; index++)
            {
                values.Add(length - index);
            }

            return values;
        }

        if (pattern == 2)
        {
            for (var index = 0; index < length; index++)
            {
                values.Add((index * 17L + 3) % (length / 8L + 5));
            }

            return values;
        }

        for (var index = 0; index < length; index++)
        {
            values.Add(index * 3L + index % 7L);
        }

        for (var index = 0; index < length; index += 7)
        {
            var swapIndex = (index * 13 + 5) % length;
            (values[index], values[swapIndex]) = (values[swapIndex], values[index]);
        }

        return values;
    }

    public static long SortArray(int scale)
    {
        var length = 16 * scale;
        var step = Math.Max(1, length / 7);
        long checksum = 0;

        for (var pattern = 0; pattern < 4; pattern++)
        {
            var values = BuildSortPattern(pattern, length);
            var subtotal = 0L;
            InsertionSort(values);
            for (var cursor = 0; cursor < length; cursor += step)
            {
                subtotal = ModReduce(subtotal + values[cursor] * (cursor + 1L));
            }

            subtotal = ModReduce(
                subtotal
                + values[0] * 3
                + values[length / 2] * 5
                + values[length - 1] * 7
                + pattern * 11L);
            checksum = ModReduce(checksum * 131 + subtotal);
        }

        return checksum;
    }

    public static long PrimeTrialDivision(int scale)
    {
        var limit = 5000 * scale;
        long checksum = 0;
        long count = 0;

        for (var candidate = 2; candidate <= limit; candidate++)
        {
            var isPrime = true;
            for (var divisor = 2; divisor * divisor <= candidate; divisor++)
            {
                if (candidate % divisor == 0)
                {
                    isPrime = false;
                    break;
                }
            }

            if (isPrime)
            {
                count++;
                checksum = ModReduce(checksum + candidate * ((count % 97) + 1));
            }
        }

        return checksum;
    }

    public static long MatrixAdd2d(int scale)
    {
        var rows = 24 * scale;
        var cols = 32 * scale;
        var cells = rows * cols;
        var lhs = new long[cells];
        var rhs = new long[cells];
        var dst = new long[cells];
        var scratch = new long[cells];
        long checksum = 0;

        for (var index = 0; index < cells; index++)
        {
            lhs[index] = (index * 13L + 7) % 997;
            rhs[index] = (index * 17L + 11) % 991;
        }

        for (var row = 0; row < rows; row++)
        {
            long rowSum = 0;
            for (var col = 0; col < cols; col++)
            {
                var index = row * cols + col;
                dst[index] = lhs[index] + rhs[index] + ((row + col) % 7L);
                scratch[index] = dst[index] - lhs[index] / 3 + (rhs[index] % 11);
                rowSum += scratch[index] * (col + 1L);
            }

            checksum = ModReduce(checksum + rowSum * (row + 1L));
        }

        for (var index = 0; index < cells; index++)
        {
            checksum = ModReduce(checksum + scratch[index] * ((index % 17L) + 1));
        }

        return checksum;
    }

    public static long StringBuild(int scale)
    {
        var fragments = new[] { "al", "be", "cy", "do", "ex", "fu" };
        var counts = new Dictionary<string, long>(StringComparer.Ordinal);
        var keys = new List<string>();
        var assembled = string.Empty;
        long assembledScore = 0;
        long checksum = 0;
        long seed = 17;
        var iterations = 180 * scale;

        for (var index = 0; index < iterations; index++)
        {
            seed = (seed * 73 + 19 + index) % 997;
            var tokenId = (seed + index) % 23;
            var token = fragments[seed % fragments.Length]
                + "-"
                + fragments[tokenId % fragments.Length]
                + fragments[(tokenId + 2) % fragments.Length];
            var tokenScore = (seed % 211) + tokenId * 17 + index;
            assembled += token;
            assembledScore = ModReduce(assembledScore * 41 + tokenScore);
            if (index % 4 == 0)
            {
                assembled += "|";
                assembledScore = ModReduce(assembledScore + 3);
            }
            else
            {
                assembled += ":";
                assembledScore = ModReduce(assembledScore + 7);
            }

            if (index % 9 == 8)
            {
                if (!counts.ContainsKey(assembled))
                {
                    counts[assembled] = 0;
                    keys.Add(assembled);
                }

                counts[assembled] = ModReduce(counts[assembled] + assembledScore + index + 1);
                checksum = ModReduce(checksum + counts[assembled] + seed % 97);
                assembled = token;
                assembledScore = ModReduce(tokenScore);
            }
        }

        if (assembled.Length > 0)
        {
            if (!counts.ContainsKey(assembled))
            {
                counts[assembled] = 0;
                keys.Add(assembled);
            }

            counts[assembled] = ModReduce(counts[assembled] + assembledScore + iterations);
        }

        for (var index = 0; index < keys.Count; index++)
        {
            checksum = ModReduce(checksum + counts[keys[index]] * (index + 1L));
        }

        return checksum;
    }

    public static long MapObjectAccess(int scale)
    {
        var labels = new[] { "aa", "bb", "cc", "dd" };
        var buckets = new Dictionary<string, long>(StringComparer.Ordinal);
        long checksum = 0;
        long left = 3;
        long right = 7;
        long hits = 0;

        for (var outer = 0; outer < 64 * scale; outer++)
        {
            for (var inner = 0; inner < 32; inner++)
            {
                left = (left * 31 + outer + inner + hits) % 10007;
                right = (right + left + inner * 3L + 5) % 10009;
                hits += 1;
                var label = labels[(outer + inner) % labels.Length];
                var key = label + "_slot";
                buckets[key] = ModReduce((buckets.TryGetValue(key, out var existing) ? existing : 0) + left + right + hits);
                checksum = ModReduce(checksum + buckets[key] + left + hits);
            }
        }

        return ModReduce(
            checksum
            + buckets["aa_slot"]
            + buckets["bb_slot"]
            + buckets["cc_slot"]
            + buckets["dd_slot"]);
    }
}
