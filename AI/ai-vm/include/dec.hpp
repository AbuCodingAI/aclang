#pragma once
#include <map>
#include <cstdint>

// dec: exact decimal arithmetic.
// A dec value is a sparse map from base-10 exponent (int) to digit (0..9).
//   42.7  →  { 1:4,  0:2,  -1:7 }
//   0.1   →  { -1:1 }
//   314   →  { 2:3,  1:1,  0:4 }
// Keys are plain integers — no floats anywhere in the representation.
using DecValue = std::map<int, int>;

// Opaque handle into the dec heap (reference-counted)
using DecRef = void*;
#define DEC_NULL nullptr

// ── Lifecycle ──────────────────────────────────────────────────────────────
DecRef dec_from_literal(const char* s);   // parse "3.14", "42", "-0.5"
DecRef dec_from_int(int64_t n);
DecRef dec_retain(DecRef r);              // increment refcount; returns r
void   dec_release(DecRef r);             // decrement; free when count hits 0

// ── Arithmetic (return new ref; caller must dec_release) ───────────────────
DecRef dec_add(DecRef a, DecRef b);
DecRef dec_sub(DecRef a, DecRef b);
DecRef dec_mul(DecRef a, DecRef b);
DecRef dec_div(DecRef a, DecRef b, int precision = 20);

// ── Comparison / predicates ────────────────────────────────────────────────
int dec_cmp(DecRef a, DecRef b);          // -1 / 0 / +1
int dec_is_zero(DecRef r);

// ── Output ─────────────────────────────────────────────────────────────────
void        dec_display(DecRef r);                         // prints value + newline
const char* dec_to_cstr(DecRef r, char* buf, int bufsz);  // writes into buf, no newline
