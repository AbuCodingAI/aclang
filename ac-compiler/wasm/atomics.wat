;; AC `atomic` vars on the JS backend: real WASM atomic load/store on linear memory, one
;; i32 cell per atomic variable (offset assigned at compile time by ir_codegen.cpp).
;; JS/Node is single-threaded and run-to-completion, so nothing can interleave between a
;; load and a store within one statement — the read-modify-write as a whole doesn't need
;; a single fused hardware RMW instruction to stay correct here (the existing
;; LOCK_BEGIN/LOCK_END brackets are already no-ops on this backend for exactly that
;; reason). What this module gives is genuine atomic *memory access* — a real
;; i32.atomic.load/store, not a plain array read/write.
;;
;; Deliberately NOT `shared` memory: the WASM threads spec allows atomic instructions on
;; plain memory too (verified: V8 accepts it), and `shared` bought nothing here — nothing
;; else ever touches this instance's memory, there's no second agent to share with — while
;; costing a MUCH larger virtual-address reservation under V8 (confirmed the hard way:
;; `WebAssembly.Instance()` threw "Out of memory: wasm memory" under the playground
;; sandbox's --rlimit-as=512MB even at limits up to 4GB, purely from the `shared` flag;
;; plain memory needs none of that reservation).
;;
;; `store` returns the stored value (not just void) so the JS codegen can do the WASM
;; atomic write and update its own mirror variable in one expression — `let counter =
;; _ac_atomic_store(offset, val);` — without evaluating `val` a second time.
(module
  (memory (export "memory") 1 1)
  (func (export "load") (param $ptr i32) (result i32)
    (i32.atomic.load (local.get $ptr)))
  (func (export "store") (param $ptr i32) (param $val i32) (result i32)
    (i32.atomic.store (local.get $ptr) (local.get $val))
    (local.get $val))
)
