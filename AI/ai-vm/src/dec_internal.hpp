#pragma once
#include "../include/dec.hpp"

// Internal heap object; not exposed outside ai-vm/src/
struct DecObject {
    int      refcount = 1;
    bool     negative = false;
    DecValue digits;               // exponent → digit (0..9), sparse
};

static inline DecObject* deref(DecRef r) { return static_cast<DecObject*>(r); }
