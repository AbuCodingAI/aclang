package main

import (
	"fmt"
)

/**
 * Benchmark Suite — Go implementation
 * Same algorithms as AC benchmark_suite.ac
 */

func fib(n int64) int64 {
	if n < 2 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func sumArr(arr []int64) int64 {
	total := int64(0)
	for _, i := range arr {
		total += i
	}
	return total
}

func mean(arr []int64) float64 {
	if len(arr) == 0 {
		return 0
	}
	return float64(sumArr(arr)) / float64(len(arr))
}

func distance(arr []int64) int64 {
	if len(arr) == 0 {
		return 0
	}
	minv := arr[0]
	maxv := arr[0]
	for _, i := range arr {
		if i < minv {
			minv = i
		}
		if i > maxv {
			maxv = i
		}
	}
	return maxv - minv
}

func stddev(arr []int64) float64 {
	if len(arr) == 0 {
		return 0
	}
	m := mean(arr)
	sumSq := 0.0
	for _, i := range arr {
		diff := float64(i) - m
		sumSq += diff * diff
	}
	return sumSq / float64(len(arr))
}

func sieve(n int) []int {
	if n < 2 {
		return []int{}
	}

	prime := make([]bool, n+1)
	for i := 0; i <= n; i++ {
		prime[i] = true
	}
	prime[0] = false
	prime[1] = false

	for p := 2; p*p <= n; p++ {
		if prime[p] {
			for i := p * p; i <= n; i += p {
				prime[i] = false
			}
		}
	}

	result := []int{}
	for i := 2; i <= n; i++ {
		if prime[i] {
			result = append(result, i)
		}
	}

	return result
}

func loopWork() int64 {
	total := int64(0)
	for i := 0; i < 100; i++ {
		total += int64(i)
	}
	return total
}

func arrayOps() float64 {
	arr := []int64{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
	s := sumArr(arr)
	m := mean(arr)
	std := stddev(arr)
	dist := distance(arr)
	return float64(s) + m + std + float64(dist)
}

func main() {
	fibResult := fib(28)
	loopResult := loopWork()
	sieveResult := len(sieve(50))
	arrayResult := arrayOps()

	checksum := fibResult + int64(loopResult) + int64(sieveResult)
	checksum = int64(float64(checksum) + arrayResult)

	fmt.Println(checksum)
}
