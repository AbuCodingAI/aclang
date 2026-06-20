#!/usr/bin/env python3
"""
Benchmark Suite — Python implementation
Same algorithms as AC benchmark_suite.ac
"""

def fib(n):
    """Fibonacci(n) - recursive"""
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

def sum_arr(arr):
    """Sum array elements"""
    total = 0
    for i in arr:
        total += i
    return total

def mean(arr):
    """Mean of array"""
    s = sum_arr(arr)
    count = len(arr)
    return s / count if count > 0 else 0

def distance(arr):
    """Max - Min (range)"""
    if not arr:
        return 0
    minv = arr[0]
    maxv = arr[0]
    for i in arr:
        if i < minv:
            minv = i
        if i > maxv:
            maxv = i
    return maxv - minv

def stddev(arr):
    """Standard deviation (variance)"""
    if not arr:
        return 0
    m = mean(arr)
    sum_sq = 0
    for i in arr:
        diff = i - m
        sum_sq += diff * diff
    return sum_sq / len(arr) if arr else 0

def sieve(n):
    """Sieve of Eratosthenes - return list of primes up to n"""
    if n < 2:
        return []

    prime = [True] * (n + 1)
    prime[0] = prime[1] = False

    p = 2
    while p * p <= n:
        if prime[p]:
            for i in range(p * p, n + 1, p):
                prime[i] = False
        p += 1

    return [i for i in range(2, n + 1) if prime[i]]

def loop_work():
    """Sum 0-99"""
    total = 0
    for i in range(100):
        total += i
    return total

def array_ops():
    """Array operations"""
    arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    s = sum_arr(arr)
    m = mean(arr)
    std = stddev(arr)
    dist = distance(arr)
    return s + m + std + dist

def main():
    """Run all benchmarks"""
    fib_result = fib(28)
    loop_result = loop_work()
    sieve_result = len(sieve(50))  # Count of primes
    array_result = array_ops()

    checksum = fib_result + loop_result + sieve_result + array_result
    print(checksum)

if __name__ == '__main__':
    main()
