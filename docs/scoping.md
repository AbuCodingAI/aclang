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

## The `free` keyword — three forms

`free` is the **override**: it forces *free* (global/persistent) behaviour even where the
surrounding context would otherwise make something `bound`. It has three syntactic forms.

### Form 1 — declaration: `free x, y, z`

Inside a bound context (a loop, or a function body), declare that these names refer to the
**free** versions, not bound copies. Reads see the free value; writes persist after the
container exits. (Analogous to Python's `global x`.)

```ac
FOR i in range 10
    free x, y, z
    result = x @ y @ z      /* reads the free x, y, z */
    IF i is 9
        return result
```

### Form 2 — assignment: `free var = value`

Create or assign a variable directly in the **free** scope, regardless of where the statement
appears. Equivalent to `free var` followed by `var = value`, in one statement.

```ac
FOR i in range 10
    free total = total + i  /* mutates the free `total`, survives the loop */
```

### Form 3 — call override: `free f()`

Call `f` in a **free execution context**. Even if the call site is inside a loop (which would
normally make `f`'s mutations bound), `free f()` promotes its writes to the free scope.

```ac
FOR i in range 3
    free mutate()           /* mutate()'s writes to free vars persist past the loop */
```

| Form | Syntax | Effect |
|------|--------|--------|
| Declaration | `free a, b, c` | names bind to free scope inside this container |
| Assignment | `free a = expr` | write `expr` to free `a` |
| Call override | `free f(args)` | run `f` so its mutations land in free scope |

---

## Default behaviour (no `free`)

| Where the write happens | Default state | After container exits |
|-------------------------|---------------|-----------------------|
| `<mainloop>`, depth 0 | free | persists |
| Loop body | bound | reverted to pre-loop free value |
| Function called outside a loop | free (promotes caller state) | persists |
| Function called inside a loop | bound | reverted |

The mental model: **a loop is a sandbox.** Writes inside it are scratch copies (`bound`) and are
thrown away on exit — *unless* `free` opts them back into the persistent scope.

---

## Design notes

- `free` variables are analogous to Python module-level globals
- `bound` variables behave like they live on a loop-local stack frame
- `NA` is not "undefined" — the function symbol is registered, only execution state is absent
- A variable can exist in both free and bound states simultaneously across call sites

---

## Current implementation status (2026-06-20)

Partially implemented. See `scoping_implementation_plan.md` for the full gap analysis and plan.

| Capability | Status |
|------------|--------|
| `free x, y, z` declaration (Form 1) | ✅ parsed → `FREE_DECL` IR op |
| Loop bound-isolation (save/restore around loops) | ✅ via `emitScopeEnter`/`emitScopeExit` |
| `free`-declared vars exempt from loop revert | ✅ `freeVarNames_` exemption |
| `free var = value` assignment (Form 2) | ❌ not parsed |
| `free f()` call override (Form 3) | ❌ not parsed |
| Function write → caller free scope (NA→free, rule 2) | ⚠️ regressed by corruption fix; needs reference-aware reinstatement |
| Context-sensitive NA→free vs NA→bound per call site | ❌ not modelled |
| Save/restore replaced by real scope frames | ❌ still string-prefix hack |

**Fixed 2026-06-20:** removed a broken path (`ir_codegen.cpp` ~6726) that marked *every*
mainloop variable as a global, forcing every function to save/restore variables it never used
and that weren't in scope — the cause of the corrupted BNY output (`140736012014688`).

---

*This scoping model was designed for AC by Abu. Implementation target: post-v0.2.*
