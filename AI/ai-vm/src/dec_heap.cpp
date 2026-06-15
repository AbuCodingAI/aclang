#include "dec_internal.hpp"

DecRef dec_retain(DecRef r) {
    if (r) deref(r)->refcount++;
    return r;
}

void dec_release(DecRef r) {
    if (!r) return;
    if (--deref(r)->refcount <= 0) delete deref(r);
}
