/**
 * Benchmark Suite — V implementation
 * Same algorithms as AC benchmark_suite.ac
 */

fn fib(n i64) i64 {
	if n < 2 {
		return n
	}
	return fib(n - 1) + fib(n - 2)
}

fn sum_arr(arr []i64) i64 {
	mut total := i64(0)
	for i in arr {
		total += i
	}
	return total
}

fn mean(arr []i64) f64 {
	if arr.len == 0 {
		return 0
	}
	return f64(sum_arr(arr)) / f64(arr.len)
}

fn distance(arr []i64) i64 {
	if arr.len == 0 {
		return 0
	}
	mut minv := arr[0]
	mut maxv := arr[0]
	for i in arr {
		if i < minv {
			minv = i
		}
		if i > maxv {
			maxv = i
		}
	}
	return maxv - minv
}

fn stddev(arr []i64) f64 {
	if arr.len == 0 {
		return 0
	}
	m := mean(arr)
	mut sum_sq := f64(0)
	for i in arr {
		diff := f64(i) - m
		sum_sq += diff * diff
	}
	return sum_sq / f64(arr.len)
}

fn sieve(n int) []int {
	if n < 2 {
		return []int{}
	}

	mut prime := []bool{len: n + 1, init: true}
	prime[0] = false
	prime[1] = false

	for p := 2; p * p <= n; p++ {
		if prime[p] {
			for mut i := p * p; i <= n; i += p {
				prime[i] = false
			}
		}
	}

	mut result := []int{}
	for i := 2; i <= n; i++ {
		if prime[i] {
			result << i
		}
	}

	return result
}

fn loop_work() i64 {
	mut total := i64(0)
	for i := 0; i < 100; i++ {
		total += i64(i)
	}
	return total
}

fn array_ops() f64 {
	arr := [i64(1), 2, 3, 4, 5, 6, 7, 8, 9, 10]
	s := sum_arr(arr)
	m := mean(arr)
	std := stddev(arr)
	dist := distance(arr)
	return f64(s) + m + std + f64(dist)
}

fn main() {
	fib_result := fib(28)
	loop_result := loop_work()
	sieve_result := sieve(50).len
	array_result := array_ops()

	checksum := fib_result + loop_result + i64(sieve_result)
	checksum = i64(f64(checksum) + array_result)

	println(checksum)
}
