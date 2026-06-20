/**
 * Benchmark Suite — Rust implementation
 * Same algorithms as AC benchmark_suite.ac
 */

fn fib(n: i64) -> i64 {
    if n < 2 {
        return n;
    }
    fib(n - 1) + fib(n - 2)
}

fn sum_arr(arr: &[i64]) -> i64 {
    let mut total = 0i64;
    for &i in arr {
        total += i;
    }
    total
}

fn mean(arr: &[i64]) -> f64 {
    if arr.is_empty() {
        return 0.0;
    }
    sum_arr(arr) as f64 / arr.len() as f64
}

fn distance(arr: &[i64]) -> i64 {
    if arr.is_empty() {
        return 0;
    }
    let mut minv = arr[0];
    let mut maxv = arr[0];
    for &i in arr {
        if i < minv {
            minv = i;
        }
        if i > maxv {
            maxv = i;
        }
    }
    maxv - minv
}

fn stddev(arr: &[i64]) -> f64 {
    if arr.is_empty() {
        return 0.0;
    }
    let m = mean(arr);
    let mut sum_sq = 0.0;
    for &i in arr {
        let diff = i as f64 - m;
        sum_sq += diff * diff;
    }
    sum_sq / arr.len() as f64
}

fn sieve(n: usize) -> Vec<usize> {
    if n < 2 {
        return vec![];
    }

    let mut prime = vec![true; n + 1];
    prime[0] = false;
    prime[1] = false;

    for p in 2..=n {
        if prime[p] {
            let mut i = p * p;
            while i <= n {
                prime[i] = false;
                i += p;
            }
        }
    }

    let mut result = vec![];
    for i in 2..=n {
        if prime[i] {
            result.push(i);
        }
    }

    result
}

fn loop_work() -> i64 {
    let mut total = 0i64;
    for i in 0..100 {
        total += i;
    }
    total
}

fn array_ops() -> f64 {
    let arr: &[i64] = &[1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    let s = sum_arr(arr);
    let m = mean(arr);
    let std = stddev(arr);
    let dist = distance(arr);
    s as f64 + m + std + dist as f64
}

fn main() {
    let fib_result = fib(28);
    let loop_result = loop_work();
    let sieve_result = sieve(50).len();
    let array_result = array_ops();

    let checksum = fib_result + loop_result as i64 + sieve_result as i64;
    let checksum = checksum as f64 + array_result;

    println!("{}", checksum as i64);
}
