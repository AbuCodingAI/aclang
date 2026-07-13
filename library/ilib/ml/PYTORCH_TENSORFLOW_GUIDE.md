# PyTorch + TensorFlow Guide for libacml Implementation

Learn PyTorch and TensorFlow fundamentals to implement the ML library.

---

## Part 1: PyTorch Fundamentals

### 1.1 What is PyTorch?

PyTorch is a tensor computation library with GPU support.

```python
import torch

# Create tensor (like numpy array but on GPU)
x = torch.tensor([1.0, 2.0, 3.0])  # [3]
print(x)  # tensor([1., 2., 3.])

# Create from Python list
y = torch.zeros(3, 4)              # 3x4 zeros
z = torch.ones(2, 5)               # 2x5 ones
r = torch.randn(3, 4)              # 3x4 random (N(0,1))

# Shape
print(r.shape)                      # torch.Size([3, 4])
print(r.shape[0])                   # 3 (rows)

# Data type
print(r.dtype)                      # torch.float32
r_int = r.to(torch.int32)           # Convert dtype
```

### 1.2 Tensor Operations

```python
# Basic math
a = torch.tensor([[1.0, 2.0], [3.0, 4.0]])
b = torch.tensor([[5.0, 6.0], [7.0, 8.0]])

# Element-wise
c = a + b                           # Addition
d = a * b                           # Multiply
e = a / b                           # Divide

# Matrix multiplication
w = torch.randn(4, 5)              # Weights [4, 5]
x = torch.randn(3, 4)              # Input [3, 4]
y = torch.matmul(x, w)              # Result [3, 5]
# or: y = x @ w

# Reshape & transpose
x = torch.randn(12)
x_reshaped = x.reshape(3, 4)        # [12] → [3, 4]
x_t = x_reshaped.t()               # Transpose [3, 4] → [4, 3]
x_flat = x_reshaped.flatten()       # [3, 4] → [12]

# Activation functions
x = torch.randn(3, 4)
relu_x = torch.nn.functional.relu(x)        # ReLU: max(0, x)
softmax_x = torch.softmax(x, dim=1)         # Softmax along dim 1

# Useful methods
x_sum = x.sum()                     # Sum all elements
x_mean = x.mean()                   # Mean
x_max = x.max()                     # Maximum
```

### 1.3 Autograd: Automatic Differentiation

```python
# PyTorch automatically computes gradients!
x = torch.tensor([2.0, 3.0], requires_grad=True)  # Enable gradient tracking

# Forward pass
y = x ** 2                          # y = [4.0, 9.0]
z = y.sum()                         # z = 13.0

# Backward pass
z.backward()                        # Compute gradients
print(x.grad)                       # tensor([4.0, 6.0])  ← dz/dx = 2x

# Why? Because:
# z = x[0]^2 + x[1]^2
# dz/dx[0] = 2*x[0] = 2*2 = 4
# dz/dx[1] = 2*x[1] = 2*3 = 6
```

### 1.4 Neural Networks

```python
import torch.nn as nn
import torch.optim as optim

# Define a simple neural network
class SimpleNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(784, 128)      # 784 → 128
        self.fc2 = nn.Linear(128, 10)       # 128 → 10
    
    def forward(self, x):
        x = torch.relu(self.fc1(x))         # Apply ReLU
        x = self.fc2(x)                     # Output layer
        return x

# Create model
model = SimpleNet()

# Loss function
criterion = nn.CrossEntropyLoss()  # For classification

# Optimizer (updates weights)
optimizer = optim.Adam(model.parameters(), lr=0.001)

# Training loop
for epoch in range(10):
    for batch_x, batch_y in dataloader:
        # Forward pass
        pred = model(batch_x)                   # Compute predictions
        loss = criterion(pred, batch_y)         # Compute loss
        
        # Backward pass
        optimizer.zero_grad()                   # Clear old gradients
        loss.backward()                         # Compute new gradients
        optimizer.step()                        # Update weights
        
        print(f"Loss: {loss.item()}")
```

### 1.5 Key PyTorch Concepts for libacml

```python
# Tensor storage (how to store tensors)
tensor_store = {}  # dict: id → tensor
next_id = 1

def create_tensor(data):
    global next_id
    t = torch.tensor(data)
    tensor_store[next_id] = t
    next_id += 1
    return next_id - 1

def get_tensor(tensor_id):
    return tensor_store[tensor_id]

def free_tensor(tensor_id):
    del tensor_store[tensor_id]

# Device handling (CPU vs GPU)
x = torch.randn(3, 4)
x_cuda = x.to('cuda')               # Move to GPU
x_cpu = x_cuda.to('cpu')            # Move back to CPU

# Gradient tracking
x = torch.randn(3, 4, requires_grad=True)
y = x.sum()
y.backward()                        # Computes x.grad

# Detach (stop gradient tracking)
z = x.detach()                      # z has no gradient
```

