	.file	"benchmark_suite.cpp"
	.text
#APP
	.globl _ZSt21ios_base_library_initv
#NO_APP
	.section	.text._ZNKSt5ctypeIcE8do_widenEc,"axG",@progbits,_ZNKSt5ctypeIcE8do_widenEc,comdat
	.align 2
	.p2align 4
	.weak	_ZNKSt5ctypeIcE8do_widenEc
	.type	_ZNKSt5ctypeIcE8do_widenEc, @function
_ZNKSt5ctypeIcE8do_widenEc:
.LFB1810:
	.cfi_startproc
	endbr64
	movl	%esi, %eax
	ret
	.cfi_endproc
.LFE1810:
	.size	_ZNKSt5ctypeIcE8do_widenEc, .-_ZNKSt5ctypeIcE8do_widenEc
	.text
	.p2align 4
	.globl	_Z3fibl
	.type	_Z3fibl, @function
_Z3fibl:
.LFB3271:
	.cfi_startproc
	endbr64
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movq	%rdi, %r14
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$200, %rsp
	.cfi_def_cfa_offset 256
	cmpq	$1, %rdi
	jle	.L39
	leaq	-1(%rdi), %rdx
	movq	%rdi, %rbx
	xorl	%ebp, %ebp
	movq	%rdx, %rax
	andq	$-2, %rax
	subq	%rax, %rbx
	movq	%rdx, %rax
	movq	%rbx, %r13
	movq	%rbp, %rbx
	movq	%rdi, %rbp
	cmpq	%r13, %rbp
	je	.L49
.L5:
	subq	$2, %rbp
	movq	%rax, %r14
	xorl	%r15d, %r15d
	movq	%r13, %rsi
	movq	%rbp, %rdx
	movq	%rbx, 72(%rsp)
	movq	%r15, %r13
	movq	%rsi, %r15
	andq	$-2, %rdx
	subq	%rdx, %r14
	movq	%r14, %rbx
.L11:
	leaq	-1(%rax), %rcx
	movq	%rcx, %rdx
	cmpq	%rbx, %rax
	je	.L50
.L8:
	subq	$2, %rax
	movq	%rdx, %rsi
	xorl	%r12d, %r12d
	movq	%rbp, %r14
	movq	%rax, %rcx
	movq	%rax, 80(%rsp)
	andq	$-2, %rcx
	subq	%rcx, %rsi
	movq	%rsi, 128(%rsp)
.L15:
	leaq	-1(%rdx), %rcx
	movq	%rcx, %r8
	cmpq	%rdx, 128(%rsp)
	je	.L51
	leaq	-2(%rdx), %rcx
	movq	%r8, %rax
	movq	%rbx, 96(%rsp)
	xorl	%ebp, %ebp
	movq	%rcx, %rdx
	movq	%rcx, 104(%rsp)
	andq	$-2, %rdx
	movq	%r12, 88(%rsp)
	movq	%r14, %r12
	movq	%rbp, %r14
	subq	%rdx, %rax
	movq	%r15, %rbp
	movq	%r13, %r15
	movq	%rax, 120(%rsp)
.L19:
	leaq	-1(%r8), %rdx
	movq	%rdx, %r11
	cmpq	%r8, 120(%rsp)
	je	.L52
	subq	$2, %r8
	movq	%r11, %rbx
	movq	%r14, %rcx
	xorl	%r13d, %r13d
	movq	%r8, %rdx
	movq	%r8, 112(%rsp)
	movq	%r15, %rsi
	andq	$-2, %rdx
	subq	%rdx, %rbx
	movq	%rbp, %rdx
	movq	%rbx, %r14
	movq	%r12, %rbx
.L23:
	leaq	-1(%r11), %rdi
	movq	%rdi, %rax
	cmpq	%r11, %r14
	je	.L53
.L20:
	leaq	-2(%r11), %r12
	movq	%rax, %r10
	xorl	%r15d, %r15d
	movq	%r12, %rdi
	movq	%r12, 8(%rsp)
	andq	$-2, %rdi
	subq	%rdi, %r10
	movq	%r10, 24(%rsp)
	.p2align 4,,10
	.p2align 3
.L27:
	leaq	-1(%rax), %rdi
	movq	%rdi, %rbp
	cmpq	%rax, 24(%rsp)
	je	.L54
	leaq	-2(%rax), %r11
	leaq	-5(%rax), %r9
	leaq	-3(%rax), %r12
	leaq	-4(%rax), %r8
	movq	%r11, %rax
	andq	$-2, %rax
	movq	%r8, %r10
	subq	%rax, %rdi
	xorl	%eax, %eax
	movq	%rdi, 56(%rsp)
	movq	%r9, %rdi
	movq	%r12, %r9
