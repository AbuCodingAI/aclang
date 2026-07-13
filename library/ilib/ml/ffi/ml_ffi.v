#flag -I@AC_LIBDIR@
#flag -L@AC_LIBDIR@ -lacml
#include "libacml.h"

fn C.ml_tensor(value f64) i64
fn C.ml_grid(rows i64, cols i64, fill f64) i64
fn C.ml_gradient_track(tensor_id i64, enabled int) int
fn C.ml_grad_wipe(tensor_id i64) int
fn C.ml_weights(learning_rate f64, tensor_id i64) int
fn C.ml_take(tensor_id i64) f64
fn C.ml_add(a i64, b i64) i64
fn C.ml_multiply(a i64, b i64) i64
fn C.ml_relu(x i64) i64
fn C.ml_backward(loss i64) int

struct MlNamespace {}

const ml = MlNamespace{}

fn (MlNamespace) tensor(value f64) i64 { return C.ml_tensor(value) }
fn (MlNamespace) grid(rows i64, cols i64, fill f64) i64 { return C.ml_grid(rows, cols, fill) }
fn (MlNamespace) gradient_track(tensor_id i64, enabled i64) i64 { return C.ml_gradient_track(tensor_id, int(enabled)) }
fn (MlNamespace) grad_wipe(tensor_id i64) i64 { return C.ml_grad_wipe(tensor_id) }
fn (MlNamespace) weights(learning_rate f64, tensor_id i64) i64 { return C.ml_weights(learning_rate, tensor_id) }
fn (MlNamespace) take(tensor_id i64) f64 { return C.ml_take(tensor_id) }
fn (MlNamespace) add(a i64, b i64) i64 { return C.ml_add(a, b) }
fn (MlNamespace) multiply(a i64, b i64) i64 { return C.ml_multiply(a, b) }
fn (MlNamespace) relu(x i64) i64 { return C.ml_relu(x) }
fn (MlNamespace) backward(loss i64) i64 { return C.ml_backward(loss) }
