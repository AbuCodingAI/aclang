# AC Variable Scoping: free / bound / NA

> **Status: Documented, not yet implemented.**
> This describes the planned scoping system for AC. Current implementation uses simple lexical scope.

---

## The Three States

| State | Meaning |
|-------|---------|
| `free` | Global / live / shared — persists across all execution contexts |
| `bound` | Temporary local execution state — owned by a loop or execution container |
| `NA` | Declared but inactive — function internals before the function runs |

---

## Rules

### 1. Functions start as NA

```ac
Make add func(a, b)
    result = a + b
    return result
```

The function exists in the symbol table, but:
- Its variables (`result`) are inactive
- No execution frame has been allocated yet
- State: **NA**

---

### 2. Normal function execution: NA → free

When a function is called outside a loop, its mutations promote to the global (free) scope.

```ac
x = 10

Make f func()
    x = 5

f()
* x is now 5 — free mutation persisted *
```

Execution path: **NA → free**

The function call creates a frame, runs, and its writes are visible globally after return.

---

### 3. Loop execution: NA → bound

When a function is called inside a loop context, mutations become **bound** — temporary and loop-local.

```ac
x = 10

Make change func()
    x = 99

for i = 1 to 3
    change()
    * x is bound = 99 inside this iteration *

* After loop exits: x is still free = 10 *
```

Execution path: **NA → bound**

The loop acts as a **temporary sandbox** (execution container). Variables mutated inside are isolated. When the loop exits, bound values are destroyed and free values are restored.

---

## Why This Matters

This system formalizes three concepts that most languages leave implicit:

| Concept | AC term |
|---------|---------|
| Persistent shared state | `free` |
| Temporary execution state | `bound` |
| Declared but not running | `NA` |

### Storage class analogy (for C programmers)

| C | AC |
|---|----|
| `extern` / global | `free` |
| `auto` / stack local | `bound` |
| Forward declaration | `NA` |

---

## Context-sensitive scope promotion

The same function can behave differently depending on *where* it's called:

```ac
Make mutate func()
    x = 99

mutate()            * NA → free:  x persists as 99 *

for i = 1 to 3
    mutate()        * NA → bound: x is 99 only inside loop, reverts after *
```

This makes loops true **execution containers** — they create an isolated scope for state changes, preventing loop-internal mutations from polluting global state.

---

## Transition table

| Context | Before call | After call | After exit |
|---------|------------|------------|------------|
| Global scope | NA | free | persists |
| Loop body | NA | bound | destroyed |
| Nested loop | NA | bound (inner) | restored to outer bound |

---

## Design notes

- `free` variables are analogous to Python module-level globals
- `bound` variables behave like they live on a loop-local stack frame
- `NA` is not "undefined" — the function symbol is registered, only execution state is absent
- A variable can exist in both free and bound states simultaneously across call sites

---

*This scoping model was designed for AC by Abu. Implementation target: post-v0.2.*
