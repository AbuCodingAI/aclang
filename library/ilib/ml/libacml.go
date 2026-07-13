// libacml.go — Go FFI bindings for AC Machine Learning Library
// Used by AC->GO backend
// Build with: go build -ldflags "-L. -lacml"

package acml

// #cgo LDFLAGS: -lacml
// #include "libacml.h"
import "C"
import (
	"fmt"
)

// ============================================================================
// TENSOR TYPE & WRAPPER
// ============================================================================

// Tensor represents a machine learning tensor
type Tensor struct {
	id C.int64_t
}

// Shape returns the tensor's dimensions
func (t *Tensor) Shape() []int64 {
	shape := make([]C.int64_t, 8)
	len := C.pt_tensor_shape(t.id, &shape[0], 8)
	result := make([]int64, len)
	for i := 0; i < int(len); i++ {
		result[i] = int64(shape[i])
	}
	return result
}

// ToSlice converts tensor to Go slice
func (t *Tensor) ToSlice() []float64 {
	shape := t.Shape()
	size := int64(1)
	for _, dim := range shape {
		size *= dim
	}
	data := make([]C.double, size)
	C.pt_tensor_to_array(t.id, &data[0], C.int64_t(size))
	result := make([]float64, size)
	for i := 0; i < int(size); i++ {
		result[i] = float64(data[i])
	}
	return result
}

// Free releases tensor memory
func (t *Tensor) Free() {
	C.pt_tensor_free(t.id)
}

// ============================================================================
// TENSOR CREATION
// ============================================================================

// NewTensor creates a tensor from Go slice
func NewTensor(data []float64, shape ...int64) *Tensor {
	cdata := make([]C.double, len(data))
	for i, v := range data {
		cdata[i] = C.double(v)
	}
	cshape := make([]C.int64_t, len(shape))
	for i, s := range shape {
		cshape[i] = C.int64_t(s)
	}
	id := C.pt_tensor_from_data(
		&cdata[0], C.int64_t(len(data)),
		&cshape[0], C.int64_t(len(shape)),
		1, // FLOAT32
	)
	return &Tensor{id}
}

// Randn creates random tensor (normal distribution)
func Randn(shape ...int64) *Tensor {
	cshape := make([]C.int64_t, len(shape))
	for i, s := range shape {
		cshape[i] = C.int64_t(s)
	}
	id := C.pt_randn(&cshape[0], C.int64_t(len(shape)), 1) // FLOAT32
	return &Tensor{id}
}

// Zeros creates zero-filled tensor
func Zeros(shape ...int64) *Tensor {
	cshape := make([]C.int64_t, len(shape))
	for i, s := range shape {
		cshape[i] = C.int64_t(s)
	}
	id := C.pt_zeros(&cshape[0], C.int64_t(len(shape)), 1) // FLOAT32
	return &Tensor{id}
}

// Ones creates one-filled tensor
func Ones(shape ...int64) *Tensor {
	cshape := make([]C.int64_t, len(shape))
	for i, s := range shape {
		cshape[i] = C.int64_t(s)
	}
	id := C.pt_ones(&cshape[0], C.int64_t(len(shape)), 1) // FLOAT32
	return &Tensor{id}
}

// ============================================================================
// TENSOR OPERATIONS
// ============================================================================

// Matmul performs matrix multiplication
func (t *Tensor) Matmul(other *Tensor) *Tensor {
	id := C.pt_matmul(t.id, other.id)
	return &Tensor{id}
}

// Add performs element-wise addition
func (t *Tensor) Add(other *Tensor) *Tensor {
	id := C.pt_add(t.id, other.id)
	return &Tensor{id}
}

// Multiply performs element-wise multiplication
func (t *Tensor) Multiply(other *Tensor) *Tensor {
	id := C.pt_multiply(t.id, other.id)
	return &Tensor{id}
}

// ReLU applies ReLU activation
func (t *Tensor) ReLU() *Tensor {
	id := C.pt_relu(t.id)
	return &Tensor{id}
}

// Softmax applies softmax along dimension
func (t *Tensor) Softmax(dim int64) *Tensor {
	id := C.pt_softmax(t.id, C.int64_t(dim))
	return &Tensor{id}
}

// Reshape reshapes tensor to new shape
func (t *Tensor) Reshape(shape ...int64) *Tensor {
	cshape := make([]C.int64_t, len(shape))
	for i, s := range shape {
		cshape[i] = C.int64_t(s)
	}
	id := C.pt_reshape(t.id, &cshape[0], C.int64_t(len(shape)))
	return &Tensor{id}
}

// ToDevice moves tensor to device (0=CPU, 1=CUDA)
func (t *Tensor) ToDevice(device int64) *Tensor {
	id := C.pt_to_device(t.id, C.int64_t(device))
	return &Tensor{id}
}

// ============================================================================
// GRADIENTS & OPTIMIZATION
// ============================================================================

// Backward computes gradients
func Backward(loss *Tensor) error {
	result := C.ml_backward(loss.id)
	if result != 0 {
		return fmt.Errorf("backward pass failed: %d", result)
	}
	return nil
}

// GetGrad retrieves gradient of tensor
func (t *Tensor) GetGrad() *Tensor {
	id := C.ml_get_grad(t.id)
	return &Tensor{id}
}

// SGDStep performs SGD optimization step
func SGDStep(learningRate float64, tensor *Tensor) error {
	result := C.ml_sgd_step(C.double(learningRate), tensor.id)
	if result != 0 {
		return fmt.Errorf("SGD step failed: %d", result)
	}
	return nil
}

// AdamStep performs Adam optimization step
func AdamStep(learningRate, beta1, beta2, epsilon float64, tensor *Tensor) error {
	result := C.ml_adam_step(
		C.double(learningRate),
		C.double(beta1),
		C.double(beta2),
		C.double(epsilon),
		tensor.id,
	)
	if result != 0 {
		return fmt.Errorf("Adam step failed: %d", result)
	}
	return nil
}

// ============================================================================
// UTILITY
// ============================================================================

// Init initializes the ML library
func Init() error {
	result := C.ml_init()
	if result != 0 {
		return fmt.Errorf("ML init failed: %d", result)
	}
	return nil
}

// Cleanup cleans up ML library
func Cleanup() {
	C.ml_cleanup()
}

// Version returns library version
func Version() string {
	cver := C.ml_version()
	return C.GoString(cver)
}
