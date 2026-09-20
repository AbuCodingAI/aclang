;; AC `short`/`mini` (fixed-width 32/16-bit signed int) wraparound for the JS backend.
;;
;; JS numbers are float64 — plain `x + y` never wraps the way C's int32_t/int16_t do.
;; wrap32 needs no arithmetic at all: the JS<->WASM call boundary itself performs
;; ToInt32 on any number passed as an i32 param (per the WebAssembly JS API spec), so an
;; identity function is already a real 32-bit truncating cast. wrap16 additionally
;; sign-extends from bit 15 (shift up 16, arithmetic-shift back down 16).
(module
  (func (export "wrap32") (param $x i32) (result i32)
    local.get $x)
  (func (export "wrap16") (param $x i32) (result i32)
    (i32.shr_s (i32.shl (local.get $x) (i32.const 16)) (i32.const 16)))
)
