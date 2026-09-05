// AC ilib: ml - small native C++ tensor/autograd core
#include "libacml.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

struct Tensor {
    std::vector<double> data;
    std::vector<int64_t> shape;
    dtype_t dtype = DT_FLOAT64;
    bool grad_track = false;
    std::vector<double> grad;
    std::string op;
    tensor_id_t a = 0;
    tensor_id_t b = 0;
};

std::map<tensor_id_t, Tensor> tensors;
tensor_id_t next_tensor_id = 1;
std::mt19937_64 rng(0xACAC2026ULL);

int64_t numel(const std::vector<int64_t>& shape) {
    if (shape.empty()) return 1;
    int64_t n = 1;
    for (int64_t d : shape) {
        if (d <= 0) return 0;
        n *= d;
    }
    return n;
}

Tensor* get(tensor_id_t id) {
    auto it = tensors.find(id);
    return it == tensors.end() ? nullptr : &it->second;
}

tensor_id_t store(Tensor t) {
    if (t.shape.empty()) t.shape = {static_cast<int64_t>(t.data.size())};
    if (t.data.empty()) t.data.resize(static_cast<size_t>(numel(t.shape)), 0.0);
    t.grad.assign(t.data.size(), 0.0);
    tensor_id_t id = next_tensor_id++;
    tensors[id] = std::move(t);
    return id;
}

tensor_id_t scalar(double v) {
    Tensor t;
    t.data = {v};
    t.shape = {};
    return store(std::move(t));
}

bool same_size(const Tensor& a, const Tensor& b) {
    return a.data.size() == b.data.size();
}

void add_grad(Tensor& t, const std::vector<double>& g) {
    if (t.grad.size() != t.data.size()) t.grad.assign(t.data.size(), 0.0);
    for (size_t i = 0; i < t.grad.size() && i < g.size(); ++i) t.grad[i] += g[i];
}

void backward_into(tensor_id_t id, const std::vector<double>& incoming) {
    Tensor* t = get(id);
    if (!t) return;
    add_grad(*t, incoming);

    if (t->op == "add") {
        backward_into(t->a, incoming);
        backward_into(t->b, incoming);
    } else if (t->op == "multiply") {
        Tensor* a = get(t->a);
        Tensor* b = get(t->b);
        if (!a || !b || !same_size(*a, *b)) return;
        std::vector<double> ga(incoming.size()), gb(incoming.size());
        for (size_t i = 0; i < incoming.size(); ++i) {
            ga[i] = incoming[i] * b->data[i];
            gb[i] = incoming[i] * a->data[i];
        }
        backward_into(t->a, ga);
        backward_into(t->b, gb);
    } else if (t->op == "relu") {
        Tensor* a = get(t->a);
        if (!a) return;
        std::vector<double> ga(incoming.size());
        for (size_t i = 0; i < incoming.size() && i < a->data.size(); ++i)
            ga[i] = a->data[i] > 0.0 ? incoming[i] : 0.0;
        backward_into(t->a, ga);
    }
}

} // namespace

