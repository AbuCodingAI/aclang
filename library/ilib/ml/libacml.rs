// libacml.rs — Rust FFI bindings for AC Machine Learning Library
// Used by AC->RS backend

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_double, c_long};

// ============================================================================
// FFI BINDINGS (call C library)
// ============================================================================

#[link(name = "acml")]
extern "C" {
    // Tensor operations
    pub fn pt_tensor_from_data(
        data: *const c_double,
        data_len: i64,
        shape: *const i64,
        shape_len: i64,
        dtype: u32,
    ) -> i64;

    pub fn pt_randn(shape: *const i64, shape_len: i64, dtype: u32) -> i64;
    pub fn pt_zeros(shape: *const i64, shape_len: i64, dtype: u32) -> i64;
    pub fn pt_ones(shape: *const i64, shape_len: i64, dtype: u32) -> i64;

    pub fn pt_tensor_shape(tensor_id: i64, shape_out: *mut i64, shape_len: i64) -> i64;
    pub fn pt_tensor_dtype(tensor_id: i64) -> u32;
    pub fn pt_tensor_to_array(tensor_id: i64, out: *mut c_double, out_len: i64) -> i64;

    // Operations
    pub fn pt_matmul(a: i64, b: i64) -> i64;
    pub fn pt_add(a: i64, b: i64) -> i64;
    pub fn pt_multiply(a: i64, b: i64) -> i64;
    pub fn pt_relu(x: i64) -> i64;
    pub fn pt_softmax(x: i64, dim: i64) -> i64;
    pub fn pt_reshape(x: i64, new_shape: *const i64, shape_len: i64) -> i64;
    pub fn pt_to_device(x: i64, device: i64) -> i64;

    // Cleanup
    pub fn pt_tensor_free(x: i64);

    // TensorFlow API
    pub fn tf_constant(
        data: *const c_double,
        data_len: i64,
        shape: *const i64,
        shape_len: i64,
        dtype: u32,
    ) -> i64;

    pub fn tf_matmul(a: i64, b: i64) -> i64;
    pub fn tf_nn_relu(x: i64) -> i64;
    pub fn tf_nn_softmax(x: i64, axis: i64) -> i64;

    // Gradients
    pub fn ml_backward(loss: i64) -> i32;
    pub fn ml_get_grad(tensor_id: i64) -> i64;

    // Optimization
    pub fn ml_sgd_step(learning_rate: c_double, tensor_id: i64) -> i32;
    pub fn ml_adam_step(
        learning_rate: c_double,
        beta1: c_double,
        beta2: c_double,
        epsilon: c_double,
        tensor_id: i64,
    ) -> i32;

    // Utility
    pub fn ml_init() -> i32;
    pub fn ml_cleanup();
    pub fn ml_version() -> *const c_char;
}

// ============================================================================
// SAFE RUST WRAPPERS
// ============================================================================

pub struct Tensor {
    id: i64,
}

impl Tensor {
    pub fn from_vec(data: Vec<f64>, shape: Vec<i64>) -> Self {
        unsafe {
            let dtype = 1; // FLOAT32
            let id = pt_tensor_from_data(
                data.as_ptr(),
                data.len() as i64,
                shape.as_ptr(),
                shape.len() as i64,
                dtype,
            );
            Tensor { id }
        }
    }

    pub fn randn(shape: &[i64]) -> Self {
        unsafe {
            let dtype = 1; // FLOAT32
            let id = pt_randn(shape.as_ptr(), shape.len() as i64, dtype);
            Tensor { id }
        }
    }

    pub fn shape(&self) -> Vec<i64> {
        unsafe {
            let mut shape = vec![0i64; 8];
            let len = pt_tensor_shape(self.id, shape.as_mut_ptr(), 8);
            shape.truncate(len as usize);
            shape
        }
    }

    pub fn to_vec(&self) -> Vec<f64> {
        unsafe {
            let shape = self.shape();
            let size: usize = shape.iter().product();
            let mut data = vec![0.0; size];
            pt_tensor_to_array(self.id, data.as_mut_ptr(), size as i64);
            data
        }
    }

    pub fn matmul(&self, other: &Tensor) -> Tensor {
        unsafe {
            let id = pt_matmul(self.id, other.id);
            Tensor { id }
        }
    }

    pub fn add(&self, other: &Tensor) -> Tensor {
        unsafe {
            let id = pt_add(self.id, other.id);
            Tensor { id }
        }
    }

    pub fn relu(&self) -> Tensor {
        unsafe {
            let id = pt_relu(self.id);
            Tensor { id }
        }
    }

    pub fn reshape(&self, shape: &[i64]) -> Tensor {
        unsafe {
            let id = pt_reshape(self.id, shape.as_ptr(), shape.len() as i64);
            Tensor { id }
        }
    }
}

impl Drop for Tensor {
    fn drop(&mut self) {
        unsafe { pt_tensor_free(self.id) }
    }
}

pub fn init() {
    unsafe { ml_init() };
}

pub fn cleanup() {
    unsafe { ml_cleanup() };
}

pub fn version() -> String {
    unsafe {
        let cstr = CStr::from_ptr(ml_version());
        cstr.to_string_lossy().to_string()
    }
}
