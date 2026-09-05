/*
#cgo LDFLAGS: -L@AC_LIBDIR@ -lacml -Wl,-rpath,@AC_LIBDIR@
#include <stdint.h>
extern int64_t ml_tensor(double value);
extern int64_t ml_grid(int64_t rows, int64_t cols, double fill);
extern int ml_gradient_track(int64_t tensor_id, int enabled);
extern int ml_grad_wipe(int64_t tensor_id);
extern int ml_weights(double learning_rate, int64_t tensor_id);
extern double ml_take(int64_t tensor_id);
extern int64_t ml_add(int64_t a, int64_t b);
extern int64_t ml_multiply(int64_t a, int64_t b);
extern int64_t ml_relu(int64_t x);
extern int ml_backward(int64_t loss);
extern int64_t ml_grad(int64_t tensor_id);
extern int ml_optimize(double learning_rate, int64_t tensor_id);
*/
import "C"

type mlNamespace struct{}

var ml mlNamespace

func (mlNamespace) tensor(value float64) int64 { return int64(C.ml_tensor(C.double(value))) }
func (mlNamespace) grid(rows int64, cols int64, fill float64) int64 {
	return int64(C.ml_grid(C.int64_t(rows), C.int64_t(cols), C.double(fill)))
}
func (mlNamespace) gradient_track(tensorId int64, enabled int64) int64 {
	return int64(C.ml_gradient_track(C.int64_t(tensorId), C.int(enabled)))
}
func (mlNamespace) grad_wipe(tensorId int64) int64 { return int64(C.ml_grad_wipe(C.int64_t(tensorId))) }
func (mlNamespace) weights(learningRate float64, tensorId int64) int64 {
	return int64(C.ml_weights(C.double(learningRate), C.int64_t(tensorId)))
}
func (mlNamespace) take(tensorId int64) float64 { return float64(C.ml_take(C.int64_t(tensorId))) }
func (mlNamespace) add(a int64, b int64) int64 { return int64(C.ml_add(C.int64_t(a), C.int64_t(b))) }
func (mlNamespace) multiply(a int64, b int64) int64 {
	return int64(C.ml_multiply(C.int64_t(a), C.int64_t(b)))
}
func (mlNamespace) relu(x int64) int64 { return int64(C.ml_relu(C.int64_t(x))) }
func (mlNamespace) backward(loss int64) int64 { return int64(C.ml_backward(C.int64_t(loss))) }
func (mlNamespace) grad(tensorId int64) int64 { return int64(C.ml_grad(C.int64_t(tensorId))) }
func (mlNamespace) optimize(learningRate float64, tensorId int64) int64 {
	return int64(C.ml_optimize(C.double(learningRate), C.int64_t(tensorId)))
}