extern "C" {

int ml_init() { return 0; }
void ml_cleanup() { tensors.clear(); next_tensor_id = 1; }
const char* ml_version() { return "acml-native-0.1"; }

tensor_id_t ml_tensor(double value) { return scalar(value); }

tensor_id_t ml_grid(int64_t rows, int64_t cols, double fill) {
    if (rows <= 0 || cols <= 0) return -1;
    Tensor t;
    t.shape = {rows, cols};
    t.data.assign(static_cast<size_t>(rows * cols), fill);
    return store(std::move(t));
}

int ml_gradient_track(tensor_id_t id, int enabled) {
    Tensor* t = get(id);
    if (!t) return -1;
    t->grad_track = enabled != 0;
    if (t->grad.size() != t->data.size()) t->grad.assign(t->data.size(), 0.0);
    return 0;
}

int ml_grad_wipe(tensor_id_t id) {
    Tensor* t = get(id);
    if (!t) return -1;
    t->grad.assign(t->data.size(), 0.0);
    return 0;
}

int ml_weights(double learning_rate, tensor_id_t id) {
    Tensor* t = get(id);
    if (!t) return -1;
    if (t->grad.size() != t->data.size()) return -2;
    for (size_t i = 0; i < t->data.size(); ++i) t->data[i] -= learning_rate * t->grad[i];
    return 0;
}

double ml_take(tensor_id_t id) {
    Tensor* t = get(id);
    if (!t || t->data.empty()) return 0.0;
    return t->data[0];
}

tensor_id_t ml_add(tensor_id_t a_id, tensor_id_t b_id) {
    Tensor* a = get(a_id);
    Tensor* b = get(b_id);
    if (!a || !b || !same_size(*a, *b)) return -1;
    Tensor out;
    out.shape = a->shape;
    out.data.resize(a->data.size());
    for (size_t i = 0; i < out.data.size(); ++i) out.data[i] = a->data[i] + b->data[i];
    out.grad_track = a->grad_track || b->grad_track;
    out.op = "add";
    out.a = a_id;
    out.b = b_id;
    return store(std::move(out));
}

tensor_id_t ml_multiply(tensor_id_t a_id, tensor_id_t b_id) {
    Tensor* a = get(a_id);
    Tensor* b = get(b_id);
    if (!a || !b || !same_size(*a, *b)) return -1;
    Tensor out;
    out.shape = a->shape;
    out.data.resize(a->data.size());
    for (size_t i = 0; i < out.data.size(); ++i) out.data[i] = a->data[i] * b->data[i];
    out.grad_track = a->grad_track || b->grad_track;
    out.op = "multiply";
    out.a = a_id;
    out.b = b_id;
    return store(std::move(out));
}

tensor_id_t ml_relu(tensor_id_t x_id) {
    Tensor* x = get(x_id);
    if (!x) return -1;
    Tensor out;
    out.shape = x->shape;
    out.data.resize(x->data.size());
    for (size_t i = 0; i < out.data.size(); ++i) out.data[i] = std::max(0.0, x->data[i]);
    out.grad_track = x->grad_track;
    out.op = "relu";
    out.a = x_id;
    return store(std::move(out));
}

int ml_backward(tensor_id_t loss) {
    Tensor* t = get(loss);
    if (!t) return -1;
    std::vector<double> seed(t->data.size(), 1.0);
    backward_into(loss, seed);
    return 0;
}

tensor_id_t ml_get_grad(tensor_id_t id) {
    Tensor* t = get(id);
    if (!t) return -1;
    Tensor g;
    g.shape = t->shape;
    g.data = t->grad;
    return store(std::move(g));
}

int ml_sgd_step(double learning_rate, tensor_id_t tensor_id) {
    return ml_weights(learning_rate, tensor_id);
}

// ml.acl maps ml:grad -> ml.grad and ml:optimize -> ml.optimize, but this file only ever
// exported ml_get_grad/ml_sgd_step — a name mismatch every non-C++ binding hit as a real,
// silent gap (ml.hpp's C++ wrapper happened to rename `grad` on its own end, but nothing
// bound `ml.optimize` anywhere, and no non-C++ binding bound `ml.grad` either). These two
// exports are pure aliases under the EXACT names the .acl table (and every language binding
// generated from it) actually expects.
tensor_id_t ml_grad(tensor_id_t id) { return ml_get_grad(id); }
int ml_optimize(double learning_rate, tensor_id_t tensor_id) { return ml_sgd_step(learning_rate, tensor_id); }

int ml_adam_step(double learning_rate, double, double, double, tensor_id_t tensor_id) {
    return ml_weights(learning_rate, tensor_id);
}

tensor_id_t pt_tensor_from_data(const double* data, int64_t data_len,
                                const int64_t* shape, int64_t shape_len, dtype_t dtype) {
    if (!data || data_len < 0) return -1;
    Tensor t;
    t.dtype = dtype;
    t.data.assign(data, data + data_len);
    if (shape && shape_len > 0) t.shape.assign(shape, shape + shape_len);
    else t.shape = {data_len};
    if (numel(t.shape) != data_len) return -2;
    return store(std::move(t));
}

tensor_id_t pt_randn(const int64_t* shape, int64_t shape_len, dtype_t dtype) {
    if (!shape || shape_len <= 0) return -1;
    Tensor t;
    t.dtype = dtype;
    t.shape.assign(shape, shape + shape_len);
    t.data.resize(static_cast<size_t>(numel(t.shape)));
    std::normal_distribution<double> dist(0.0, 1.0);
    for (double& v : t.data) v = dist(rng);
    return store(std::move(t));
}

tensor_id_t pt_zeros(const int64_t* shape, int64_t shape_len, dtype_t dtype) {
    if (!shape || shape_len <= 0) return -1;
    Tensor t;
    t.dtype = dtype;
    t.shape.assign(shape, shape + shape_len);
    t.data.assign(static_cast<size_t>(numel(t.shape)), 0.0);
    return store(std::move(t));
}

tensor_id_t pt_ones(const int64_t* shape, int64_t shape_len, dtype_t dtype) {
    if (!shape || shape_len <= 0) return -1;
    Tensor t;
    t.dtype = dtype;
    t.shape.assign(shape, shape + shape_len);
    t.data.assign(static_cast<size_t>(numel(t.shape)), 1.0);
    return store(std::move(t));
}

int64_t pt_tensor_shape(tensor_id_t id, int64_t* out, int64_t out_len) {
    Tensor* t = get(id);
    if (!t || !out || out_len <= 0) return -1;
    int64_t n = std::min<int64_t>(out_len, static_cast<int64_t>(t->shape.size()));
    for (int64_t i = 0; i < n; ++i) out[i] = t->shape[static_cast<size_t>(i)];
    return static_cast<int64_t>(t->shape.size());
}

dtype_t pt_tensor_dtype(tensor_id_t id) {
    Tensor* t = get(id);
    return t ? t->dtype : DT_FLOAT64;
}

int64_t pt_tensor_to_array(tensor_id_t id, double* out, int64_t out_len) {
    Tensor* t = get(id);
    if (!t || !out || out_len <= 0) return -1;
    int64_t n = std::min<int64_t>(out_len, static_cast<int64_t>(t->data.size()));
    for (int64_t i = 0; i < n; ++i) out[i] = t->data[static_cast<size_t>(i)];
    return n;
}

tensor_id_t pt_add(tensor_id_t a, tensor_id_t b) { return ml_add(a, b); }
tensor_id_t pt_multiply(tensor_id_t a, tensor_id_t b) { return ml_multiply(a, b); }
tensor_id_t pt_relu(tensor_id_t x) { return ml_relu(x); }

tensor_id_t pt_matmul(tensor_id_t a_id, tensor_id_t b_id) {
    Tensor* a = get(a_id);
    Tensor* b = get(b_id);
    if (!a || !b || a->shape.size() != 2 || b->shape.size() != 2) return -1;
    int64_t m = a->shape[0], k = a->shape[1], k2 = b->shape[0], n = b->shape[1];
    if (k != k2) return -2;
    Tensor out;
    out.shape = {m, n};
    out.data.assign(static_cast<size_t>(m * n), 0.0);
    for (int64_t r = 0; r < m; ++r)
        for (int64_t c = 0; c < n; ++c)
            for (int64_t i = 0; i < k; ++i)
                out.data[static_cast<size_t>(r * n + c)] +=
                    a->data[static_cast<size_t>(r * k + i)] * b->data[static_cast<size_t>(i * n + c)];
    return store(std::move(out));
}

tensor_id_t pt_softmax(tensor_id_t x_id, int64_t) {
    Tensor* x = get(x_id);
    if (!x) return -1;
    Tensor out;
    out.shape = x->shape;
    out.data.resize(x->data.size());
    if (x->data.empty()) return store(std::move(out));   // empty tensor → no max_element deref (was UB)
    double maxv = *std::max_element(x->data.begin(), x->data.end());
    double sum = 0.0;
    for (size_t i = 0; i < x->data.size(); ++i) {
        out.data[i] = std::exp(x->data[i] - maxv);
        sum += out.data[i];
    }
    if (sum != 0.0) for (double& v : out.data) v /= sum;
    return store(std::move(out));
}

tensor_id_t pt_reshape(tensor_id_t x_id, const int64_t* shape, int64_t shape_len) {
    Tensor* x = get(x_id);
    if (!x || !shape || shape_len <= 0) return -1;
    std::vector<int64_t> ns(shape, shape + shape_len);
    if (numel(ns) != static_cast<int64_t>(x->data.size())) return -2;
    Tensor out = *x;
    out.shape = std::move(ns);
    out.op.clear();
    return store(std::move(out));
}

tensor_id_t pt_to_device(tensor_id_t x, device_t) { return x; }
void pt_tensor_free(tensor_id_t x) { tensors.erase(x); }

tensor_id_t tf_constant(const double* data, int64_t data_len, const int64_t* shape,
                        int64_t shape_len, dtype_t dtype) {
    return pt_tensor_from_data(data, data_len, shape, shape_len, dtype);
}
tensor_id_t tf_variable(tensor_id_t initial_value) {
    Tensor* t = get(initial_value);
    if (!t) return -1;
    t->grad_track = true;
    return initial_value;
}
tensor_id_t tf_matmul(tensor_id_t a, tensor_id_t b) { return pt_matmul(a, b); }
tensor_id_t tf_nn_relu(tensor_id_t x) { return pt_relu(x); }
tensor_id_t tf_nn_softmax(tensor_id_t x, int64_t axis) { return pt_softmax(x, axis); }
tensor_id_t tf_losses_sparse_categorical_crossentropy(tensor_id_t, tensor_id_t y_pred) { return y_pred; }

int64_t ml_model_new() { return 1; }
int64_t ml_model_add_dense(int64_t, int64_t, int64_t) { return 1; }
tensor_id_t ml_model_forward(int64_t, tensor_id_t x) { return x; }
void ml_model_free(int64_t) {}
int64_t ml_load_csv(const char*, const char*, tensor_id_t*, tensor_id_t*) { return -1; }
int ml_train_test_split(tensor_id_t, tensor_id_t, double, tensor_id_t*, tensor_id_t*, tensor_id_t*, tensor_id_t*) { return -1; }
tensor_id_t ml_normalize(tensor_id_t x) { return x; }
tensor_id_t ml_one_hot(tensor_id_t y, int64_t) { return y; }
void ml_tensor_print(tensor_id_t x) { std::printf("%g\n", ml_take(x)); }

} // extern "C"
