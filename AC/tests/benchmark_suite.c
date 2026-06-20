#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Benchmark Suite — C implementation
 * Same algorithms as AC benchmark_suite.ac
 */

long fib(long n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

long sum_arr(long* arr, int len) {
    long total = 0;
    for (int i = 0; i < len; i++) {
        total += arr[i];
    }
    return total;
}

double mean(long* arr, int len) {
    if (len == 0) return 0;
    return (double)sum_arr(arr, len) / len;
}

long distance(long* arr, int len) {
    if (len == 0) return 0;
    long minv = arr[0];
    long maxv = arr[0];
    for (int i = 0; i < len; i++) {
        if (arr[i] < minv) minv = arr[i];
        if (arr[i] > maxv) maxv = arr[i];
    }
    return maxv - minv;
}

double stddev(long* arr, int len) {
    if (len == 0) return 0;
    double m = mean(arr, len);
    double sum_sq = 0;
    for (int i = 0; i < len; i++) {
        double diff = arr[i] - m;
        sum_sq += diff * diff;
    }
    return sum_sq / len;
}

int sieve(int n, int* result) {
    if (n < 2) return 0;

    int* prime = (int*)malloc((n + 1) * sizeof(int));
    for (int i = 0; i <= n; i++) {
        prime[i] = 1;
    }
    prime[0] = prime[1] = 0;

    for (int p = 2; p * p <= n; p++) {
        if (prime[p]) {
            for (int i = p * p; i <= n; i += p) {
                prime[i] = 0;
            }
        }
    }

    int count = 0;
    for (int i = 2; i <= n; i++) {
        if (prime[i]) {
            result[count++] = i;
        }
    }

    free(prime);
    return count;
}

long loop_work() {
    long total = 0;
    for (int i = 0; i < 100; i++) {
        total += i;
    }
    return total;
}

double array_ops() {
    long arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int len = 10;
    long s = sum_arr(arr, len);
    double m = mean(arr, len);
    double std = stddev(arr, len);
    long dist = distance(arr, len);
    return s + m + std + dist;
}

int main() {
    long fib_result = fib(28);
    long loop_result = loop_work();

    int* sieve_primes = (int*)malloc(51 * sizeof(int));
    int sieve_count = sieve(50, sieve_primes);
    free(sieve_primes);

    double array_result = array_ops();

    long checksum = fib_result + loop_result + sieve_count;
    checksum = (long)(checksum + array_result);

    printf("%ld\n", checksum);
    return 0;
}
