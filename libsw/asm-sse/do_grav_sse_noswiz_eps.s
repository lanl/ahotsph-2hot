# -- Begin  do_grav_sse
# mark_begin;
       .align    4,0x90
# parameter 1(p): 8 + %ebp
# parameter 2(end): 12 + %ebp
# parameter 3(pos0): 16 + %ebp
# parameter 4(mass0): 20 + %ebp
# parameter 5(acc0): 24 + %ebp
# parameter 6(phi0): 28 + %ebp
# parameter 7(eps): 32 + %ebp
	.globl   do_grav_sse_noswiz_eps
do_grav_sse_noswiz_eps:
.init:
        pushl     %ebp
        movl      %esp, %ebp
        subl      $144, %esp
	andl      $0xfffffff0, %esp	# make sure stack is aligned
        movl      8(%ebp), %eax		# p
	movl	  16(%ebp), %edx
	movss	  (%edx), %xmm0
	movss	  4(%edx), %xmm1
	movss	  8(%edx), %xmm2
        movss     %xmm0, (%esp)		# pposx
        movss     %xmm0, 4(%esp)                   
        movss     %xmm0, 8(%esp)                   
        movss     %xmm0, 12(%esp)                   
        movss     %xmm1, 16(%esp)	# pposy
        movss     %xmm1, 20(%esp)                   
        movss     %xmm1, 24(%esp)                   
        movss     %xmm1, 28(%esp)                   
        movss     %xmm2, 32(%esp)	# pposz
        movss     %xmm2, 36(%esp)                   
        movss     %xmm2, 40(%esp)                   
        movss     %xmm2, 44(%esp)
	movl	  32(%ebp), %edx
	movss	  (%edx), %xmm0
        movss     %xmm0, 48(%esp)	#  eps
        movss     %xmm0, 52(%esp)                              
        movss     %xmm0, 56(%esp)                              
        movss     %xmm0, 60(%esp)                              
        movl      12(%ebp), %edx	# end
	xorps	  %xmm0, %xmm0		# zero %xmm0
	movaps	  %xmm0, 64(%esp)	# mass
	movaps	  %xmm0, 80(%esp)	# phi
	movaps	  %xmm0, 96(%esp)	# ax
	movaps	  %xmm0, 112(%esp)	# ay
	movaps	  %xmm0, 128(%esp)	# az
        cmpl      %edx, %eax
        jae       .end
        .align    4,0x90
.loop:					# 112 flops in 43 cycles
					# 11 add/sub
					# 9 mul
					# 1 rsqrt
        movaps    (%eax), %xmm5		# x
        movaps    16(%eax), %xmm6	# y
        movaps    32(%eax), %xmm7	# z
	movaps	  48(%eax), %xmm4	# eps
	movaps	  64(%eax), %xmm0	# mass
	subps	  (%esp), %xmm5
	subps	  16(%esp), %xmm6
	subps	  32(%esp), %xmm7
	addps	  48(%esp), %xmm4
	movaps	  %xmm5, %xmm1
	movaps	  %xmm6, %xmm2
	movaps	  %xmm7, %xmm3

	mulps	  %xmm4, %xmm4
	mulps	  %xmm1, %xmm1
	mulps	  %xmm2, %xmm2
	mulps	  %xmm3, %xmm3
	addps	  %xmm1, %xmm4

	addps	  %xmm2, %xmm4

	addps	  %xmm3, %xmm4
	rsqrtps	  %xmm4, %xmm4
	movaps	  %xmm0, %xmm1
	addps	  64(%esp), %xmm1	# mass
	movaps	  %xmm1, 64(%esp)
        addl      $80, %eax
        cmpl      %edx, %eax
	mulps	  %xmm4, %xmm0
	mulps	  %xmm4, %xmm4
	prefetcht0 512(%eax)		# empirically determined
	prefetcht0 544(%eax)
	mulps	  %xmm0, %xmm4
	addps	  80(%esp), %xmm0	# phi
	movaps	  %xmm0, 80(%esp)
	mulps	  %xmm4, %xmm5
	mulps	  %xmm4, %xmm6
	mulps	  %xmm4, %xmm7
	addps	  96(%esp), %xmm5	# ax
	addps	  112(%esp), %xmm6	# ay
	addps	  128(%esp), %xmm7	# az
	movaps	  %xmm5, 96(%esp)
	movaps	  %xmm6, 112(%esp)
	movaps	  %xmm7, 128(%esp)
        jb       .loop         # Prob 97%                      #290.5
.store:
	movl	20(%ebp), %edx
	movss	(%edx), %xmm0
	addss	64(%esp), %xmm0
	addss	68(%esp), %xmm0
	addss	72(%esp), %xmm0
	addss	76(%esp), %xmm0
	movss	%xmm0, (%edx)		# mass0

	movl	28(%ebp), %edx
	movss	(%edx), %xmm0
	subss	80(%esp), %xmm0
	subss	84(%esp), %xmm0
	subss	88(%esp), %xmm0
	subss	92(%esp), %xmm0
	movss	%xmm0, (%edx)		# phi0

	movl	24(%ebp), %edx
	movss	(%edx), %xmm1
	movss	4(%edx), %xmm2
	movss	8(%edx), %xmm3
	
	addss	96(%esp), %xmm1
	addss	100(%esp), %xmm1	# acc0
	addss	104(%esp), %xmm1
	addss	108(%esp), %xmm1

	addss	112(%esp), %xmm2
	addss	116(%esp), %xmm2
	addss	120(%esp), %xmm2
	addss	124(%esp), %xmm2
	
	addss	128(%esp), %xmm3
	addss	132(%esp), %xmm3
	addss	136(%esp), %xmm3
	addss	140(%esp), %xmm3
	movss	%xmm1, (%edx)
	movss	%xmm2, 4(%edx)
	movss	%xmm3, 8(%edx)
.end:
	movl      %ebp, %esp                                    #377.1
        popl      %ebp                                          #377.1
        ret                                                     #377.1
        .align    4,0x90
# mark_end;
	.type	do_grav_sse,@function
	.size	do_grav_sse,.-do_grav_sse
# -- End  do_grav_sse
