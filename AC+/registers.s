/* ============================================================
   AC+ Register Reference — x86-64 Alias Table
   Include this file in hand-written .s companions with:
       .include "registers.s"

   The acp compiler resolves these names internally;
   this file is a human-readable reference and can be used
   as a GAS include for mixed hand-written / compiled code.
   ============================================================ */

    .set r64box1,       0   /* %rdi  — 1st param / main video data stream    */
    .set r64box2,       1   /* %rsi  — 2nd param / secondary stream          */
    .set r64box3,       2   /* %rdx  — 3rd param / register allocator pool   */
    .set r64box4,       3   /* %rcx  — 4th param / register allocator pool   */
    .set r64box5,       4   /* %r8   — 5th param / register allocator pool   */
    .set r64box6,       5   /* %r9   — 6th param / register allocator pool   */
    .set r64vault,      6   /* %rax  — math accumulator / return value slot  */
    .set r64base,       7   /* %rbx  — base pointer / general scratchpad     */
    .set r64stack_pin,  8   /* %rsp  — stack pointer                         */
    .set r64frame_pin,  9   /* %rbp  — frame pointer / local variable base   */
    .set r64scratch1,  10   /* %r10  — caller-saved temporary                */
    .set r64scratch2,  11   /* %r11  — caller-saved temporary                */
    .set r64saved1,    12   /* %r12  — callee-saved permanent slot           */
    .set r64saved2,    13   /* %r13  — callee-saved permanent slot           */
    .set r64saved3,    14   /* %r14  — callee-saved permanent slot           */
    .set r64saved4,    15   /* %r15  — callee-saved permanent slot           */

/* ── Calling convention (Linux x86-64 System V ABI) ──────────────────────────
   Arguments (left to right):  64box1 64box2 64box3 64box4 64box5 64box6
                                %rdi   %rsi   %rdx   %rcx   %r8    %r9
   Return value:                64vault / %rax
   Callee-saved:                64base 64saved1..4 64frame_pin 64stack_pin
                                %rbx   %r12..%r15  %rbp        %rsp
   Caller-saved (scratch):      64scratch1 64scratch2  (and all box registers)
   ────────────────────────────────────────────────────────────────────────── */

/* ── Full alias map (AC+ name → real register) ───────────────────────────────
   64-bit              32-bit
   64box1   = %rdi     32box1   = %edi
   64box2   = %rsi     32box2   = %esi
   64box3   = %rdx     32box3   = %edx
   64box4   = %rcx     32box4   = %ecx
   64box5   = %r8      32box5   = %r8d
   64box6   = %r9      32box6   = %r9d
   64vault  = %rax     32vault  = %eax
   64base   = %rbx     32base   = %ebx
   64stack_pin = %rsp  32stack_pin = %esp
   64frame_pin = %rbp  32frame_pin = %ebp
   64scratch1  = %r10  32scratch1  = %r10d
   64scratch2  = %r11  32scratch2  = %r11d
   64saved1    = %r12  32saved1    = %r12d
   64saved2    = %r13  32saved2    = %r13d
   64saved3    = %r14  32saved3    = %r14d
   64saved4    = %r15  32saved4    = %r15d
   ────────────────────────────────────────────────────────────────────────── */
