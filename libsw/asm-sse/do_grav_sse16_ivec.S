#define ppos0 -56(%rsp)
#define ppos1 -40(%rsp)
#define ppos2 -24(%rsp)
#define eps2   -8(%rsp)	
#define ma 0(%rdi)
#define xa 16(%rdi)
#define ya 32(%rdi)
#define za 48(%rdi)
#define mb 64(%rdi)
#define xb 80(%rdi)
#define yb 96(%rdi)
#define zb 112(%rdi)
#define mc 128(%rdi)
#define xc 144(%rdi)
#define yc 160(%rdi)
#define zc 176(%rdi)
#define md 192(%rdi)
#define xd 208(%rdi)
#define yd 224(%rdi)
#define zd 240(%rdi)
	.file "do_grav_sse16_ivec.S"
	.text
	.p2align 4,,15
	.globl	do_grav_sse16_ivec_asm
do_grav_sse16_ivec_asm:
.init:
	subq	$256, %rsp
        movq    264(%rsp), %rax
        movss   (%rdx), %xmm0
        movss   4(%rdx), %xmm4
        movss   8(%rdx), %xmm8
	movss   (%rax), %xmm9
        cmpq    %rsi, %rdi
        shufps  $0, %xmm0, %xmm0
        shufps  $0, %xmm4, %xmm4
        shufps  $0, %xmm8, %xmm8
        shufps  $0, %xmm9, %xmm9
        movaps  %xmm0, ppos0
        movaps  %xmm4, ppos1
        movaps  %xmm8, ppos2
        movaps  %xmm9, eps2
	xorps	%xmm12, %xmm12
	movaps	%xmm12, %xmm13
	movaps	%xmm12, %xmm14
	movaps	%xmm12, %xmm15
	jae	.end
	.p2align 4,,15
