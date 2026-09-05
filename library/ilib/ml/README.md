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
| `ml.grad(t)` | read back the accumulated gradient (as a new tensor) |
| `ml.weights(rate, t)` / `ml.optimize(rate, t)` | SGD update for one tensor (aliases) |
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
| AC->C / AC->C++ | native `libacml.so` — verified numerically correct |
| AC->BNY | dynamic `libacml.so` routing — verified numerically correct |
| AC->PY / RS / GO / V | thin shims to `libacml.so` — verified numerically correct |
| AC->JS / Java | independent pure-language autodiff engines (not FFI-bound — no native
  binding is possible in a browser/plain-Node or a JVM without JNI) — verified numerically
  correct against the native engine (matching gradients/SGD steps) for every operation
  `ml.acl` exposes. `grid()`'s multi-element shape is collapsed to a single scalar
  internally; this is behaviorally identical to native's real per-element storage for
  everything AC's own `ml.acl` surface can express (no per-element read/write exists in the
  language yet), but is not literally the same implementation — worth remembering if a
  future per-element indexing feature gets added to `ml.acl`, since JS/Java would need real
  arrays at that point, not a collapse. |
| AI->VM / HTML | not implemented — a real, unstarted feature gap, not a stub |

## Build

```bash
make -C library/ilib/ml
make -C ac-compiler
```