---

## Part 2: TensorFlow Fundamentals

### 2.1 What is TensorFlow?

TensorFlow is a symbolic computation library (more rigid than PyTorch).

```python
import tensorflow as tf

# Create tensors
x = tf.constant([1.0, 2.0, 3.0])
y = tf.zeros([3, 4])               # 3x4 zeros
z = tf.ones([2, 5])                # 2x5 ones
r = tf.random.normal([3, 4])       # 3x4 random

# Properties
print(x.shape)                      # TensorShape([3])
print(x.dtype)                      # <dtype: 'float32'>

# Convert dtype
x_int = tf.cast(x, tf.int32)
```

### 2.2 Tensor Operations (TensorFlow)

```python
# Math operations
a = tf.constant([[1.0, 2.0], [3.0, 4.0]])
b = tf.constant([[5.0, 6.0], [7.0, 8.0]])

c = a + b                           # Addition
d = a * b                           # Element-wise multiply
e = tf.matmul(a, b)                # Matrix multiplication

# Reshaping
x = tf.range(12)                    # [0, 1, 2, ..., 11]
x_reshaped = tf.reshape(x, [3, 4]) # [12] → [3, 4]
x_t = tf.transpose(x_reshaped)     # Transpose

# Activations
x = tf.random.normal([3, 4])
relu_x = tf.nn.relu(x)             # ReLU
softmax_x = tf.nn.softmax(x, axis=1)  # Softmax

# Reduction
x_sum = tf.reduce_sum(x)           # Sum
x_mean = tf.reduce_mean(x)         # Mean
x_max = tf.reduce_max(x)           # Max
```

### 2.3 GradientTape: Automatic Differentiation

```python
# TensorFlow's autograd (different from PyTorch!)
x = tf.Variable([2.0, 3.0])         # Variable = trainable

with tf.GradientTape() as tape:
    y = x ** 2
    z = tf.reduce_sum(y)

grad = tape.gradient(z, x)
print(grad)                         # [4.0, 6.0]
```

### 2.4 Neural Networks (TensorFlow/Keras)

```python
from tensorflow import keras
from tensorflow.keras import layers

# Simple model
model = keras.Sequential([
    layers.Dense(128, activation='relu', input_shape=(784,)),
    layers.Dense(10, activation='softmax')
])

# Compile
model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

# Train
model.fit(train_data, train_labels, epochs=10, batch_size=32)

# Predict
predictions = model.predict(test_data)
```

### 2.5 Custom Training Loop (TensorFlow)

```python
# More control over training
model = keras.Sequential([
    layers.Dense(128, activation='relu'),
    layers.Dense(10)
])

loss_fn = keras.losses.SparseCategoricalCrossentropy(from_logits=True)
optimizer = keras.optimizers.Adam(learning_rate=0.001)

for epoch in range(10):
    for batch_x, batch_y in train_dataset:
        with tf.GradientTape() as tape:
            pred = model(batch_x)
            loss = loss_fn(batch_y, pred)
        
        grads = tape.gradient(loss, model.trainable_weights)
        optimizer.apply_gradients(zip(grads, model.trainable_weights))
```

---

## Part 3: PyTorch vs TensorFlow

| Aspect | PyTorch | TensorFlow |
|--------|---------|-----------|
| **Philosophy** | Eager execution (run code immediately) | Symbolic computation (build graph first) |
| **Debugging** | Easier (like normal Python) | Harder (graph mode) |
| **Flexibility** | More flexible | More structured |
| **Performance** | Good | Excellent (optimized graphs) |
| **Gradients** | `.backward()` | `GradientTape()` context |
| **GPU** | `.to('cuda')` | Automatic |
| **Production** | TorchServe | TF Serving (battle-tested) |
| **Research** | More popular | Less popular now |

---

## Part 4: Implementing libacml.cpp

Here's how to implement the C functions:

