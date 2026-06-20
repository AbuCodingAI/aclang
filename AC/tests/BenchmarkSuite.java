/**
 * Benchmark Suite — Java implementation
 * Same algorithms as AC benchmark_suite.ac
 */

public class BenchmarkSuite {

    static long fib(long n) {
        if (n < 2) return n;
        return fib(n - 1) + fib(n - 2);
    }

    static long sumArr(long[] arr) {
        long total = 0;
        for (long i : arr) {
            total += i;
        }
        return total;
    }

    static double mean(long[] arr) {
        if (arr.length == 0) return 0;
        return (double) sumArr(arr) / arr.length;
    }

    static long distance(long[] arr) {
        if (arr.length == 0) return 0;
        long minv = arr[0];
        long maxv = arr[0];
        for (long i : arr) {
            if (i < minv) minv = i;
            if (i > maxv) maxv = i;
        }
        return maxv - minv;
    }

    static double stddev(long[] arr) {
        if (arr.length == 0) return 0;
        double m = mean(arr);
        double sumSq = 0;
        for (long i : arr) {
            double diff = i - m;
            sumSq += diff * diff;
        }
        return sumSq / arr.length;
    }

    static java.util.List<Integer> sieve(int n) {
        java.util.List<Integer> result = new java.util.ArrayList<>();
        if (n < 2) return result;

        boolean[] prime = new boolean[n + 1];
        for (int i = 0; i <= n; i++) {
            prime[i] = true;
        }
        prime[0] = false;
        prime[1] = false;

        for (int p = 2; p * p <= n; p++) {
            if (prime[p]) {
                for (int i = p * p; i <= n; i += p) {
                    prime[i] = false;
                }
            }
        }

        for (int i = 2; i <= n; i++) {
            if (prime[i]) {
                result.add(i);
            }
        }

        return result;
    }

    static long loopWork() {
        long total = 0;
        for (int i = 0; i < 100; i++) {
            total += i;
        }
        return total;
    }

    static double arrayOps() {
        long[] arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        long s = sumArr(arr);
        double m = mean(arr);
        double std = stddev(arr);
        long dist = distance(arr);
        return s + m + std + dist;
    }

    public static void main(String[] args) {
        long fibResult = fib(28);
        long loopResult = loopWork();
        long sieveResult = sieve(50).size();
        double arrayResult = arrayOps();

        long checksum = fibResult + loopResult + sieveResult;
        checksum = (long) (checksum + arrayResult);

        System.out.println(checksum);
    }
}
