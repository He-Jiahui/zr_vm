import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Objects;

final class BenchmarkSupport {
    static final long MOD = 1_000_000_007L;

    private BenchmarkSupport() {}

    static int parseScale(String[] args) {
        String tier = "core";
        Integer explicitScale = null;

        for (int index = 0; index < args.length; index++) {
            String argument = args[index];
            if ("--tier".equals(argument)) {
                if (index + 1 >= args.length) {
                    throw new IllegalArgumentException("--tier requires smoke, core, stress, or profile");
                }
                tier = args[++index];
                continue;
            }

            if ("--scale".equals(argument)) {
                if (index + 1 >= args.length) {
                    throw new IllegalArgumentException("--scale requires a positive integer");
                }
                try {
                    explicitScale = Integer.parseInt(args[++index]);
                } catch (NumberFormatException ex) {
                    throw new IllegalArgumentException("--scale requires a positive integer");
                }
                if (explicitScale < 1) {
                    throw new IllegalArgumentException("--scale requires a positive integer");
                }
                continue;
            }

            throw new IllegalArgumentException("unknown argument: " + argument);
        }

        if (explicitScale != null) {
            return explicitScale;
        }

        switch (tier) {
            case "smoke":
                return 1;
            case "core":
                return 4;
            case "stress":
                return 16;
            case "profile":
                return 1;
            default:
                throw new IllegalArgumentException("unsupported tier: " + tier);
        }
    }

    private static long modReduce(long value) {
        long reduced = value % MOD;
        return reduced < 0 ? reduced + MOD : reduced;
    }

    static long numericLoops(int scale) {
        int outerLimit = 24 * scale;
        int innerLimit = 3000 * scale;
        long value = 17;
        long checksum = 0;

        for (int outer = 0; outer < outerLimit; outer++) {
            for (int inner = 0; inner < innerLimit; inner++) {
                value = (value * 1103 + 97 + outer + inner) % 65521;
                if (value % 7 == 0) {
                    checksum += value / 7;
                } else if (value % 5 == 0) {
                    checksum += value % 97;
                } else {
                    checksum += value % 31;
                }
                checksum = modReduce(checksum);
            }

            checksum = modReduce(checksum + outer * 17L + value % 13);
        }

        return checksum;
    }

    private interface DispatchWorker {
        long step(long delta);
        long read();
    }

    private static final class MultiplyWorker implements DispatchWorker {
        private long state;

        private MultiplyWorker(long seed) {
            state = seed;
        }

        public long step(long delta) {
            state = (state * 13 + delta + 7) % 10007;
            return state;
        }

        public long read() {
            return state;
        }
    }

    private static final class ScaleWorker implements DispatchWorker {
        private long state;

        private ScaleWorker(long seed) {
            state = seed;
        }

        public long step(long delta) {
            state = (state * 17 + delta * 3 + 11) % 10009;
            return state;
        }

        public long read() {
            return state;
        }
    }

    private static final class XorWorker implements DispatchWorker {
        private long state;

        private XorWorker(long seed) {
            state = seed;
        }

        public long step(long delta) {
            state = ((state ^ (delta + 31)) + delta * 5 + 19) % 10037;
            return state;
        }

        public long read() {
            return state;
        }
    }

    private static final class DriftWorker implements DispatchWorker {
        private long state;

        private DriftWorker(long seed) {
            state = seed;
        }

        public long step(long delta) {
            state = (state + delta * delta + 23) % 10039;
            return state;
        }

        public long read() {
            return state;
        }
    }

    private static long dispatch(DispatchWorker worker, long delta) {
        return worker.step(delta);
    }

    static long dispatchLoops(int scale) {
        DispatchWorker[] workers = new DispatchWorker[] {
                new MultiplyWorker(17),
                new ScaleWorker(29),
                new XorWorker(43),
                new DriftWorker(61)
        };
        int outerLimit = 120 * scale;
        int innerLimit = 320 * scale;
        long checksum = 0;

        for (int outer = 0; outer < outerLimit; outer++) {
            for (int inner = 0; inner < innerLimit; inner++) {
                int index = (outer + inner) & 3;
                long delta = outer * 7L + inner * 11L + index;
                long value = dispatch(workers[index], delta);
                checksum = modReduce(checksum + value * (index + 1L) + delta % 29);
            }

            checksum = modReduce(checksum + workers[outer & 3].read() * (outer + 1L));
        }

        return checksum;
    }