```cpp
// libacml.cpp
#include "libacml.h"
#include <torch/torch.h>
#include <unordered_map>

// Global tensor storage
static std::unordered_map<int64_t, torch::Tensor> tensor_store;
static int64_t next_tensor_id = 1;

// ============================================================================
// TENSOR CREATION
// ============================================================================

extern "C" {
    tensor_id_t pt_tensor_from_data(
        const double* data, int64_t data_len,
        const int64_t* shape, int64_t shape_len,
        dtype_t dtype
    ) {
        try {
            // Convert shape array to torch::IntArrayRef
            std::vector<int64_t> shape_vec(shape, shape + shape_len);
            
            // Create tensor from data
            auto options = torch::TensorOptions().dtype(torch::kFloat32);
            torch::Tensor t = torch::from_blob(
                const_cast<double*>(data),
                shape_vec,
                options
            ).clone();  // clone to own the data
            
            // Store and return ID
            int64_t id = next_tensor_id++;
            tensor_store[id] = t;
            return id;
        } catch (...) {
            return -1;  // Error
        }
    }

    tensor_id_t pt_randn(
        const int64_t* shape, int64_t shape_len,
        dtype_t dtype
    ) {
        try {
            std::vector<int64_t> shape_vec(shape, shape + shape_len);
            torch::Tensor t = torch::randn(shape_vec);
            
            int64_t id = next_tensor_id++;
            tensor_store[id] = t;
            return id;
        } catch (...) {
            return -1;
        }
    }

    tensor_id_t pt_zeros(
        const int64_t* shape, int64_t shape_len,
        dtype_t dtype
    ) {
        try {
            std::vector<int64_t> shape_vec(shape, shape + shape_len);
            torch::Tensor t = torch::zeros(shape_vec);
            
            int64_t id = next_tensor_id++;
            tensor_store[id] = t;
            return id;
        } catch (...) {
            return -1;
        }
    }

    // ========================================================================
    // TENSOR QUERIES
    // ========================================================================

    int64_t pt_tensor_shape(
        tensor_id_t tensor_id,
        int64_t* shape_out,
        int64_t shape_len
    ) {
        try {
            auto t = tensor_store.at(tensor_id);
            auto shape = t.sizes();
            
            // Copy shape to output
            int64_t copy_len = std::min((int64_t)shape.size(), shape_len);
            for (int64_t i = 0; i < copy_len; i++) {
                shape_out[i] = shape[i];
            }
            return (int64_t)shape.size();
        } catch (...) {
            return -1;
        }
    }

    dtype_t pt_tensor_dtype(tensor_id_t tensor_id) {
        try {
            auto t = tensor_store.at(tensor_id);
            if (t.dtype() == torch::kFloat32) return DT_FLOAT32;
            if (t.dtype() == torch::kFloat64) return DT_FLOAT64;
            if (t.dtype() == torch::kInt32) return DT_INT32;
            if (t.dtype() == torch::kInt64) return DT_INT64;
            if (t.dtype() == torch::kBool) return DT_BOOL;
            return -1;
        } catch (...) {
            return -1;
        }
    }

    int64_t pt_tensor_to_array(
        tensor_id_t tensor_id,
        double* out,
        int64_t out_len
    ) {
        try {
            auto t = tensor_store.at(tensor_id).cpu().flatten();
            int64_t numel = t.numel();
            int64_t copy_len = std::min(numel, out_len);
            
            // Copy to output array
            auto accessor = t.accessor<double, 1>();
            for (int64_t i = 0; i < copy_len; i++) {
                out[i] = accessor[i];
            }
            return copy_len;
        } catch (...) {
            return -1;
        }
    }

    // ========================================================================
    // OPERATIONS
    // ========================================================================

    tensor_id_t pt_matmul(tensor_id_t a, tensor_id_t b) {
        try {
            auto t_a = tensor_store.at(a);
            auto t_b = tensor_store.at(b);
            auto result = torch::matmul(t_a, t_b);
            
            int64_t id = next_tensor_id++;
            tensor_store[id] = result;
            return id;
        } catch (...) {
            return -1;
        }
    }

    tensor_id_t pt_add(tensor_id_t a, tensor_id_t b) {
        try {
            auto result = tensor_store.at(a) + tensor_store.at(b);
            int64_t id = next_tensor_id++;
            tensor_store[id] = result;
            return id;
        } catch (...) {
            return -1;
        }
    }

    tensor_id_t pt_relu(tensor_id_t x) {
        try {
            auto result = torch::relu(tensor_store.at(x));
            int64_t id = next_tensor_id++;
            tensor_store[id] = result;
            return id;
        } catch (...) {
            return -1;
        }
    }

    tensor_id_t pt_softmax(tensor_id_t x, int64_t dim) {
        try {
            auto result = torch::softmax(tensor_store.at(x), dim);
            int64_t id = next_tensor_id++;
            tensor_store[id] = result;
            return id;
        } catch (...) {
            return -1;
        }
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    void pt_tensor_free(tensor_id_t x) {
        try {
            tensor_store.erase(x);
        } catch (...) {
            // Ignore
        }
    }

    // ========================================================================
    // UTILITY
    // ========================================================================

    int ml_init() {
        // Initialize PyTorch (nothing needed usually)
        return 0;
    }

    void ml_cleanup() {
        tensor_store.clear();
    }

    const char* ml_version() {
        return "libacml 0.1.0";
    }
}
```

