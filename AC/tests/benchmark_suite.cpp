#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * Benchmark Suite — C++ implementation
 * Same algorithms as AC benchmark_suite.ac
 */

long fib(long n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

long sum_arr(const std::vector<long>& arr) {
    long total = 0;
    for (long i : arr) {
        total += i;
    }
    return total;
}

double mean(const std::vector<long>& arr) {
    if (arr.empty()) return 0;
    return static_cast<double>(sum_arr(arr)) / arr.size();
}

long distance(const std::vector<long>& arr) {
    if (arr.empty()) return 0;
    long minv = arr[0];
    long maxv = arr[0];
    for (long i : arr) {
        if (i < minv) minv = i;
        if (i > maxv) maxv = i;
    }
    return maxv - minv;
}

double stddev(const std::vector<long>& arr) {
    if (arr.empty()) return 0;
    double m = mean(arr);
    double sum_sq = 0;
    for (long i : arr) {
        double diff = i - m;
        sum_sq += diff * diff;
    }
    return sum_sq / arr.size();
}

std::vector<int> sieve(int n) {
    std::vector<int> result;
    if (n < 2) return result;

    std::vector<bool> prime(n + 1, true);
    prime[0] = prime[1] = false;

    for (int p = 2; p * p <= n; p++) {
        if (prime[p]) {
            for (int i = p * p; i <= n; i += p) {
                prime[i] = false;
            }
        }
    }

    for (int i = 2; i <= n; i++) {
        if (prime[i]) {
            result.push_back(i);
        }
    }

    return result;
}

long loop_work() {
    long total = 0;
    for (int i = 0; i < 100; i++) {
        total += i;
    }
    return total;
}

double array_ops() {
    std::vector<long> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    long s = sum_arr(arr);
    double m = mean(arr);
    double std = stddev(arr);
    long dist = distance(arr);
    return s + m + std + dist;
}

int main() {
    long fib_result = fib(28);
    long loop_result = loop_work();
    std::vector<int> sieve_result = sieve(50);
    double array_result = array_ops();

    long checksum = fib_result + loop_result + static_cast<long>(sieve_result.size());
    checksum = static_cast<long>(checksum + array_result);

    std::cout << checksum << std::endl;
    return 0;
}