.L31:
	movq	56(%rsp), %r8
	cmpq	%r8, %rbp
	je	.L55
	movq	%r9, %r8
	leaq	-4(%rbp), %r12
	movq	%rsi, 32(%rsp)
	movq	%rdi, %rsi
	andq	$-2, %r8
	movq	%rcx, 40(%rsp)
	movq	%rax, %rcx
	subq	%r8, %r12
	movq	%r10, %r8
	movq	%rdx, 48(%rsp)
	andq	$-2, %r8
	movq	%r12, 16(%rsp)
	leaq	-6(%rbp), %r12
	subq	%r8, %r12
	movq	$0, (%rsp)
	movq	%rbx, %r8
	movq	%r12, 64(%rsp)
	movq	%rdi, %r12
	leaq	2(%r12), %rdx
	cmpq	%r12, 16(%rsp)
	je	.L56
.L37:
	xorl	%ebx, %ebx
.L32:
	leaq	-1(%rdx), %rdi
	movq	%r11, 184(%rsp)
	movq	%r8, 176(%rsp)
	movq	%r9, 168(%rsp)
	movq	%rsi, 160(%rsp)
	movq	%r10, 152(%rsp)
	movq	%rcx, 144(%rsp)
	movq	%rdx, 136(%rsp)
	call	_Z3fibl
	movq	136(%rsp), %rdx
	movq	144(%rsp), %rcx
	addq	%rax, %rbx
	movq	152(%rsp), %r10
	movq	160(%rsp), %rsi
	subq	$2, %rdx
	movq	168(%rsp), %r9
	movq	176(%rsp), %r8
	cmpq	$1, %rdx
	movq	184(%rsp), %r11
	jg	.L32
	movq	%r12, %rax
	andl	$1, %eax
	addq	%rbx, %rax
	addq	%rax, (%rsp)
	leaq	-2(%r12), %rax
	cmpq	%rax, 64(%rsp)
	je	.L34
	movq	%rax, %r12
	leaq	2(%r12), %rdx
	cmpq	%r12, 16(%rsp)
	jne	.L37
.L56:
	movq	(%rsp), %r12
	movq	%rcx, %rax
	movq	%rsi, %rdi
	movq	%r8, %rbx
	movq	40(%rsp), %rcx
	movq	%rdx, %r8
	movq	32(%rsp), %rsi
	movq	48(%rsp), %rdx
	addq	%r12, %r8
.L33:
	subq	$2, %rbp
	addq	%r8, %rax
	subq	$2, %r9
	subq	$2, %rdi
	subq	$2, %r10
	cmpq	$1, %rbp
	jne	.L31
	leaq	1(%rax), %r10
	movq	%r11, %rax
	addq	%r10, %r15
	cmpq	$1, %r11
	jne	.L27
	movq	8(%rsp), %r12
	leaq	1(%r15), %rdi
.L59:
	movq	%r12, %r11
	addq	%rdi, %r13
	cmpq	$1, %r12
	jne	.L23
.L58:
	movq	112(%rsp), %r8
	movq	%rsi, %r15
	movq	%rcx, %r14
	movq	%rdx, %rbp
	movq	%rbx, %r12
	leaq	1(%r13), %rdi
.L21:
	addq	%rdi, %r14
	cmpq	$1, %r8
	jne	.L19
	movq	%r15, %r13
	movq	96(%rsp), %rbx
	movq	%rbp, %r15
	movq	104(%rsp), %rcx
	movq	%r14, %rbp
	movq	%r12, %r14
	movq	88(%rsp), %r12
	leaq	1(%rbp), %rdi
.L17:
	movq	%rcx, %rdx
	addq	%rdi, %r12
	cmpq	$1, %rcx
	jne	.L15
	movq	80(%rsp), %rax
	leaq	1(%r12), %rdx
	movq	%r14, %rbp
	addq	%rdx, %r13
	cmpq	$1, %rax
	jne	.L11
.L60:
	movq	%r15, %rax
	movq	72(%rsp), %rbx
	movq	%r13, %r15
	movq	%rax, %r13
	leaq	1(%r15), %rax
	addq	%rax, %rbx
	cmpq	$1, %rbp
	jne	.L57
.L40:
	leaq	1(%rbx), %r14
	jmp	.L39
	.p2align 4,,10
	.p2align 3
.L54:
	movq	8(%rsp), %r12
	addq	%r15, %rdi
	addq	%rdi, %r13
	movq	%r12, %r11
	cmpq	$1, %r12
	je	.L58
	leaq	-1(%r11), %rdi
	movq	%rdi, %rax
	cmpq	%r11, %r14
	jne	.L20
.L53:
	movq	112(%rsp), %r8
	movq	%rsi, %r15
	movq	%rcx, %r14
	movq	%rdx, %rbp
	movq	%rbx, %r12
	addq	%r13, %rdi
	jmp	.L21
	.p2align 4,,10
	.p2align 3
.L55:
	leaq	-1(%rbp,%rax), %r10
	movq	%r11, %rax
	addq	%r10, %r15
	cmpq	$1, %r11
	jne	.L27
	movq	8(%rsp), %r12
	leaq	1(%r15), %rdi
	jmp	.L59
