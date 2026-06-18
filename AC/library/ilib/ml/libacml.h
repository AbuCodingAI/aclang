/*
libacml.h — AC Machine Learning Library FFI (C Header)

Provides unified interface for TensorFlow + PyTorch across all AC backends.

FFI targets:
- C (AC->C backend)
- C++ (AC->C++ backend)
- Rust (AC->RS via FFI)
- Go (AC->GO via cgo)
- Java (AC->Java via JNI)
- V (AC->V via C interop)
- Direct linking (AC->BNY, AI->VM)
*/

#ifndef LIBACML_H
#define LIBACML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
   TENSOR TYPES & STRUCTURES
   ============================================================================ */

typedef int64_t tensor_id_t;
typedef int64_t device_t;

#define DEVICE_CPU    0
#define DEVICE_CUDA   1
#define DEVICE_TPU    2

/* Tensor data types */
typedef enum {
    DT_FLOAT32  = 1,
    DT_FLOAT64  = 2,
    DT_INT32    = 3,
    DT_INT64    = 4,
    DT_BOOL     = 5,
    DT_COMPLEX  = 6,
} dtype_t;

/* ============================================================================
   TENSOR OPERATIONS (PyTorch-style)
   ============================================================================ */

/* Create tensor from data array
   Args:
     data: flat array of values
     data_len: number of elements
     shape: dimension array
     shape_len: number of dimensions
     dtype: data type
   Returns:
     tensor_id > 0 on success, < 0 on error
*/
tensor_id_t pt_tensor_from_data(
    const double* data, int64_t data_len,
    const int64_t* shape, int64_t shape_len,
    dtype_t dtype
);

/* Create random tensor (normal distribution N(0,1))
   Args:
     shape: dimension array
     shape_len: number of dimensions
     dtype: data type
   Returns:
     tensor_id
*/
tensor_id_t pt_randn(
    const int64_t* shape, int64_t shape_len,
    dtype_t dtype
);

/* Create zero tensor */
tensor_id_t pt_zeros(
    const int64_t* shape, int64_t shape_len,
    dtype_t dtype
);

/* Create one tensor */
tensor_id_t pt_ones(
    const int64_t* shape, int64_t shape_len,
    dtype_t dtype
);

/* Get tensor shape
   Args:
     tensor_id: tensor identifier
     shape_out: output buffer for shape
     shape_len: max size of shape_out
   Returns:
     actual shape length
*/
int64_t pt_tensor_shape(
    tensor_id_t tensor_id,
    int64_t* shape_out,
    int64_t shape_len
);

/* Get tensor data type */
dtype_t pt_tensor_dtype(tensor_id_t tensor_id);

/* Convert tensor to flat array
   Args:
     tensor_id: tensor identifier
     out: output buffer
     out_len: buffer size
   Returns:
     number of elements copied
*/
int64_t pt_tensor_to_array(
    tensor_id_t tensor_id,
    double* out,
    int64_t out_len
);

/* Matrix multiplication: C = A @ B
   Args:
     a, b: tensor ids
   Returns:
     result tensor_id
*/
tensor_id_t pt_matmul(tensor_id_t a, tensor_id_t b);

/* Element-wise addition: C = A + B */
tensor_id_t pt_add(tensor_id_t a, tensor_id_t b);

/* Element-wise multiplication: C = A * B */
tensor_id_t pt_multiply(tensor_id_t a, tensor_id_t b);

/* ReLU activation: y = max(0, x) */
tensor_id_t pt_relu(tensor_id_t x);

/* Softmax activation along dimension */
tensor_id_t pt_softmax(tensor_id_t x, int64_t dim);

/* Reshape tensor */
tensor_id_t pt_reshape(
    tensor_id_t x,
    const int64_t* new_shape,
    int64_t shape_len
);

/* Move tensor to device */
tensor_id_t pt_to_device(tensor_id_t x, device_t device);

/* Free tensor (cleanup) */
void pt_tensor_free(tensor_id_t x);

/* ============================================================================
   TENSORFLOW-STYLE API
   ============================================================================ */