.loop:
	movaps  ppos0, %xmm0
	movaps  %xmm0, %xmm1
	movaps  %xmm0, %xmm2
	movaps  %xmm0, %xmm3
	movaps	ppos1, %xmm4
	movaps	%xmm4, %xmm5
	movaps	%xmm4, %xmm6
	movaps	%xmm4, %xmm7
	movaps	ppos2, %xmm8
	movaps	%xmm8, %xmm9
	movaps	%xmm8, %xmm10
	movaps	%xmm8, %xmm11
	subps	xa, %xmm0
	subps	ya, %xmm4
	movaps  %xmm0, 8(%rsp)    # dxa
	mulps   %xmm0, %xmm0
	subps	za, %xmm8
	movaps  %xmm4, 72(%rsp)   # dya
	mulps   %xmm4, %xmm4
	subps	xb, %xmm1
	movaps  %xmm8, 136(%rsp)  # dza
	mulps   %xmm8, %xmm8
	addps   eps2, %xmm0
	subps	yb, %xmm5
	movaps  %xmm1, 24(%rsp)   # dxb
	mulps   %xmm1, %xmm1
	addps   %xmm4, %xmm0
	subps	zb, %xmm9
	movaps  %xmm5, 88(%rsp)   # dyb
	mulps   %xmm5, %xmm5
	addps   %xmm8, %xmm0
	subps	xc, %xmm2
	movaps  %xmm9, 152(%rsp)  # dzb
	mulps   %xmm9, %xmm9
	prefetcht0 512(%rdi)
	rsqrtps	%xmm0, %xmm4
	addps   eps2, %xmm1
	subps	yc, %xmm6
	movaps  %xmm2, 40(%rsp)   # dxc
	mulps   %xmm2, %xmm2
	addps   %xmm5, %xmm1
	subps	zc, %xmm10
	movaps  %xmm6, 104(%rsp)  # dyc
	mulps   %xmm6, %xmm6
	addps   %xmm9, %xmm1
	subps	xd, %xmm3
	movaps  %xmm10, 168(%rsp)  # dzc
	mulps   %xmm10, %xmm10
	rsqrtps	%xmm1, %xmm5
 	addps   eps2, %xmm2
	movaps  %xmm3, 56(%rsp)   # dxd
	mulps   %xmm3, %xmm3
	subps	yd, %xmm7
	addps   %xmm6, %xmm2
	subps	zd, %xmm11
	movaps  %xmm7, 120(%rsp)  # dyd
	mulps   %xmm7, %xmm7
	addps   %xmm10, %xmm2
	movaps  %xmm11, 184(%rsp)  # dzd
	mulps   %xmm11, %xmm11
	addps   %xmm7, %xmm3
	prefetcht0 576(%rdi)
	rsqrtps	%xmm2, %xmm6
	addps   eps2, %xmm3
	mulps   %xmm4, %xmm0
	addps   %xmm11, %xmm3
	mulps   %xmm5, %xmm1
	mulps   %xmm6, %xmm2
	rsqrtps	%xmm3, %xmm7
	mulps   %xmm7, %xmm3
	movaps  %xmm4, %xmm8
	movaps  %xmm5, %xmm9
	movaps  %xmm6, %xmm10
	movaps  %xmm7, %xmm11
	mulps   %xmm0, %xmm4
	mulps   %xmm1, %xmm5
	mulps   %xmm2, %xmm6
	mulps   %xmm3, %xmm7
	subps	.three(%rip), %xmm4
	subps	.three(%rip), %xmm5
	subps	.three(%rip), %xmm6
	subps	.three(%rip), %xmm7
	mulps   %xmm8, %xmm4
	mulps   %xmm9, %xmm5
	mulps   %xmm10, %xmm6
	mulps   %xmm11, %xmm7
	mulps	.minus_half(%rip), %xmm4
	mulps	.minus_half(%rip), %xmm5
	mulps	.minus_half(%rip), %xmm6
	mulps	.minus_half(%rip), %xmm7
	prefetcht0 640(%rdi)
	movaps  %xmm4, %xmm0
	movaps  %xmm5, %xmm1
	movaps  %xmm6, %xmm2
	movaps  %xmm7, %xmm3
	mulps   %xmm0, %xmm0
	mulps   %xmm1, %xmm1
	mulps   %xmm2, %xmm2
	mulps   %xmm3, %xmm3
	mulps	ma, %xmm4
	mulps	mb, %xmm5
	mulps	mc, %xmm6
	mulps	md, %xmm7
	prefetcht0 704(%rdi)
	addq	$256, %rdi
	subps   %xmm4, %xmm12
	mulps   %xmm4, %xmm0
	subps   %xmm5, %xmm12
	mulps   %xmm5, %xmm1
	subps   %xmm6, %xmm12
	mulps   %xmm6, %xmm2
	subps   %xmm7, %xmm12
	mulps   %xmm7, %xmm3
	movaps  %xmm0, %xmm8
	movaps  %xmm1, %xmm9
	movaps  %xmm2, %xmm10
	movaps  %xmm3, %xmm11
	mulps   8(%rsp), %xmm8
	mulps   24(%rsp), %xmm9
	mulps   40(%rsp), %xmm10
	mulps   56(%rsp), %xmm11
	cmpq	%rdi, %rsi
	subps   %xmm8, %xmm13
	movaps  %xmm0, %xmm8
	subps   %xmm9, %xmm13
	movaps  %xmm1, %xmm9
	subps   %xmm10, %xmm13
	movaps  %xmm2, %xmm10
	subps   %xmm11, %xmm13
	movaps  %xmm3, %xmm11
	mulps   72(%rsp), %xmm8
	mulps   88(%rsp), %xmm9
	mulps   104(%rsp), %xmm10
	mulps   120(%rsp), %xmm11
	subps   %xmm8, %xmm14
	mulps   136(%rsp), %xmm0
	subps   %xmm9, %xmm14
	mulps   152(%rsp), %xmm1
	subps   %xmm10, %xmm14
	subps   %xmm0, %xmm15
	mulps   168(%rsp), %xmm2
	subps   %xmm1, %xmm15
	subps   %xmm11, %xmm14
	subps   %xmm2, %xmm15
	mulps   184(%rsp), %xmm3
	subps   %xmm3, %xmm15
	ja	.loop
	haddps  %xmm12, %xmm12
	haddps  %xmm13, %xmm13
	haddps  %xmm14, %xmm14
	haddps  %xmm15, %xmm15
	haddps  %xmm12, %xmm12
	haddps  %xmm13, %xmm13
	haddps  %xmm14, %xmm14
	haddps  %xmm15, %xmm15
	addss   (%r8),  %xmm13
	addss   4(%r8), %xmm14
	addss   8(%r8), %xmm15
	addss   (%r9),  %xmm12
	movss   %xmm13,  (%r8)  # acc[0]
	movss   %xmm14, 4(%r8)  # acc[1]
	movss   %xmm15, 8(%r8)  # acc[2]
	movss   %xmm12,  (%r9)  # phi
.end:
	addq	$256, %rsp
	ret
.LFE0:
	.align 16
.three:
	.long	1077936128
	.long	1077936128
	.long	1077936128
	.long	1077936128
	.align 16
.minus_half:
	.long	3204448256
	.long	3204448256
	.long	3204448256
	.long	3204448256
	.align 16
	.ident	"GCC: (GNU) 4.6.0 20110603 (Red Hat 4.6.0-10)"
