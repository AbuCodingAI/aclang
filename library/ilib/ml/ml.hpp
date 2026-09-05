#pragma once
#include "libacml.h"

struct ACMLNamespace {
    tensor_id_t tensor(double value) const { return ml_tensor(value); }
    tensor_id_t grid(long long rows, long long cols, double fill = 0.0) const {
        return ml_grid(rows, cols, fill);
    }
    int gradient_track(tensor_id_t id, long long enabled) const {
        return ml_gradient_track(id, enabled != 0);
    }
    int backward(tensor_id_t loss) const { return ml_backward(loss); }
    int grad_wipe(tensor_id_t id) const { return ml_grad_wipe(id); }
    int weights(double learning_rate, tensor_id_t id) const { return ml_weights(learning_rate, id); }
    // ml.acl maps ml:optimize -> ml.optimize, but no backend (this one included) ever had a
    // matching function — `ml.optimize(...)` was a hard compile error on every backend that
    // reaches real typechecking (verified: C++ "no member named 'optimize'"). ml_sgd_step is
    // already a real, working alias of ml_weights in the C ABI; wire it up here too.
    int optimize(double learning_rate, tensor_id_t id) const { return ml_optimize(learning_rate, id); }
    double take(tensor_id_t id) const { return ml_take(id); }
    tensor_id_t grad(tensor_id_t id) const { return ml_grad(id); }
    tensor_id_t add(tensor_id_t a, tensor_id_t b) const { return ml_add(a, b); }
    tensor_id_t multiply(tensor_id_t a, tensor_id_t b) const { return ml_multiply(a, b); }
    tensor_id_t relu(tensor_id_t x) const { return ml_relu(x); }
};

static const ACMLNamespace ml;