/* Create constant tensor */
tensor_id_t tf_constant(
    const double* data,
    int64_t data_len,
    const int64_t* shape,
    int64_t shape_len,
    dtype_t dtype
);

/* Create variable (trainable) */
tensor_id_t tf_variable(tensor_id_t initial_value);

/* TensorFlow matrix multiplication */
tensor_id_t tf_matmul(tensor_id_t a, tensor_id_t b);

/* TensorFlow ReLU */
tensor_id_t tf_nn_relu(tensor_id_t x);

/* TensorFlow Softmax */
tensor_id_t tf_nn_softmax(tensor_id_t x, int64_t axis);

/* Sparse categorical cross-entropy loss
   Args:
     y_true: ground truth labels (int tensor)
     y_pred: predicted logits
   Returns:
     scalar loss tensor_id
*/
tensor_id_t tf_losses_sparse_categorical_crossentropy(
    tensor_id_t y_true,
    tensor_id_t y_pred
);

/* ============================================================================
   GRADIENT & OPTIMIZATION
   ============================================================================ */

/* Compute gradients via backward pass
   Args:
     loss: scalar loss tensor
   Returns:
     0 on success, <0 on error
*/
int ml_backward(tensor_id_t loss);

/* Get gradient of tensor
   Args:
     tensor_id: tensor whose gradient to get
   Returns:
     gradient tensor_id
*/
tensor_id_t ml_get_grad(tensor_id_t tensor_id);

/* Gradient descent optimizer step
   Args:
     learning_rate: step size
     tensor_id: parameter to update
   Returns:
     0 on success
*/
int ml_sgd_step(double learning_rate, tensor_id_t tensor_id);

/* Adam optimizer step
   Args:
     learning_rate: step size
     beta1, beta2: momentum parameters
     epsilon: numerical stability
     tensor_id: parameter to update
*/
int ml_adam_step(
    double learning_rate,
    double beta1,
    double beta2,
    double epsilon,
    tensor_id_t tensor_id
);

/* ============================================================================
   MODEL UTILITIES
   ============================================================================ */

/* Create empty model */
int64_t ml_model_new();

/* Add dense layer to model
   Args:
     model_id: model identifier
     in_features, out_features: layer dimensions
   Returns:
     layer_id
*/
int64_t ml_model_add_dense(
    int64_t model_id,
    int64_t in_features,
    int64_t out_features
);

/* Forward pass
   Args:
     model_id: model identifier
     x: input tensor
   Returns:
     output tensor_id
*/
tensor_id_t ml_model_forward(int64_t model_id, tensor_id_t x);

/* Free model */
void ml_model_free(int64_t model_id);

/* ============================================================================
   DATA LOADING & PREPROCESSING
   ============================================================================ */

/* Load CSV file
   Args:
     path: file path
     target_column: column name to use as labels
     out_features: output buffer for feature matrix
     out_labels: output buffer for label vector
   Returns:
     number of rows, <0 on error
*/
int64_t ml_load_csv(
    const char* path,
    const char* target_column,
    tensor_id_t* out_features,
    tensor_id_t* out_labels
);

/* Train-test split
   Args:
     x, y: input tensors
     test_size: fraction for test set (0.0-1.0)
     out arrays: output tensors
*/
int ml_train_test_split(
    tensor_id_t x,
    tensor_id_t y,
    double test_size,
    tensor_id_t* x_train,
    tensor_id_t* x_test,
    tensor_id_t* y_train,
    tensor_id_t* y_test
);

/* Normalize features (mean=0, std=1) */
tensor_id_t ml_normalize(tensor_id_t x);

/* One-hot encoding */
tensor_id_t ml_one_hot(tensor_id_t y, int64_t num_classes);

/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================ */

/* Print tensor (for debugging) */
void ml_tensor_print(tensor_id_t x);

/* Get library version */
const char* ml_version();

/* Initialize ML backend (call once at startup) */
int ml_init();

/* Cleanup ML backend (call once at shutdown) */
void ml_cleanup();

#ifdef __cplusplus
}
#endif

#endif /* LIBACML_H */
