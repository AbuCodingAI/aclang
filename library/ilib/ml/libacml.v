// libacml.v — V language FFI bindings for AC Machine Learning Library
// Used by AC->V backend

module acml

// Data types
pub const (
    dtype_float32 = 1
    dtype_float64 = 2
    dtype_int32   = 3
    dtype_int64   = 4
    dtype_bool    = 5
    dtype_complex = 6
)

pub const (
    device_cpu  = 0
    device_cuda = 1
    device_tpu  = 2
)

// Tensor ID type
pub type TensorID = i64

// ============================================================================
// C FFI DECLARATIONS
// ============================================================================

#flag -lacml
#include "libacml.h"

fn C.pt_tensor_from_data(data &f64, data_len i64, shape &i64, shape_len i64, dtype u32) i64
fn C.pt_randn(shape &i64, shape_len i64, dtype u32) i64
fn C.pt_zeros(shape &i64, shape_len i64, dtype u32) i64
fn C.pt_ones(shape &i64, shape_len i64, dtype u32) i64

fn C.pt_tensor_shape(tensor_id i64, shape_out &i64, shape_len i64) i64
fn C.pt_tensor_dtype(tensor_id i64) u32
fn C.pt_tensor_to_array(tensor_id i64, out &f64, out_len i64) i64

fn C.pt_matmul(a i64, b i64) i64
fn C.pt_add(a i64, b i64) i64
fn C.pt_multiply(a i64, b i64) i64
fn C.pt_relu(x i64) i64
fn C.pt_softmax(x i64, dim i64) i64
fn C.pt_reshape(x i64, new_shape &i64, shape_len i64) i64
fn C.pt_to_device(x i64, device i64) i64
fn C.pt_tensor_free(x i64)

fn C.tf_constant(data &f64, data_len i64, shape &i64, shape_len i64, dtype u32) i64
fn C.tf_matmul(a i64, b i64) i64
fn C.tf_nn_relu(x i64) i64
fn C.tf_nn_softmax(x i64, axis i64) i64

fn C.ml_backward(loss i64) int
fn C.ml_get_grad(tensor_id i64) i64
fn C.ml_sgd_step(learning_rate f64, tensor_id i64) int
fn C.ml_adam_step(learning_rate f64, beta1 f64, beta2 f64, epsilon f64, tensor_id i64) int

fn C.ml_init() int
fn C.ml_cleanup()
fn C.ml_version() &u8

// ============================================================================
// TENSOR STRUCT & METHODS
// ============================================================================

pub struct Tensor {
    id TensorID
}

pub fn tensor_from_array(data []f64, shape []i64) Tensor {
    id := C.pt_tensor_from_data(
        raw_array_data(data),
        data.len(),
        raw_array_data(shape),
        shape.len(),
        dtype_float32
    )
    return Tensor{id}
}

pub fn randn(shape []i64) Tensor {
    id := C.pt_randn(raw_array_data(shape), shape.len(), dtype_float32)
    return Tensor{id}
}

pub fn zeros(shape []i64) Tensor {
    id := C.pt_zeros(raw_array_data(shape), shape.len(), dtype_float32)
    return Tensor{id}
}

pub fn ones(shape []i64) Tensor {
    id := C.pt_ones(raw_array_data(shape), shape.len(), dtype_float32)
    return Tensor{id}
}

pub fn (t Tensor) shape() []i64 {
    mut shape := [i64(0); 8]
    len := C.pt_tensor_shape(t.id, raw_array_data(shape), 8)
    return shape[..len]
}

pub fn (t Tensor) to_array() []f64 {
    shape := t.shape()
    mut size := i64(1)
    for dim in shape {
        size *= dim
    }
    mut data := [f64(0); 0]
    data.grow_len(int(size))
    C.pt_tensor_to_array(t.id, raw_array_data(data), size)
    return data
}

pub fn (t Tensor) matmul(other Tensor) Tensor {
    id := C.pt_matmul(t.id, other.id)
    return Tensor{id}
}

pub fn (t Tensor) add(other Tensor) Tensor {
    id := C.pt_add(t.id, other.id)
    return Tensor{id}
}

pub fn (t Tensor) multiply(other Tensor) Tensor {
    id := C.pt_multiply(t.id, other.id)
    return Tensor{id}
}

pub fn (t Tensor) relu() Tensor {
    id := C.pt_relu(t.id)
    return Tensor{id}
}

pub fn (t Tensor) softmax(dim i64) Tensor {
    id := C.pt_softmax(t.id, dim)
    return Tensor{id}
}

pub fn (t Tensor) reshape(new_shape []i64) Tensor {
    id := C.pt_reshape(t.id, raw_array_data(new_shape), new_shape.len())
    return Tensor{id}
}

pub fn (t Tensor) to_device(device i64) Tensor {
    id := C.pt_to_device(t.id, device)
    return Tensor{id}
}

pub fn (t Tensor) free() {
    C.pt_tensor_free(t.id)
}

// ============================================================================
// GRADIENT & OPTIMIZATION
// ============================================================================

pub fn backward(loss Tensor) ! {
    result := C.ml_backward(loss.id)
    if result != 0 {
        return error('backward pass failed')
    }
}

pub fn (t Tensor) get_grad() Tensor {
    id := C.ml_get_grad(t.id)
    return Tensor{id}
}

pub fn sgd_step(learning_rate f64, tensor Tensor) ! {
    result := C.ml_sgd_step(learning_rate, tensor.id)
    if result != 0 {
        return error('SGD step failed')
    }
}

pub fn adam_step(learning_rate f64, beta1 f64, beta2 f64, epsilon f64, tensor Tensor) ! {
    result := C.ml_adam_step(learning_rate, beta1, beta2, epsilon, tensor.id)
    if result != 0 {
        return error('Adam step failed')
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

pub fn init() ! {
    result := C.ml_init()
    if result != 0 {
        return error('ML init failed')
    }
}

pub fn cleanup() {
    C.ml_cleanup()
}

pub fn version() string {
    cver := C.ml_version()
    return unsafe { cver.vstring() }
}

// Helper function to get raw array pointer
fn raw_array_data(arr []any) &any {
    return unsafe { &arr[0] }
}