---

## Part 5: Implementing libacml.py

```python
# libacml.py
import torch
import numpy as np

# Tensor storage (like C++)
tensor_store = {}
next_tensor_id = 1

def pt_tensor_from_data(data, data_len, shape, shape_len, dtype):
    global next_tensor_id
    try:
        # Convert to numpy, then tensor
        data_arr = np.array(data[:data_len]).reshape(shape[:shape_len])
        t = torch.from_numpy(data_arr).float()
        
        tensor_id = next_tensor_id
        next_tensor_id += 1
        tensor_store[tensor_id] = t
        return tensor_id
    except:
        return -1

def pt_randn(shape, shape_len, dtype):
    global next_tensor_id
    try:
        t = torch.randn(shape[:shape_len])
        tensor_id = next_tensor_id
        next_tensor_id += 1
        tensor_store[tensor_id] = t
        return tensor_id
    except:
        return -1

def pt_zeros(shape, shape_len, dtype):
    global next_tensor_id
    try:
        t = torch.zeros(shape[:shape_len])
        tensor_id = next_tensor_id
        next_tensor_id += 1
        tensor_store[tensor_id] = t
        return tensor_id
    except:
        return -1

def pt_matmul(a, b):
    global next_tensor_id
    try:
        result = torch.matmul(tensor_store[a], tensor_store[b])
        tensor_id = next_tensor_id
        next_tensor_id += 1
        tensor_store[tensor_id] = result
        return tensor_id
    except:
        return -1

def pt_add(a, b):
    global next_tensor_id
    try:
        result = tensor_store[a] + tensor_store[b]
        tensor_id = next_tensor_id
        next_tensor_id += 1
        tensor_store[tensor_id] = result
        return tensor_id
    except:
        return -1

def pt_relu(x):
    global next_tensor_id
    try:
        result = torch.relu(tensor_store[x])
        tensor_id = next_tensor_id
        next_tensor_id += 1
        tensor_store[tensor_id] = result
        return tensor_id
    except:
        return -1

def pt_tensor_to_array(tensor_id, out, out_len):
    try:
        t = tensor_store[tensor_id].cpu().flatten()
        data = t.numpy()
        copy_len = min(len(data), out_len)
        for i in range(copy_len):
            out[i] = data[i]
        return copy_len
    except:
        return -1

def pt_tensor_free(x):
    try:
        del tensor_store[x]
    except:
        pass

def ml_init():
    return 0

def ml_cleanup():
    global tensor_store
    tensor_store.clear()

def ml_version():
    return "libacml 0.1.0"
```

---

## Part 6: Compile & Test

```bash
# Compile C++
g++ -O2 -fPIC -shared libacml.cpp -o libacml.so \
    -I/usr/include/torch -ltorch -ltorch_cpu

# Test from Python
python3 -c "
import ctypes
lib = ctypes.CDLL('./libacml.so')

# Create tensor [3, 4]
shape = (3, 4)
id = lib.pt_randn((ctypes.c_int64 * 2)(*shape), 2, 1)
print(f'Tensor ID: {id}')

# Get shape back
shape_out = (ctypes.c_int64 * 8)()
len_out = lib.pt_tensor_shape(id, shape_out, 8)
print(f'Shape: {shape_out[:len_out]}')
"
```

---

## Key Takeaways

1. **PyTorch**: Eager execution, `.backward()` for gradients, `.to()` for devices
2. **TensorFlow**: Symbolic graphs, `GradientTape()` for gradients, automatic device placement
3. **Storage**: Use dict/map to store tensors, return opaque IDs
4. **Data conversion**: Python ↔ C arrays via ctypes/numpy
5. **Error handling**: Always try-catch, return -1 on error
6. **Cleanup**: `pt_tensor_free()` removes from storage

Now you can implement libacml.cpp and libacml.py! 🚀
