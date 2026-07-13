# ilib/ml - Native Machine Learning Seed for AC

`ilib/ml` is a native C++ `libacml.so` library. It does not wrap PyTorch,
TensorFlow, NumPy, or Python. The first supported surface is the small
Rosetta-style API used by AC examples.

| AC API | Meaning |
| --- | --- |
| `ml.tensor(value)` / `ml.grid(rows, cols, fill)` | allocate tensor handles |
| `ml.gradient_track(t, enabled)` | toggle gradient tracking |
| `ml.backward(loss)` | run reverse-mode scalar autodiff |
| `ml.grad_wipe(t)` | clear gradients |
| `ml.weights(rate, t)` | SGD update for one tensor |
| `ml.take(t)` | read the first scalar value |
| `ml.add(a, b)`, `ml.multiply(a, b)`, `ml.relu(x)` | primitive ops |

The older `pt_*` and `tf_*` symbols are compatibility stubs backed by the same
native tensor store. They are not real PyTorch or TensorFlow integrations.

## Quick Start

```ac
AC->CPP
use ilib ml

<mainloop>
    w = ml.tensor(2)
    ml.gradient_track(w, 1)
    x = ml.tensor(3)
    y = ml.multiply(w, x)
    loss = ml.multiply(y, y)
    ml.backward(loss)
    Term.display ml.take(w)
    ml.weights(0.01, w)
    Term.display ml.take(w)
<mainloop>
```

Expected output starts at `2` and then updates `w` with one SGD step.

## Architecture

```
ilib/ml/
├── ml.acl             <- lowering rules
├── libacml.h          <- C FFI header
├── ml.cpp             <- native C++ implementation
├── ml.hpp             <- C++ namespace wrapper
├── ml_c.h             <- C include shim
├── ffi/               <- backend shims where needed
├── libacml.so         <- compiled shared object
└── README.md
```

## Backend Status

| Backend | Status |
| --- | --- |
| AC->C / AC->C++ | native `libacml.so` |
| AC->BNY | dynamic `libacml.so` routing |
| AC->PY / RS / GO / V | thin shims to `libacml.so` |
| AC->JS / Java | scalar smoke-test shims only |
| AI->VM / HTML | not implemented |

## Build

```bash
make -C library/ilib/ml
make -C ac-compiler
```
