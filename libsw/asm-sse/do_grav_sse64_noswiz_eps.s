# -- Begin  do_grav_sse
# parameter 1(p): %rdi
# parameter 2(end): %rsi
# parameter 3(pos0): %rdx
# parameter 4(mass0): %rcx
# parameter 5(acc0): %r8
# parameter 6(phi0): %r9
# parameter 7(eps2p): 16(%rbp)
# parameter 8(ncut):
# must preserve rbx, rsp, rbp, r12-15 for caller	
	.globl   do_grav_sse_noswiz_eps
do_grav_sse_noswiz_eps:
.init:
        pushq     %rbp
        movq      %rsp, %rbp		# use red zone instead?
        subq      $16, %rsp		# assume %rsp-8 16-byte aligned
	movss	  (%rdx), %xmm8		# pposx
	movsldup  %xmm8, %xmm8
	movddup   %xmm8, %xmm8
	movss	  4(%rdx), %xmm9	# pposy
	movsldup  %xmm9, %xmm9
	movddup   %xmm9, %xmm9
	movss	  8(%rdx), %xmm10	# pposz
	movsldup  %xmm10, %xmm10
	movddup   %xmm10, %xmm10
	movq	  16(%rbp), %rdx
	movss	  (%rdx), %xmm0
        movss     %xmm0, (%rsp)		#  eps
        movss     %xmm0, 4(%rsp)                              
        movss     %xmm0, 8(%rsp)                              
        movss     %xmm0, 12(%rsp)                              
	xorps	  %xmm11, %xmm11	# mass (zero %xmm11)
	xorps	  %xmm12, %xmm12	# phi
	xorps	  %xmm13, %xmm13	# ax
	xorps	  %xmm14, %xmm14	# ay
	xorps	  %xmm15, %xmm15	# az
        cmpq      %rsi, %rdi
        jae       .end
.loop:					# 112 flops in 38 cycles
					# 11 add/sub
					# 9 mul
					# 1 rsqrt
        movaps    (%rdi), %xmm5		# x
        movaps    16(%rdi), %xmm6	# y
        movaps    32(%rdi), %xmm7	# z
        movaps    48(%rdi), %xmm4	# eps
	movaps	  64(%rdi), %xmm0	# mass
	// This has not been rewritten, and does not work
	// msw, 9/2/2009
	subps	  %xmm8, %xmm5
	subps	  %xmm9, %xmm6
	subps	  %xmm10, %xmm7
	movaps	  %xmm5, %xmm1
	movaps	  %xmm6, %xmm2
	movaps	  %xmm7, %xmm3
	movaps	  (%rsp), %xmm4	# eps
	mulps	  %xmm1, %xmm1
	mulps	  %xmm2, %xmm2
	mulps	  %xmm3, %xmm3
	addps	  %xmm1, %xmm4

	addps	  %xmm2, %xmm4

	addps	  %xmm3, %xmm4
	rsqrtps	  %xmm4, %xmm4
	movaps	  %xmm0, %xmm1
	addps	  %xmm1, %xmm11		# mass
        addq      $64, %rdi
        cmpq      %rsi, %rdi
	mulps	  %xmm4, %xmm0
	mulps	  %xmm4, %xmm4
	prefetcht0 512(%rdi)		# empirically determined
	prefetcht0 544(%rdi)
	mulps	  %xmm0, %xmm4
	addps	  %xmm0, %xmm12		# phi
	mulps	  %xmm4, %xmm5
	mulps	  %xmm4, %xmm6
	mulps	  %xmm4, %xmm7
	addps	  %xmm5, %xmm13		# ax
	addps	  %xmm6, %xmm14		# ay
	addps	  %xmm7, %xmm15		# az
        jb       .loop         # Prob 97%                      #290.5
.store:
	movss	(%rcx), %xmm0
	haddps  %xmm11, %xmm11		# SSE3
	haddps  %xmm11, %xmm11
	addss   %xmm11, %xmm0
	movss	%xmm0, (%rcx)		# mass0

	movss	(%r9), %xmm0
	haddps  %xmm12, %xmm12		# SSE3
	haddps  %xmm12, %xmm12
	subss	%xmm12, %xmm0
	movss	%xmm0, (%r9)		# phi0

	movss	(%r8), %xmm1
	movss	4(%r8), %xmm2
	movss	8(%r8), %xmm3

	haddps  %xmm13, %xmm13		# SSE3
	haddps  %xmm13, %xmm13
	addss	%xmm13, %xmm1		# ax

	haddps  %xmm14, %xmm14		# SSE3
	haddps  %xmm14, %xmm14
	addss	%xmm14, %xmm2		# ay

	haddps  %xmm15, %xmm15		# SSE3
	haddps  %xmm15, %xmm15
	addss	%xmm15, %xmm3		# az

	movss	%xmm1, (%r8)
	movss	%xmm2, 4(%r8)
	movss	%xmm3, 8(%r8)
.end:
	movq      %rbp, %rsp                                    #377.1
        popq      %rbp                                          #377.1
        ret                                                     #377.1
# mark_end;
	.type	do_grav_sse,@function
	.size	do_grav_sse,.-do_grav_sse
# -- End  do_grav_sse