    private static String containerLabel(long value) {
        if (value % 2 == 0) {
            return "even";
        }
        if (value > 128) {
            return "odd_hi";
        }
        return "odd_lo";
    }

    private static final class ContainerEntry {
        private final String label;
        private final long value;

        private ContainerEntry(String label, long value) {
            this.label = label;
            this.value = value;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) {
                return true;
            }
            if (!(other instanceof ContainerEntry)) {
                return false;
            }
            ContainerEntry entry = (ContainerEntry) other;
            return value == entry.value && Objects.equals(label, entry.label);
        }

        @Override
        public int hashCode() {
            return Objects.hash(label, value);
        }
    }

    static long containerPipeline(int scale) {
        int total = 1024 * scale;
        ArrayDeque<ContainerEntry> queue = new ArrayDeque<>();
        HashSet<ContainerEntry> seen = new HashSet<>();
        HashMap<String, ArrayList<Long>> buckets = new HashMap<>();
        long seed = 41;

        for (int index = 0; index < total; index++) {
            seed = (seed * 29 + 17 + index) % 257;
            queue.addLast(new ContainerEntry(containerLabel(seed), seed * scale + (index % 13)));
        }

        while (!queue.isEmpty()) {
            seen.add(queue.removeFirst());
        }

        for (ContainerEntry entry : seen) {
            ArrayList<Long> items = buckets.get(entry.label);
            if (items == null) {
                items = new ArrayList<>();
                buckets.put(entry.label, items);
            }
            items.add(entry.value);
        }

        long oddLoSum = sumBucket(buckets.get("odd_lo"));
        long oddHiSum = sumBucket(buckets.get("odd_hi"));
        long evenSum = sumBucket(buckets.get("even"));
        return modReduce(evenSum * 100000 + oddHiSum * 100 + oddLoSum + seen.size());
    }

    private static long sumBucket(ArrayList<Long> bucket) {
        long total = 0;
        if (bucket == null) {
            return 0;
        }
        for (Long value : bucket) {
            total += value;
        }
        return total;
    }

    private static void insertionSort(long[] values) {
        for (int index = 1; index < values.length; index++) {
            long key = values[index];
            int cursor = index - 1;
            while (cursor >= 0 && values[cursor] > key) {
                values[cursor + 1] = values[cursor];
                cursor--;
            }
            values[cursor + 1] = key;
        }
    }

    private static long[] buildSortPattern(int pattern, int length) {
        long[] values = new long[length];
        long seed = 97;

        if (pattern == 0) {
            for (int index = 0; index < length; index++) {
                seed = (seed * 1103515245 + 12345 + index) % 2147483647;
                values[index] = seed % 100000;
            }
            return values;
        }

        if (pattern == 1) {
            for (int index = 0; index < length; index++) {
                values[index] = length - index;
            }
            return values;
        }

        if (pattern == 2) {
            for (int index = 0; index < length; index++) {
                values[index] = (index * 17L + 3) % (length / 8L + 5);
            }
            return values;
        }

        for (int index = 0; index < length; index++) {
            values[index] = index * 3L + index % 7L;
        }
        for (int index = 0; index < length; index += 7) {
            int swapIndex = (index * 13 + 5) % length;
            long temp = values[index];
            values[index] = values[swapIndex];
            values[swapIndex] = temp;
        }
        return values;
    }

    static long sortArray(int scale) {
        int length = 16 * scale;
        int step = Math.max(1, length / 7);
        long checksum = 0;

        for (int pattern = 0; pattern < 4; pattern++) {
            long[] values = buildSortPattern(pattern, length);
            long subtotal = 0;
            insertionSort(values);
            for (int cursor = 0; cursor < length; cursor += step) {
                subtotal = modReduce(subtotal + values[cursor] * (cursor + 1L));
            }

            subtotal = modReduce(
                    subtotal
                            + values[0] * 3
                            + values[length / 2] * 5
                            + values[length - 1] * 7
                            + pattern * 11L);
            checksum = modReduce(checksum * 131 + subtotal);
        }

        return checksum;
    }

    static long primeTrialDivision(int scale) {
        int limit = 5000 * scale;
        long checksum = 0;
        long count = 0;

        for (int candidate = 2; candidate <= limit; candidate++) {
            boolean isPrime = true;
            for (int divisor = 2; divisor * divisor <= candidate; divisor++) {
                if (candidate % divisor == 0) {
                    isPrime = false;
                    break;
                }
            }

            if (isPrime) {
                count++;
                checksum = modReduce(checksum + candidate * ((count % 97) + 1));
            }
        }

        return checksum;
    }

    static long matrixAdd2d(int scale) {
        int rows = 24 * scale;
        int cols = 32 * scale;
        int cells = rows * cols;
        long[] lhs = new long[cells];
        long[] rhs = new long[cells];
        long[] dst = new long[cells];
        long[] scratch = new long[cells];
        long checksum = 0;

        for (int index = 0; index < cells; index++) {
            lhs[index] = (index * 13L + 7) % 997;
            rhs[index] = (index * 17L + 11) % 991;
        }

        for (int row = 0; row < rows; row++) {
            long rowSum = 0;
            for (int col = 0; col < cols; col++) {
                int index = row * cols + col;
                dst[index] = lhs[index] + rhs[index] + ((row + col) % 7L);
                scratch[index] = dst[index] - lhs[index] / 3 + (rhs[index] % 11);
                rowSum += scratch[index] * (col + 1L);
            }
            checksum = modReduce(checksum + rowSum * (row + 1L));
        }

        for (int index = 0; index < cells; index++) {
            checksum = modReduce(checksum + scratch[index] * ((index % 17L) + 1));
        }

        return checksum;
    }

    static long stringBuild(int scale) {
        String[] fragments = new String[] {"al", "be", "cy", "do", "ex", "fu"};
        HashMap<String, Long> counts = new HashMap<>();
        ArrayList<String> keys = new ArrayList<>();
        StringBuilder assembled = new StringBuilder();
        long assembledScore = 0;
        long checksum = 0;
        long seed = 17;
        int iterations = 180 * scale;

        for (int index = 0; index < iterations; index++) {
            seed = (seed * 73 + 19 + index) % 997;
            long tokenId = (seed + index) % 23;
            String token = fragments[(int) (seed % fragments.length)]
                    + "-"
                    + fragments[(int) (tokenId % fragments.length)]
                    + fragments[(int) ((tokenId + 2) % fragments.length)];
            long tokenScore = (seed % 211) + tokenId * 17 + index;

            assembled.append(token);
            assembledScore = modReduce(assembledScore * 41 + tokenScore);
            if (index % 4 == 0) {
                assembled.append('|');
                assembledScore = modReduce(assembledScore + 3);
            } else {
                assembled.append(':');
                assembledScore = modReduce(assembledScore + 7);
            }

            if (index % 9 == 8) {
                String key = assembled.toString();
                if (!counts.containsKey(key)) {
                    counts.put(key, 0L);
                    keys.add(key);
                }

                counts.put(key, modReduce(counts.get(key) + assembledScore + index + 1L));
                checksum = modReduce(checksum + counts.get(key) + seed % 97);
                assembled.setLength(0);
                assembled.append(token);
                assembledScore = modReduce(tokenScore);
            }
        }

        if (assembled.length() > 0) {
            String key = assembled.toString();
            if (!counts.containsKey(key)) {
                counts.put(key, 0L);
                keys.add(key);
            }

            counts.put(key, modReduce(counts.get(key) + assembledScore + iterations));
        }

        for (int index = 0; index < keys.size(); index++) {
            checksum = modReduce(checksum + counts.get(keys.get(index)) * (index + 1L));
        }

        return checksum;
    }

    static long mapObjectAccess(int scale) {
        String[] labels = new String[] {"aa", "bb", "cc", "dd"};
        HashMap<String, Long> buckets = new HashMap<>();
        long checksum = 0;
        long left = 3;
        long right = 7;
        long hits = 0;

        for (int outer = 0; outer < 64 * scale; outer++) {
            for (int inner = 0; inner < 32; inner++) {
                left = (left * 31 + outer + inner + hits) % 10007;
                right = (right + left + inner * 3L + 5) % 10009;
                hits += 1;
                String label = labels[(outer + inner) % labels.length];
                String key = label + "_slot";
                long updated = modReduce(buckets.getOrDefault(key, 0L) + left + right + hits);
                buckets.put(key, updated);
                checksum = modReduce(checksum + updated + left + hits);
            }
        }

        return modReduce(
                checksum
                        + buckets.getOrDefault("aa_slot", 0L)
                        + buckets.getOrDefault("bb_slot", 0L)
                        + buckets.getOrDefault("cc_slot", 0L)
                        + buckets.getOrDefault("dd_slot", 0L));
    }

    static long fibRecursive(int scale) {
        int rounds = 18 * scale;
        long checksum = 0;

        for (int index = 0; index < rounds; index++) {
            int n = 13 + (index % 6);
            long value = fibRecursiveValue(n);
            checksum = modReduce(checksum * 131 + value * (index + 3L) + n);
        }

        return checksum;
    }

    private static long fibRecursiveValue(int n) {
        if (n <= 1) {
            return n;
        }
        return fibRecursiveValue(n - 1) + fibRecursiveValue(n - 2);
    }

    private interface PolyCallable {
        long apply(long value, long delta);
    }

    private static final class PolyAdder implements PolyCallable {
        private final long base;

        private PolyAdder(long base) {
            this.base = base;
        }

        public long apply(long value, long delta) {
            return (value + delta + base) % 100003;
        }
    }

    private static final class PolyMultiply implements PolyCallable {
        private final long factor;

        private PolyMultiply(long factor) {
            this.factor = factor;
        }

        public long apply(long value, long delta) {
            return (value * (factor + 3) + delta + 7) % 100003;
        }
    }

    private static final class PolyXor implements PolyCallable {
        private final long mask;

        private PolyXor(long mask) {
            this.mask = mask;
        }

        public long apply(long value, long delta) {
            return ((value ^ (delta + mask)) + mask * 5 + delta) % 100003;
        }
    }

    static long callChainPolymorphic(int scale) {
        int rounds = 320 * scale;
        PolyCallable adder = new PolyAdder(17);
        PolyCallable multiply = new PolyMultiply(23);
        PolyCallable xorCall = new PolyXor(31);
        long state = 17;
        long checksum = 0;

        for (int outer = 0; outer < rounds; outer++) {
            long delta = outer * 7L + (state % 13);
            int selector = outer % 3;
            if (selector == 0) {
                state = dispatchCallable(adder, callChainA(state, delta), delta);
            } else if (selector == 1) {
                state = dispatchCallable(multiply, callChainB(state, delta), delta);
            } else {
                state = dispatchCallable(xorCall, callChainC(state, delta), delta);
            }

            checksum = modReduce(checksum + state * (selector + 1L) + tailAccumulate((outer % 5) + 1, state));
        }

        return checksum;
    }

    private static long callLeaf(long value, long salt) {
        return (value * 17 + salt * 13 + 19) % 100003;
    }

    private static long callChainA(long value, long salt) {
        return callLeaf(value + 3, salt + 1);
    }

    private static long callChainB(long value, long salt) {
        return callLeaf(callChainA(value + (salt % 5), salt + 7), salt + 11);
    }

    private static long callChainC(long value, long salt) {
        return callLeaf(callChainB(value ^ salt, salt + 13), salt + 17);
    }

    private static long tailAccumulate(int steps, long acc) {
        if (steps == 0) {
            return acc;
        }
        return tailAccumulate(steps - 1, (acc * 3 + steps + 5) % 100003);
    }

    private static long dispatchCallable(PolyCallable callable, long value, long delta) {
        return callable.apply(value, delta);
    }

    private static final class HotRecord {
        private long a;
        private long b;
        private long c;
        private long d;

        private HotRecord(long seed) {
            a = seed;
            b = seed + 3;
            c = seed + 7;
            d = seed + 11;
        }
    }

    static long objectFieldHot(int scale) {
        int rounds = 12000 * scale;
        HotRecord record = new HotRecord(5);
        long checksum = 0;

        for (int index = 0; index < rounds; index++) {
            record.a = (record.a + record.b + index) % 10007;
            record.b = (record.b + record.c + record.a + 3) % 10009;
            record.c = (record.c + record.d + record.b + (index % 7L)) % 10037;
            record.d = (record.d + record.a + record.c + 5) % 10039;
            long snapshot = record.a * 3 + record.b * 5 + record.c * 7 + record.d * 11;
            if ((snapshot & 1L) == 0) {
                checksum = modReduce(checksum + snapshot + record.b);
            } else {
                checksum = modReduce(checksum + snapshot + record.c);
            }
        }

        return checksum;
    }

    static long arrayIndexDense(int scale) {
        int length = 128 * scale;
        int rounds = 48 * scale;
        long[] values = new long[length];
        long checksum = 0;

        for (int index = 0; index < length; index++) {
            values[index] = (index * 13L + 7) % 997;
        }

        for (int roundIndex = 0; roundIndex < rounds; roundIndex++) {
            for (int cursor = 1; cursor < length - 1; cursor++) {
                long left = values[cursor - 1];
                long mid = values[cursor];
                long right = values[cursor + 1];
                long updated = (left + mid * 3 + right * 5 + roundIndex + cursor) % 1000003;
                values[cursor] = updated;
                checksum = modReduce(checksum + updated * (cursor + 1L));
            }

            checksum = modReduce(checksum + values[0] + values[length - 1] + roundIndex);
        }

        return checksum;
    }

    static long branchJumpDense(int scale) {
        int outerLimit = 180 * scale;
        int innerLimit = 180;
        long state = 23;
        long checksum = 0;

        for (int outer = 0; outer < outerLimit; outer++) {
            for (int inner = 0; inner < innerLimit; inner++) {
                state = (state * 97 + outer * 13L + inner * 17L + 19) % 65521;
                if (state % 11 == 0) {
                    checksum += state / 11 + outer;
                } else if (state % 7 == 0) {
                    checksum += state % 97 + inner * 3L;
                } else if (state % 5 == 0) {
                    checksum += (state / 5) % 89 + outer * 5L;
                } else if (state % 3 == 0) {
                    checksum += (state ^ (outer + inner)) + 17;
                } else {
                    checksum += state % 31 + outer + inner;
                }

                checksum = modReduce(checksum);
                if ((checksum & 1L) == 0) {
                    checksum = modReduce(checksum + state % 19);
                } else {
                    checksum = modReduce(checksum + state % 23);
                }
            }
        }

        return checksum;
    }

    private static final class Service {
        private final long weight;
        private long bias;

        private Service(long weight, long bias) {
            this.weight = weight;
            this.bias = bias;
        }

        private long handle(long value, long ticket) {
            bias = (bias + ticket + weight) % 10007;
            if ((ticket + weight) % 2 == 0) {
                return (value * weight + bias + ticket) % 1000003;
            }
            return (value + weight * 7 + bias + ticket * 3) % 1000003;
        }
    }

    static long mixedServiceLoop(int scale) {
        int length = 24 * scale;
        int rounds = 320 * scale;
        long[] counters = new long[length];
        Service service0 = new Service(3, 11);
        Service service1 = new Service(5, 17);
        Service service2 = new Service(7, 23);
        long checksum = 0;
        long state = 31;

        for (int index = 0; index < length; index++) {
            counters[index] = (index * 19L + 5) % 257;
        }

        for (int outer = 0; outer < rounds; outer++) {
            for (int inner = 0; inner < 32; inner++) {
                int slot = (int) ((outer + inner + state) % length);
                long current = counters[slot];
                int selector = slot % 3;
                long ticket = outer * 11L + inner * 7L + selector;

                if (selector == 0) {
                    state = routeService(service0, current + state, ticket);
                } else if (selector == 1) {
                    state = routeService(service1, current + state, ticket);
                } else {
                    state = routeService(service2, current + state, ticket);
                }

                counters[slot] = (current + state + selector + inner) % 1000003;
                if (counters[slot] % 4 == 0) {
                    checksum = modReduce(checksum + counters[slot] + state + current);
                } else {
                    checksum = modReduce(checksum + counters[slot] * (selector + 1L) + state);
                }
            }
        }

        return modReduce(
                checksum
                        + service0.bias
                        + service1.bias
                        + service2.bias
                        + counters[0]
                        + counters[length / 2]
                        + counters[length - 1]);
    }

    private static long routeService(Service service, long value, long ticket) {
        return service.handle(value, ticket);
    }
}
