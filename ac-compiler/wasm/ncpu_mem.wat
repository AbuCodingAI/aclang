;; native-cpu ilib, JS backend: real byte-addressable WASM linear memory instead of the
;; old Map/Buffer-based simulation of a heap. A simple bump allocator (matches the ilib's
;; own arena-allocator philosophy — dha/zdha/arena_alloc were never meant to free
;; individual objects, only reset() the whole region) with automatic growth.
;;
;; Offset 0 is reserved (kept unused) so a real allocation's pointer is never 0 — lets the
;; JS-side ptr registry keep using 0/NULL_PTR-style sentinels safely.
(module
  (memory (export "memory") 16 65536)
  (global $bump (mut i32) (i32.const 8))

  (func (export "alloc") (param $size i32) (result i32)
    (local $ptr i32)
    (local.set $ptr (global.get $bump))
    (global.set $bump (i32.add (global.get $bump) (local.get $size)))
    (if (i32.gt_u (global.get $bump) (i32.mul (memory.size) (i32.const 65536)))
      (then (drop (memory.grow
        (i32.add (i32.div_u (local.get $size) (i32.const 65536)) (i32.const 1))))))
    (local.get $ptr))

  (func (export "reset")
    (global.set $bump (i32.const 8)))

  (func (export "load8") (param $ptr i32) (result i32)
    (i32.load8_u (local.get $ptr)))
  (func (export "store8") (param $ptr i32) (param $val i32)
    (i32.store8 (local.get $ptr) (local.get $val)))
  (func (export "load32") (param $ptr i32) (result i32)
    (i32.load (local.get $ptr)))
  (func (export "store32") (param $ptr i32) (param $val i32)
    (i32.store (local.get $ptr) (local.get $val)))
)