.L52:
	movq	%r15, %r13
	movq	%rbp, %r15
	movq	%r14, %rbp
	movq	96(%rsp), %rbx
	movq	%r12, %r14
	movq	104(%rsp), %rcx
	movq	88(%rsp), %r12
	leaq	(%rdx,%rbp), %rdi
	jmp	.L17
	.p2align 4,,10
	.p2align 3
.L34:
	movq	%r8, %rbx
	movq	(%rsp), %r8
	movq	%rcx, %rax
	movq	%rsi, %rdi
	movq	40(%rsp), %rcx
	movq	32(%rsp), %rsi
	movq	48(%rsp), %rdx
	leaq	1(%r8,%r12), %r8
	jmp	.L33
.L51:
	movq	80(%rsp), %rax
	leaq	(%rcx,%r12), %rdx
	movq	%r14, %rbp
	addq	%rdx, %r13
	cmpq	$1, %rax
	je	.L60
	leaq	-1(%rax), %rcx
	movq	%rcx, %rdx
	cmpq	%rbx, %rax
	jne	.L8
.L50:
	movq	%r15, %rax
	movq	72(%rsp), %rbx
	movq	%r13, %r15
	movq	%rax, %r13
	leaq	(%rcx,%r15), %rax
	addq	%rax, %rbx
	cmpq	$1, %rbp
	je	.L40
.L57:
	leaq	-1(%rbp), %rdx
	movq	%rdx, %rax
	cmpq	%r13, %rbp
	jne	.L5
.L49:
	leaq	(%rdx,%rbx), %r14
.L39:
	addq	$200, %rsp
	.cfi_def_cfa_offset 56
	movq	%r14, %rax
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE3271:
	.size	_Z3fibl, .-_Z3fibl
	.p2align 4
	.globl	_Z7sum_arrRKSt6vectorIlSaIlEE
	.type	_Z7sum_arrRKSt6vectorIlSaIlEE, @function
_Z7sum_arrRKSt6vectorIlSaIlEE:
.LFB3272:
	.cfi_startproc
	endbr64
	movq	(%rdi), %rax
	movq	8(%rdi), %rcx
	xorl	%edx, %edx
	cmpq	%rax, %rcx
	je	.L61
	.p2align 4,,10
	.p2align 3
.L63:
	addq	(%rax), %rdx
	addq	$8, %rax
	cmpq	%rcx, %rax
	jne	.L63
.L61:
	movq	%rdx, %rax
	ret
	.cfi_endproc
.LFE3272:
	.size	_Z7sum_arrRKSt6vectorIlSaIlEE, .-_Z7sum_arrRKSt6vectorIlSaIlEE
	.p2align 4
	.globl	_Z4meanRKSt6vectorIlSaIlEE
	.type	_Z4meanRKSt6vectorIlSaIlEE, @function
_Z4meanRKSt6vectorIlSaIlEE:
.LFB3273:
	.cfi_startproc
	endbr64
	movq	8(%rdi), %rcx
	movq	(%rdi), %rsi
	cmpq	%rcx, %rsi
	je	.L71
	movq	%rsi, %rax
	xorl	%edx, %edx
	.p2align 4,,10
	.p2align 3
.L68:
	addq	(%rax), %rdx
	addq	$8, %rax
	cmpq	%rcx, %rax
	jne	.L68
	subq	%rsi, %rax
	pxor	%xmm0, %xmm0
	sarq	$3, %rax
	cvtsi2sdq	%rdx, %xmm0
	js	.L69
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rax, %xmm1
	divsd	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L69:
	movq	%rax, %rdx
	andl	$1, %eax
	pxor	%xmm1, %xmm1
	shrq	%rdx
	orq	%rax, %rdx
	cvtsi2sdq	%rdx, %xmm1
	addsd	%xmm1, %xmm1
	divsd	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L71:
	pxor	%xmm0, %xmm0
	ret
	.cfi_endproc
.LFE3273:
	.size	_Z4meanRKSt6vectorIlSaIlEE, .-_Z4meanRKSt6vectorIlSaIlEE
	.p2align 4
	.globl	_Z8distanceRKSt6vectorIlSaIlEE
	.type	_Z8distanceRKSt6vectorIlSaIlEE, @function
_Z8distanceRKSt6vectorIlSaIlEE:
.LFB3274:
	.cfi_startproc
	endbr64
	movq	8(%rdi), %r8
	movq	(%rdi), %rcx
	cmpq	%rcx, %r8
	je	.L76
	movq	(%rcx), %rsi
	movq	%rsi, %rax
	.p2align 4,,10
	.p2align 3
.L75:
	movq	(%rcx), %rdx
	cmpq	%rdx, %rsi
	cmovg	%rdx, %rsi
	cmpq	%rdx, %rax
	cmovl	%rdx, %rax
	addq	$8, %rcx
	cmpq	%rcx, %r8
	jne	.L75
	subq	%rsi, %rax
	ret
	.p2align 4,,10
	.p2align 3
.L76:
	xorl	%eax, %eax
	ret
	.cfi_endproc
