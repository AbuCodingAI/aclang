.code16
.global _start

_start:
    /* Clear registers and set data segments */
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    
    /* Set up our initial stack pointer location */
    mov $0x7C00, %sp
    mov %sp, %bp

    /* Print a tiny BIOS hardware status message */
    mov $0x0E, %ah
    mov $'A', %al; int $0x10
    mov $'O', %al; int $0x10
    mov $'S', %al; int $0x10
    mov $' ', %al; int $0x10

    /* Transition directly to our compiled AC+ mainloop */
    jmp __ai_main__

.org 510
.word 0xAA55
