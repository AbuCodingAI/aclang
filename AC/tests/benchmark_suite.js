#!/usr/bin/env node

/**
 * Benchmark Suite — JavaScript implementation
 * Same algorithms as AC benchmark_suite.ac
 */

function fib(n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

function sumArr(arr) {
    let total = 0;
    for (let i of arr) {
        total += i;
    }
    return total;
}

function mean(arr) {
    if (arr.length === 0) return 0;
    return sumArr(arr) / arr.length;
}

function distance(arr) {
    if (arr.length === 0) return 0;
    let minv = arr[0];
    let maxv = arr[0];
    for (let i of arr) {
        if (i < minv) minv = i;
        if (i > maxv) maxv = i;
    }
    return maxv - minv;
}

function stddev(arr) {
    if (arr.length === 0) return 0;
    const m = mean(arr);
    let sumSq = 0;
    for (let i of arr) {
        const diff = i - m;
        sumSq += diff * diff;
    }
    return sumSq / arr.length;
}

function sieve(n) {
    if (n < 2) return [];

    const prime = Array(n + 1).fill(true);
    prime[0] = prime[1] = false;

    for (let p = 2; p * p <= n; p++) {
        if (prime[p]) {
            for (let i = p * p; i <= n; i += p) {
                prime[i] = false;
            }
        }
    }

    const result = [];
    for (let i = 2; i <= n; i++) {
        if (prime[i]) result.push(i);
    }
    return result;
}

function loopWork() {
    let total = 0;
    for (let i = 0; i < 100; i++) {
        total += i;
    }
    return total;
}

function arrayOps() {
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    const s = sumArr(arr);
    const m = mean(arr);
    const std = stddev(arr);
    const dist = distance(arr);
    return s + m + std + dist;
}

function main() {
    const fibResult = fib(28);
    const loopResult = loopWork();
    const sieveResult = sieve(50).length;  // Count of primes
    const arrayResult = arrayOps();

    const checksum = fibResult + loopResult + sieveResult + arrayResult;
    console.log(checksum);
}

main();