.LFE3274:
	.size	_Z8distanceRKSt6vectorIlSaIlEE, .-_Z8distanceRKSt6vectorIlSaIlEE
	.p2align 4
	.globl	_Z6stddevRKSt6vectorIlSaIlEE
	.type	_Z6stddevRKSt6vectorIlSaIlEE, @function
_Z6stddevRKSt6vectorIlSaIlEE:
.LFB3275:
	.cfi_startproc
	endbr64
	movq	8(%rdi), %rsi
	movq	(%rdi), %r9
	cmpq	%rsi, %r9
	je	.L84
	movq	%r9, %rax
	movq	%r9, %rdx
	xorl	%r8d, %r8d
	.p2align 4,,10
	.p2align 3
.L80:
	movq	(%rdx), %rdi
	movq	%rdx, %rcx
	leaq	8(%rdx), %rdx
	addq	%r8, %rdi
	movq	%rdi, %r8
	cmpq	%rdx, %rsi
	jne	.L80
	subq	%r9, %rsi
	sarq	$3, %rsi
	js	.L81
	pxor	%xmm3, %xmm3
	cvtsi2sdq	%rsi, %xmm3
.L82:
	pxor	%xmm2, %xmm2
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rdi, %xmm2
	divsd	%xmm3, %xmm2
	.p2align 4,,10
	.p2align 3
.L83:
	pxor	%xmm1, %xmm1
	movq	%rax, %rdx
	addq	$8, %rax
	cvtsi2sdq	-8(%rax), %xmm1
	subsd	%xmm2, %xmm1
	mulsd	%xmm1, %xmm1
	addsd	%xmm1, %xmm0
	cmpq	%rdx, %rcx
	jne	.L83
	divsd	%xmm3, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L81:
	movq	%rsi, %rdx
	andl	$1, %esi
	pxor	%xmm3, %xmm3
	shrq	%rdx
	orq	%rsi, %rdx
	cvtsi2sdq	%rdx, %xmm3
	addsd	%xmm3, %xmm3
	jmp	.L82
	.p2align 4,,10
	.p2align 3
.L84:
	pxor	%xmm0, %xmm0
	ret
	.cfi_endproc
.LFE3275:
	.size	_Z6stddevRKSt6vectorIlSaIlEE, .-_Z6stddevRKSt6vectorIlSaIlEE
	.p2align 4
	.globl	_Z9loop_workv
	.type	_Z9loop_workv, @function
_Z9loop_workv:
.LFB3293:
	.cfi_startproc
	endbr64
	movl	$4950, %eax
	ret
	.cfi_endproc
.LFE3293:
	.size	_Z9loop_workv, .-_Z9loop_workv
	.p2align 4
	.globl	_Z9array_opsv
	.type	_Z9array_opsv, @function
_Z9array_opsv:
.LFB3294:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movl	$80, %edi
	pushq	%rbx
	.cfi_def_cfa_offset 24
	.cfi_offset 3, -24
	subq	$152, %rsp
	.cfi_def_cfa_offset 176
	movdqa	.LC1(%rip), %xmm0
	movq	%fs:40, %rax
	movq	%rax, 136(%rsp)
	xorl	%eax, %eax
	movaps	%xmm0, 48(%rsp)
	movdqa	.LC2(%rip), %xmm0
	movaps	%xmm0, 64(%rsp)
	movdqa	.LC3(%rip), %xmm0
	movaps	%xmm0, 80(%rsp)
	movdqa	.LC4(%rip), %xmm0
	movaps	%xmm0, 96(%rsp)
	movdqa	.LC5(%rip), %xmm0
	movaps	%xmm0, 112(%rsp)
	call	_Znwm@PLT
	movdqa	48(%rsp), %xmm2
	xorl	%edx, %edx
	movdqa	64(%rsp), %xmm3
	leaq	80(%rax), %r11
	movdqa	80(%rsp), %xmm5
	movq	%rax, %rbp
	movq	%rax, %r10
	movdqa	96(%rsp), %xmm6
	movdqa	112(%rsp), %xmm7
	movq	%rax, 16(%rsp)
	movq	%r11, 24(%rsp)
	movups	%xmm2, (%rax)
	movups	%xmm3, 16(%rax)
	movups	%xmm5, 32(%rax)
	movups	%xmm6, 48(%rax)
	movups	%xmm7, 64(%rax)
	.p2align 4,,10
	.p2align 3
.L89:
	movq	(%rax), %rbx
	addq	$8, %rax
	addq	%rdx, %rbx
	movq	%rbx, %rdx
	cmpq	%rax, %r11
	jne	.L89
	movq	%rbp, %rax
	xorl	%edx, %edx
	.p2align 4,,10
	.p2align 3
.L90:
	addq	(%rax), %rdx
	addq	$8, %rax
	cmpq	%r11, %rax
	jne	.L90
	pxor	%xmm4, %xmm4
	leaq	16(%rsp), %rdi
	cvtsi2sdq	%rdx, %xmm4
	divsd	.LC6(%rip), %xmm4
	call	_Z6stddevRKSt6vectorIlSaIlEE
	movl	$1, %edx
	movl	$1, %ecx
	movapd	%xmm0, %xmm1
	.p2align 4,,10
	.p2align 3
