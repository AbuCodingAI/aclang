# AC Language — Examples

## What's Here Now

| File | Target | What it shows |
|------|--------|---------------|
| `hello-world.ac` | PY | Minimal program, `Term.display`, `<mainloop>` |
| `benchmark.ac` | BNY | Functions, recursion, loops, all four arithmetic ops |
| `operators.ac` | PY | Every AC operator: `|` `#|` `#` `&` `#>` `#<` |
| `compound_assign.ac` | PY | Compound assignment operators: `|=` |
| `pong.ac` | C++ | Vision example — shows the high-level game API AC is designed for |

---

## Examples I Want to Write

### Algorithms

| File | Target | Purpose |
|------|--------|---------|
| `fizzbuzz.ac` | PY | Classic FizzBuzz — `WHILST`, `is`, `#=`, `Term.display` |
| `primes.ac` | PY | Sieve of Eratosthenes — nested loops, list indexing |
| `gcd.ac` | PY | Euclidean GCD — showcases `xsub` and WHILST in a tight loop |
| `bubble_sort.ac` | GO | Sorting a list in-place — FOR, index assignment |
| `binary_search.ac` | PY | Search through a sorted list — `#>` `#<`, return mid |
| `collatz.ac` | PY | Collatz conjecture — conditional branching on odd/even |

### Language Feature Demos

| File | Target | Purpose |
|------|--------|---------|
| `all_operators.ac` | PY | Every operator in one program with labelled output |
| `for_loop.ac` | PY | FOR loop over a list, sum and max |
| `nested_functions.ac` | PY | Functions calling functions, closures via return value |
| `recursion.ac` | BNY | Tower of Hanoi — deep recursion in native binary |
| `strings.ac` | JS | String ops: `$...$` literals, display, concatenation |
| `floats.ac` | PY | Float arithmetic, `PosDec`/`NegDec` type inference |
| `boolean_logic.ac` | PY | Full truth-table demo of `&` `|` `#|` `#` `#>` `#<` |

### Cross-Backend Showcase

| File | Target | Purpose |
|------|--------|---------|
| `hello_all.ac` | `--all` | One source, compiles to all 12 backends |
| `fibonacci_all.ac` | `--all` | Fibonacci that produces identical output on every backend |

### Real Programs

| File | Target | Purpose |
|------|--------|---------|
| `caesar_cipher.ac` | PY | Caesar cipher encode/decode — char arithmetic |
| `number_guess.ac` | PY | Guess-the-number game loop — `Term.ask`, `WHILST` |
| `calculator.ac` | JS | Four-function calculator with error handling |
| `matrix_mul.ac` | RS | 3×3 matrix multiply — nested FOR, index ops |

### Native Binary (BNY only)

| File | Target | Purpose |
|------|--------|---------|
| `counter.ac` | BNY | Simple counter — demonstrates `-g` DWARF output |
| `factorial_bny.ac` | BNY | Factorial to verify BNY matches PY/JS output exactly |
| `linked_ops.ac` | BNY | Many chained arithmetic ops — shows register allocator working |

---

## AC Syntax Quick Reference

```
* this is a comment *

AC->PY               * set compile target *

Make name func(a, b) * function definition *
    return a + b

<mainloop>           * entry point (open and close tag are the same) *
    x = 10
    WHILST x #> 0
        Term.display x
        x -= 1
    /kill
<mainloop>

FOR item in myList   * for loop *
    Term.display item

IF cond              * if / else-if / else *
    ...
ELSEIF other
    ...
OTHER
    ...
```

| Concept | AC syntax |
|---------|-----------|
| Equality | `is` |
| Not-equal | `#=` |
| Multiply | `@` |
| AND | `&` or `and` |
| OR | `or` |
| NOT | `#` |
| XOR | `\|` |
| XNOR | `#\|` |
| Not-greater-than (≤) | `#>` |
| Not-less-than (≥) | `#<` |
| Inclusive range count | `xsub` |
| String literal | `$...$` |
| Display | `Term.display value` |