.L91:
	movq	(%r10), %rax
	cmpq	%rax, %rcx
	cmovg	%rax, %rcx
	cmpq	%rax, %rdx
	cmovl	%rax, %rdx
	addq	$8, %r10
	cmpq	%r11, %r10
	jne	.L91
	pxor	%xmm0, %xmm0
	subq	%rcx, %rdx
	movl	$80, %esi
	movq	%rbp, %rdi
	cvtsi2sdq	%rbx, %xmm0
	addsd	%xmm4, %xmm0
	addsd	%xmm1, %xmm0
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rdx, %xmm1
	addsd	%xmm1, %xmm0
	movsd	%xmm0, 8(%rsp)
	call	_ZdlPvm@PLT
	movsd	8(%rsp), %xmm0
	movq	136(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L97
	addq	$152, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	ret
.L97:
	.cfi_restore_state
	call	__stack_chk_fail@PLT
	.cfi_endproc
.LFE3294:
	.size	_Z9array_opsv, .-_Z9array_opsv
	.section	.text._ZNSt6vectorIiSaIiEED2Ev,"axG",@progbits,_ZNSt6vectorIiSaIiEED5Ev,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt6vectorIiSaIiEED2Ev
	.type	_ZNSt6vectorIiSaIiEED2Ev, @function
_ZNSt6vectorIiSaIiEED2Ev:
.LFB3619:
	.cfi_startproc
	endbr64
	movq	(%rdi), %rax
	testq	%rax, %rax
	je	.L98
	movq	16(%rdi), %rsi
	movq	%rax, %rdi
	subq	%rax, %rsi
	jmp	_ZdlPvm@PLT
	.p2align 4,,10
	.p2align 3
.L98:
	ret
	.cfi_endproc
.LFE3619:
	.size	_ZNSt6vectorIiSaIiEED2Ev, .-_ZNSt6vectorIiSaIiEED2Ev
	.weak	_ZNSt6vectorIiSaIiEED1Ev
	.set	_ZNSt6vectorIiSaIiEED1Ev,_ZNSt6vectorIiSaIiEED2Ev
	.section	.rodata._ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_.str1.1,"aMS",@progbits,1
.LC7:
	.string	"vector::_M_realloc_insert"
	.section	.text._ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_,"axG",@progbits,_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_
	.type	_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_, @function
_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_:
.LFB3852:
	.cfi_startproc
	endbr64
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movq	%rdx, %r15
	movabsq	$2305843009213693951, %rdx
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$24, %rsp
	.cfi_def_cfa_offset 80
	movq	8(%rdi), %r12
	movq	(%rdi), %r13
	movq	%r12, %rax
	subq	%r13, %rax
	sarq	$2, %rax
	cmpq	%rdx, %rax
	je	.L122
	movq	%rsi, %rdx
	movq	%rdi, %rbp
	movq	%rsi, %r14
	subq	%r13, %rdx
	cmpq	%r12, %r13
	je	.L123
	leaq	(%rax,%rax), %rcx
	cmpq	%rax, %rcx
	jb	.L115
	testq	%rcx, %rcx
	jne	.L124
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
.L106:
	movl	(%r15), %eax
	leaq	4(%rcx,%rdx), %r8
	subq	%r14, %r12
	leaq	(%r8,%r12), %r15
	movl	%eax, (%rcx,%rdx)
	testq	%rdx, %rdx
	jg	.L125
	testq	%r12, %r12
	jle	.L110
	movq	%r12, %rdx
	movq	%r14, %rsi
	movq	%r8, %rdi
	movq	%rcx, (%rsp)
	call	memcpy@PLT
	movq	(%rsp), %rcx
.L110:
	testq	%r13, %r13
	jne	.L109
.L112:
	movq	%rcx, 0(%rbp)
	movq	%r15, 8(%rbp)
	movq	%rbx, 16(%rbp)
	addq	$24, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L115:
	.cfi_restore_state
	movabsq	$9223372036854775804, %rbx
.L105:
	movq	%rbx, %rdi
	movq	%rdx, (%rsp)
	call	_Znwm@PLT
	movq	(%rsp), %rdx
	movq	%rax, %rcx
	addq	%rax, %rbx
	jmp	.L106
	.p2align 4,,10
	.p2align 3
.L125:
	movq	%rcx, %rdi
	movq	%r13, %rsi
	movq	%r8, (%rsp)
	call	memmove@PLT
	movq	%rax, %rcx
	testq	%r12, %r12
	jg	.L126
.L109:
	movq	16(%rbp), %rsi
	movq	%r13, %rdi
	movq	%rcx, (%rsp)
	subq	%r13, %rsi
	call	_ZdlPvm@PLT
	movq	(%rsp), %rcx
	jmp	.L112
	.p2align 4,,10
	.p2align 3
.L123:
	addq	$1, %rax
	jc	.L115
	movabsq	$2305843009213693951, %rcx
	cmpq	%rcx, %rax
	movq	%rcx, %rbx
	cmovbe	%rax, %rbx
	salq	$2, %rbx
	jmp	.L105
	.p2align 4,,10
	.p2align 3
.L126:
	movq	(%rsp), %rdi
	movq	%r12, %rdx
	movq	%r14, %rsi
	movq	%rax, 8(%rsp)
	call	memcpy@PLT
	movq	8(%rsp), %rcx
	jmp	.L109
.L124:
	movabsq	$2305843009213693951, %rax
	cmpq	%rax, %rcx
	cmova	%rax, %rcx
	leaq	0(,%rcx,4), %rbx
	jmp	.L105
.L122:
	leaq	.LC7(%rip), %rdi
	call	_ZSt20__throw_length_errorPKc@PLT
	.cfi_endproc
.LFE3852:
	.size	_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_, .-_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_
	.section	.text._ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv,"axG",@progbits,_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv,comdat
	.align 2
	.p2align 4
	.weak	_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv
	.type	_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv, @function
_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv:
.LFB3992:
	.cfi_startproc
	endbr64
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	movq	%rdi, %rbx
	movq	(%rdi), %rdi
	testq	%rdi, %rdi
	je	.L127
	movq	32(%rbx), %rsi
	subq	%rdi, %rsi
	call	_ZdlPvm@PLT
	movq	$0, (%rbx)
	movl	$0, 8(%rbx)
	movq	$0, 16(%rbx)
	movl	$0, 24(%rbx)
	movq	$0, 32(%rbx)
.L127:
	popq	%rbx
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE3992:
	.size	_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv, .-_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv
	.section	.text.unlikely,"ax",@progbits
.LCOLDB8:
	.text
.LHOTB8:
	.p2align 4
	.globl	_Z5sievei
	.type	_Z5sievei, @function
_Z5sievei:
.LFB3276:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA3276
	endbr64
	pushq	%r14
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -16
	pxor	%xmm0, %xmm0
	pushq	%r13
	.cfi_def_cfa_offset 24
	.cfi_offset 13, -24
	pushq	%r12
	.cfi_def_cfa_offset 32
	.cfi_offset 12, -32
	pushq	%rbp
	.cfi_def_cfa_offset 40
	.cfi_offset 6, -40
	movq	%rdi, %rbp
	pushq	%rbx
	.cfi_def_cfa_offset 48
	.cfi_offset 3, -48
	subq	$64, %rsp
	.cfi_def_cfa_offset 112
	movq	%fs:40, %rax
	movq	%rax, 56(%rsp)
	xorl	%eax, %eax
	movq	$0, 16(%rdi)
	movups	%xmm0, (%rdi)
	cmpl	$1, %esi
	jg	.L164
.L133:
	movq	56(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L165
	addq	$64, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 48
	movq	%rbp, %rax
	popq	%rbx
	.cfi_def_cfa_offset 40
	popq	%rbp
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r13
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L164:
	.cfi_restore_state
	movq	$0, 16(%rsp)
	leal	1(%rsi), %r13d
	movl	%esi, %ebx
	movq	$0, 24(%rsp)
	movslq	%r13d, %r14
	movq	$0, 32(%rsp)
	leaq	63(%r14), %r12
	movq	$0, 40(%rsp)
	shrq	$6, %r12
	movq	$0, 48(%rsp)
	salq	$3, %r12
	movq	%r12, %rdi
.LEHB0:
	call	_Znwm@PLT
.LEHE0:
	movq	%rax, %rdi
	sarq	$6, %r14
	leaq	(%rax,%r12), %rax
	andl	$63, %r13d
	movq	%rax, 48(%rsp)
	movl	$-1, %esi
	leaq	(%rdi,%r14,8), %rax
	movq	%r12, %rdx
	movq	%rdi, 16(%rsp)
	movl	$0, 24(%rsp)
	movq	%rax, (%rsp)
	movl	%r13d, 8(%rsp)
	movq	%rax, 32(%rsp)
	movl	%r13d, 40(%rsp)
	call	memset@PLT
	movl	$3, %r9d
	movl	$4, %esi
	movl	$1, %r11d
	movq	%rax, %rdi
	movq	(%rax), %rax
	andq	$-4, %rax
	movq	%rax, (%rdi)
	cmpl	$3, %ebx
	jg	.L135
	jmp	.L136
	.p2align 4,,10
	.p2align 3
.L141:
	movl	%r9d, %esi
	imull	%r9d, %esi
	cmpl	%ebx, %esi
	jg	.L136
.L140:
	movq	%r9, %rax
	addq	$1, %r9
	sarq	$6, %rax
	movq	(%rdi,%rax,8), %rax
.L135:
	leal	-1(%r9), %r10d
	movq	%r11, %rdx
	movl	%r10d, %ecx
	salq	%cl, %rdx
	testq	%rax, %rdx
	je	.L141
	cmpl	%esi, %ebx
	jl	.L141
	.p2align 4,,10
	.p2align 3
.L144:
	movslq	%esi, %rax
	testq	%rax, %rax
	leaq	63(%rax), %rdx
	cmovns	%rax, %rdx
	sarq	$6, %rdx
	leaq	(%rdi,%rdx,8), %r8
	cqto
	shrq	$58, %rdx
	addq	%rdx, %rax
	andl	$63, %eax
	subq	%rdx, %rax
	movl	%eax, %ecx
	jns	.L143
	subq	$8, %r8
	leal	64(%rax), %ecx
.L143:
	movq	%r11, %rax
	addl	%r10d, %esi
	salq	%cl, %rax
	notq	%rax
	andq	%rax, (%r8)
	cmpl	%esi, %ebx
	jge	.L144
	movl	%r9d, %esi
	imull	%r9d, %esi
	cmpl	%ebx, %esi
	jle	.L140
.L136:
	movl	$2, (%rsp)
	movl	$2, %r12d
	movl	$1, %r14d
	jmp	.L149
	.p2align 4,,10
	.p2align 3
.L146:
	addl	$1, %r13d
	addq	$1, %r12
	movl	%r13d, (%rsp)
	cmpl	%r12d, %ebx
	jl	.L166
.L149:
	movq	%r12, %rdx
	movq	%r14, %rax
	movl	%r12d, %ecx
	movl	%r12d, %r13d
	sarq	$6, %rdx
	salq	%cl, %rax
	andq	(%rdi,%rdx,8), %rax
	je	.L146
	movq	8(%rbp), %rsi
	cmpq	16(%rbp), %rsi
	je	.L147
	movl	%r12d, (%rsi)
	addq	$4, %rsi
	movq	%rsi, 8(%rbp)
	jmp	.L146
	.p2align 4,,10
	.p2align 3
.L166:
	testq	%rdi, %rdi
	je	.L133
	movq	48(%rsp), %rsi
	subq	%rdi, %rsi
	call	_ZdlPvm@PLT
	jmp	.L133
	.p2align 4,,10
	.p2align 3
.L147:
	movq	%rsp, %rdx
	movq	%rbp, %rdi
.LEHB1:
	call	_ZNSt6vectorIiSaIiEE17_M_realloc_insertIJRKiEEEvN9__gnu_cxx17__normal_iteratorIPiS1_EEDpOT_
.LEHE1:
	movq	16(%rsp), %rdi
	jmp	.L146
.L165:
	call	__stack_chk_fail@PLT
.L155:
	endbr64
	movq	%rax, %rbx
	jmp	.L151
.L156:
	endbr64
	movq	%rax, %rbx
	jmp	.L151
	.globl	__gxx_personality_v0
	.section	.gcc_except_table,"a",@progbits
.LLSDA3276:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE3276-.LLSDACSB3276
.LLSDACSB3276:
	.uleb128 .LEHB0-.LFB3276
	.uleb128 .LEHE0-.LEHB0
	.uleb128 .L156-.LFB3276
	.uleb128 0
	.uleb128 .LEHB1-.LFB3276
	.uleb128 .LEHE1-.LEHB1
	.uleb128 .L155-.LFB3276
	.uleb128 0
.LLSDACSE3276:
	.text
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDAC3276
	.type	_Z5sievei.cold, @function
_Z5sievei.cold:
.LFSB3276:
.L151:
	.cfi_def_cfa_offset 112
	.cfi_offset 3, -48
	.cfi_offset 6, -40
	.cfi_offset 12, -32
	.cfi_offset 13, -24
	.cfi_offset 14, -16
	leaq	16(%rsp), %rdi
	call	_ZNSt13_Bvector_baseISaIbEE13_M_deallocateEv
	movq	%rbp, %rdi
	call	_ZNSt6vectorIiSaIiEED1Ev
	movq	56(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L167
	movq	%rbx, %rdi
.LEHB2:
	call	_Unwind_Resume@PLT
.LEHE2:
.L167:
	call	__stack_chk_fail@PLT
	.cfi_endproc
.LFE3276:
	.section	.gcc_except_table
.LLSDAC3276:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSEC3276-.LLSDACSBC3276
.LLSDACSBC3276:
	.uleb128 .LEHB2-.LCOLDB8
	.uleb128 .LEHE2-.LEHB2
	.uleb128 0
	.uleb128 0
.LLSDACSEC3276:
	.section	.text.unlikely
	.text
	.size	_Z5sievei, .-_Z5sievei
	.section	.text.unlikely
	.size	_Z5sievei.cold, .-_Z5sievei.cold
.LCOLDE8:
	.text
.LHOTE8:
	.section	.text.unlikely
.LCOLDB9:
	.section	.text.startup,"ax",@progbits
.LHOTB9:
	.p2align 4
	.globl	main
	.type	main, @function
main:
.LFB3295:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA3295
	endbr64
	pushq	%r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	movl	$28, %edi
	pushq	%rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	subq	$32, %rsp
	.cfi_def_cfa_offset 64
	movq	%fs:40, %rax
	movq	%rax, 24(%rsp)
	xorl	%eax, %eax
	movq	%rsp, %r12
	call	_Z3fibl
	movl	$50, %esi
	movq	%r12, %rdi
	movq	%rax, %rbx
.LEHB3:
	call	_Z5sievei
.LEHE3:
.LEHB4:
	call	_Z9array_opsv
	movq	8(%rsp), %rax
	subq	(%rsp), %rax
	movapd	%xmm0, %xmm1
	pxor	%xmm0, %xmm0
	sarq	$2, %rax
	leaq	_ZSt4cout(%rip), %rdi
	leaq	4950(%rbx,%rax), %rax
	cvtsi2sdq	%rax, %xmm0
	addsd	%xmm1, %xmm0
	cvttsd2siq	%xmm0, %rsi
	call	_ZNSo9_M_insertIlEERSoT_@PLT
	movq	%rax, %rbx
	movq	(%rax), %rax
	movq	-24(%rax), %rax
	movq	240(%rbx,%rax), %rbp
	testq	%rbp, %rbp
	je	.L180
	cmpb	$0, 56(%rbp)
	je	.L171
	movsbl	67(%rbp), %esi
.L172:
	movq	%rbx, %rdi
	call	_ZNSo3putEc@PLT
	movq	%rax, %rdi
	call	_ZNSo5flushEv@PLT
	movq	%r12, %rdi
	call	_ZNSt6vectorIiSaIiEED1Ev
	movq	24(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L178
	addq	$32, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 32
	xorl	%eax, %eax
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%rbp
	.cfi_def_cfa_offset 16
	popq	%r12
	.cfi_def_cfa_offset 8
	ret
.L171:
	.cfi_restore_state
	movq	%rbp, %rdi
	call	_ZNKSt5ctypeIcE13_M_widen_initEv@PLT
	movq	0(%rbp), %rax
	leaq	_ZNKSt5ctypeIcE8do_widenEc(%rip), %rdx
	movl	$10, %esi
	movq	48(%rax), %rax
	cmpq	%rdx, %rax
	je	.L172
	movl	$10, %esi
	movq	%rbp, %rdi
	call	*%rax
	movsbl	%al, %esi
	jmp	.L172
.L178:
	call	__stack_chk_fail@PLT
.L180:
	movq	24(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L178
	call	_ZSt16__throw_bad_castv@PLT
.LEHE4:
.L177:
	endbr64
	movq	%rax, %rbx
	jmp	.L173
	.section	.gcc_except_table
.LLSDA3295:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE3295-.LLSDACSB3295
.LLSDACSB3295:
	.uleb128 .LEHB3-.LFB3295
	.uleb128 .LEHE3-.LEHB3
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB4-.LFB3295
	.uleb128 .LEHE4-.LEHB4
	.uleb128 .L177-.LFB3295
	.uleb128 0
.LLSDACSE3295:
	.section	.text.startup
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDAC3295
	.type	main.cold, @function
main.cold:
.LFSB3295:
.L173:
	.cfi_def_cfa_offset 64
	.cfi_offset 3, -32
	.cfi_offset 6, -24
	.cfi_offset 12, -16
	movq	%r12, %rdi
	call	_ZNSt6vectorIiSaIiEED1Ev
	movq	24(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L181
	movq	%rbx, %rdi
.LEHB5:
	call	_Unwind_Resume@PLT
.LEHE5:
.L181:
	call	__stack_chk_fail@PLT
	.cfi_endproc
.LFE3295:
	.section	.gcc_except_table
.LLSDAC3295:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSEC3295-.LLSDACSBC3295
.LLSDACSBC3295:
	.uleb128 .LEHB5-.LCOLDB9
	.uleb128 .LEHE5-.LEHB5
	.uleb128 0
	.uleb128 0
.LLSDACSEC3295:
	.section	.text.unlikely
	.section	.text.startup
	.size	main, .-main
	.section	.text.unlikely
	.size	main.cold, .-main.cold
.LCOLDE9:
	.section	.text.startup
.LHOTE9:
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC1:
	.quad	1
	.quad	2
	.align 16
.LC2:
	.quad	3
	.quad	4
	.align 16
.LC3:
	.quad	5
	.quad	6
	.align 16
.LC4:
	.quad	7
	.quad	8
	.align 16
.LC5:
	.quad	9
	.quad	10
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC6:
	.long	0
	.long	1076101120
	.hidden	DW.ref.__gxx_personality_v0
	.weak	DW.ref.__gxx_personality_v0
	.section	.data.rel.local.DW.ref.__gxx_personality_v0,"awG",@progbits,DW.ref.__gxx_personality_v0,comdat
	.align 8
	.type	DW.ref.__gxx_personality_v0, @object
	.size	DW.ref.__gxx_personality_v0, 8
DW.ref.__gxx_personality_v0:
	.quad	__gxx_personality_v0
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
