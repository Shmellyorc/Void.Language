
/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/captures/guarded-range-i32.candidate.True.o:	file format elf64-x86-64

Disassembly of section .text:

0000000000000000 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void>:
       0: 85 ff                        	testl	%edi, %edi
       2: 0f 8e 6c 01 00 00            	jle	0x174 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x174>
       8: 31 c9                        	xorl	%ecx, %ecx
       a: ba 00 00 00 00               	movl	$0x0, %edx
       f: 83 ff 08                     	cmpl	$0x8, %edi
      12: 0f 82 0f 01 00 00            	jb	0x127 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x127>
      18: 89 f9                        	movl	%edi, %ecx
      1a: 81 e1 f8 ff ff 7f            	andl	$0x7ffffff8, %ecx       # imm = 0x7FFFFFF8
      20: 66 0f ef ed                  	pxor	%xmm5, %xmm5
      24: 66 0f 6f 05 00 00 00 00      	movdqa	(%rip), %xmm0           # 0x2c <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x2c>
		0000000000000028:  R_X86_64_PC32	.LCPI0_0-0x4
      2c: 66 0f 6f 0d 00 00 00 00      	movdqa	(%rip), %xmm1           # 0x34 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x34>
		0000000000000030:  R_X86_64_PC32	.LCPI0_1-0x4
      34: 66 0f 6f 15 00 00 00 00      	movdqa	(%rip), %xmm2           # 0x3c <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x3c>
		0000000000000038:  R_X86_64_PC32	.LCPI0_2-0x4
      3c: 66 0f 6f 1d 00 00 00 00      	movdqa	(%rip), %xmm3           # 0x44 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x44>
		0000000000000040:  R_X86_64_PC32	.LCPI0_3-0x4
      44: 89 c8                        	movl	%ecx, %eax
      46: 66 0f ef e4                  	pxor	%xmm4, %xmm4
      4a: 66 0f 1f 44 00 00            	nopw	(%rax,%rax)
      50: 66 0f 6f f5                  	movdqa	%xmm5, %xmm6
      54: 66 0f 6f e8                  	movdqa	%xmm0, %xmm5
      58: 66 0f fe e9                  	paddd	%xmm1, %xmm5
      5c: 66 0f 6f f8                  	movdqa	%xmm0, %xmm7
      60: 66 0f f4 fa                  	pmuludq	%xmm2, %xmm7
      64: 66 0f 70 ff ed               	pshufd	$0xed, %xmm7, %xmm7     # xmm7 = xmm7[1,3,2,3]
      69: 66 44 0f 70 c0 f5            	pshufd	$0xf5, %xmm0, %xmm8     # xmm8 = xmm0[1,1,3,3]
      6f: 66 44 0f f4 c2               	pmuludq	%xmm2, %xmm8
      74: 66 45 0f 70 c0 ed            	pshufd	$0xed, %xmm8, %xmm8     # xmm8 = xmm8[1,3,2,3]
      7a: 66 41 0f 62 f8               	punpckldq	%xmm8, %xmm7    # xmm7 = xmm7[0],xmm8[0],xmm7[1],xmm8[1]
      7f: 66 44 0f 6f c0               	movdqa	%xmm0, %xmm8
      84: 66 44 0f fa c7               	psubd	%xmm7, %xmm8
      89: 66 41 0f 72 d0 01            	psrld	$0x1, %xmm8
      8f: 66 44 0f fe c7               	paddd	%xmm7, %xmm8
      94: 66 41 0f 72 d0 06            	psrld	$0x6, %xmm8
      9a: 66 41 0f 6f f8               	movdqa	%xmm8, %xmm7
      9f: 66 0f 72 f7 07               	pslld	$0x7, %xmm7
      a4: 66 44 0f fa c7               	psubd	%xmm7, %xmm8
      a9: 66 0f 6f fd                  	movdqa	%xmm5, %xmm7
      ad: 66 0f f4 fa                  	pmuludq	%xmm2, %xmm7
      b1: 66 0f 70 ff ed               	pshufd	$0xed, %xmm7, %xmm7     # xmm7 = xmm7[1,3,2,3]
      b6: 66 44 0f 70 cd f5            	pshufd	$0xf5, %xmm5, %xmm9     # xmm9 = xmm5[1,1,3,3]
      bc: 66 44 0f f4 ca               	pmuludq	%xmm2, %xmm9
      c1: 66 45 0f 70 c9 ed            	pshufd	$0xed, %xmm9, %xmm9     # xmm9 = xmm9[1,3,2,3]
      c7: 66 41 0f 62 f9               	punpckldq	%xmm9, %xmm7    # xmm7 = xmm7[0],xmm9[0],xmm7[1],xmm9[1]
      cc: 66 0f fe f5                  	paddd	%xmm5, %xmm6
      d0: 66 0f fa ef                  	psubd	%xmm7, %xmm5
      d4: 66 0f 72 d5 01               	psrld	$0x1, %xmm5
      d9: 66 0f fe ef                  	paddd	%xmm7, %xmm5
      dd: 66 0f 72 d5 06               	psrld	$0x6, %xmm5
      e2: 66 0f 6f fd                  	movdqa	%xmm5, %xmm7
      e6: 66 0f 72 f7 07               	pslld	$0x7, %xmm7
      eb: 66 0f fa ef                  	psubd	%xmm7, %xmm5
      ef: 66 0f fe e0                  	paddd	%xmm0, %xmm4
      f3: 66 41 0f fe e0               	paddd	%xmm8, %xmm4
      f8: 66 0f fe ee                  	paddd	%xmm6, %xmm5
      fc: 66 0f fe c3                  	paddd	%xmm3, %xmm0
     100: 83 c0 f8                     	addl	$-0x8, %eax
     103: 0f 85 47 ff ff ff            	jne	0x50 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x50>
     109: 66 0f fe ec                  	paddd	%xmm4, %xmm5
     10d: 66 0f 70 c5 ee               	pshufd	$0xee, %xmm5, %xmm0     # xmm0 = xmm5[2,3,2,3]
     112: 66 0f fe c5                  	paddd	%xmm5, %xmm0
     116: 66 0f 70 c8 55               	pshufd	$0x55, %xmm0, %xmm1     # xmm1 = xmm0[1,1,1,1]
     11b: 66 0f fe c8                  	paddd	%xmm0, %xmm1
     11f: 66 0f 7e ca                  	movd	%xmm1, %edx
     123: 39 cf                        	cmpl	%ecx, %edi
     125: 74 4a                        	je	0x171 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x171>
     127: 8d 71 ff                     	leal	-0x1(%rcx), %esi
     12a: 29 cf                        	subl	%ecx, %edi
     12c: 49 b9 00 00 00 22 10 08 04 02	movabsq	$0x204081022000000, %r9 # imm = 0x204081022000000
     136: 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
     140: 41 89 d0                     	movl	%edx, %r8d
     143: 89 c8                        	movl	%ecx, %eax
     145: 49 f7 e1                     	mulq	%r9
     148: 89 d0                        	movl	%edx, %eax
     14a: c1 e0 07                     	shll	$0x7, %eax
     14d: 29 c2                        	subl	%eax, %edx
     14f: 44 01 c2                     	addl	%r8d, %edx
     152: 01 ca                        	addl	%ecx, %edx
     154: ff c1                        	incl	%ecx
     156: ff c6                        	incl	%esi
     158: ff cf                        	decl	%edi
     15a: 75 e4                        	jne	0x140 <__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void+0x140>
     15c: 89 f0                        	movl	%esi, %eax
     15e: 49 f7 e1                     	mulq	%r9
     161: 89 d0                        	movl	%edx, %eax
     163: c1 e0 07                     	shll	$0x7, %eax
     166: 29 c2                        	subl	%eax, %edx
     168: 41 01 f0                     	addl	%esi, %r8d
     16b: 41 01 d0                     	addl	%edx, %r8d
     16e: 44 89 c2                     	movl	%r8d, %edx
     171: 89 d0                        	movl	%edx, %eax
     173: c3                           	retq
     174: 31 d2                        	xorl	%edx, %edx
     176: 89 d0                        	movl	%edx, %eax
     178: c3                           	retq
     179: 0f 1f 80 00 00 00 00         	nopl	(%rax)

0000000000000180 <main>:
     180: 55                           	pushq	%rbp
     181: 48 89 e5                     	movq	%rsp, %rbp
     184: 41 57                        	pushq	%r15
     186: 41 56                        	pushq	%r14
     188: 41 55                        	pushq	%r13
     18a: 41 54                        	pushq	%r12
     18c: 53                           	pushq	%rbx
     18d: 50                           	pushq	%rax
     18e: 31 db                        	xorl	%ebx, %ebx
     190: ba 00 00 00 00               	movl	$0x0, %edx
     195: 83 ff 01                     	cmpl	$0x1, %edi
     198: 0f 84 e7 01 00 00            	je	0x385 <main+0x205>
     19e: 41 89 fe                     	movl	%edi, %r14d
     1a1: 49 89 e5                     	movq	%rsp, %r13
     1a4: 49 8d 45 f0                  	leaq	-0x10(%r13), %rax
     1a8: 48 89 c4                     	movq	%rax, %rsp
     1ab: 4c 8b 66 08                  	movq	0x8(%rsi), %r12
     1af: 4c 89 e7                     	movq	%r12, %rdi
     1b2: 48 89 c6                     	movq	%rax, %rsi
     1b5: ba 0a 00 00 00               	movl	$0xa, %edx
     1ba: e8 00 00 00 00               	callq	0x1bf <main+0x3f>
		00000000000001bb:  R_X86_64_PLT32	strtoll-0x4
     1bf: 49 89 c7                     	movq	%rax, %r15
     1c2: 49 63 c7                     	movslq	%r15d, %rax
     1c5: 4c 39 f8                     	cmpq	%r15, %rax
     1c8: 0f 85 c9 01 00 00            	jne	0x397 <main+0x217>
     1ce: 41 83 fe 02                  	cmpl	$0x2, %r14d
     1d2: 0f 85 bf 01 00 00            	jne	0x397 <main+0x217>
     1d8: 49 8b 45 f0                  	movq	-0x10(%r13), %rax
     1dc: 4c 39 e0                     	cmpq	%r12, %rax
     1df: 0f 84 b2 01 00 00            	je	0x397 <main+0x217>
     1e5: 80 38 00                     	cmpb	$0x0, (%rax)
     1e8: 0f 85 a9 01 00 00            	jne	0x397 <main+0x217>
     1ee: 49 89 e6                     	movq	%rsp, %r14
     1f1: 49 81 c6 f0 fe ff ff         	addq	$-0x110, %r14           # imm = 0xFEF0
     1f8: 4c 89 f4                     	movq	%r14, %rsp
     1fb: ba 10 01 00 00               	movl	$0x110, %edx            # imm = 0x110
     200: 4c 89 f7                     	movq	%r14, %rdi
     203: 31 f6                        	xorl	%esi, %esi
     205: e8 00 00 00 00               	callq	0x20a <main+0x8a>
		0000000000000206:  R_X86_64_PLT32	memset-0x4
     20a: 31 d2                        	xorl	%edx, %edx
     20c: 4d 85 ff                     	testq	%r15, %r15
     20f: 0f 8e 70 01 00 00            	jle	0x385 <main+0x205>
     215: 31 c9                        	xorl	%ecx, %ecx
     217: ba 00 00 00 00               	movl	$0x0, %edx
     21c: 41 83 ff 08                  	cmpl	$0x8, %r15d
     220: 0f 82 12 01 00 00            	jb	0x338 <main+0x1b8>
     226: 44 89 f9                     	movl	%r15d, %ecx
     229: 83 e1 f8                     	andl	$-0x8, %ecx
     22c: 66 0f ef ed                  	pxor	%xmm5, %xmm5
     230: 66 0f 6f 05 00 00 00 00      	movdqa	(%rip), %xmm0           # 0x238 <main+0xb8>
		0000000000000234:  R_X86_64_PC32	.LCPI1_0-0x4
     238: 66 0f 6f 0d 00 00 00 00      	movdqa	(%rip), %xmm1           # 0x240 <main+0xc0>
		000000000000023c:  R_X86_64_PC32	.LCPI1_1-0x4
     240: 66 0f 6f 15 00 00 00 00      	movdqa	(%rip), %xmm2           # 0x248 <main+0xc8>
		0000000000000244:  R_X86_64_PC32	.LCPI1_2-0x4
     248: 66 0f 6f 1d 00 00 00 00      	movdqa	(%rip), %xmm3           # 0x250 <main+0xd0>
		000000000000024c:  R_X86_64_PC32	.LCPI1_3-0x4
     250: 89 c8                        	movl	%ecx, %eax
     252: 66 0f ef e4                  	pxor	%xmm4, %xmm4
     256: 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
     260: 66 0f 6f f5                  	movdqa	%xmm5, %xmm6
     264: 66 0f 6f e8                  	movdqa	%xmm0, %xmm5
     268: 66 0f fe e9                  	paddd	%xmm1, %xmm5
     26c: 66 0f 6f f8                  	movdqa	%xmm0, %xmm7
     270: 66 0f f4 fa                  	pmuludq	%xmm2, %xmm7
     274: 66 0f 70 ff ed               	pshufd	$0xed, %xmm7, %xmm7     # xmm7 = xmm7[1,3,2,3]
     279: 66 44 0f 70 c0 f5            	pshufd	$0xf5, %xmm0, %xmm8     # xmm8 = xmm0[1,1,3,3]
     27f: 66 44 0f f4 c2               	pmuludq	%xmm2, %xmm8
     284: 66 45 0f 70 c0 ed            	pshufd	$0xed, %xmm8, %xmm8     # xmm8 = xmm8[1,3,2,3]
     28a: 66 41 0f 62 f8               	punpckldq	%xmm8, %xmm7    # xmm7 = xmm7[0],xmm8[0],xmm7[1],xmm8[1]
     28f: 66 44 0f 6f c0               	movdqa	%xmm0, %xmm8
     294: 66 44 0f fa c7               	psubd	%xmm7, %xmm8
     299: 66 41 0f 72 d0 01            	psrld	$0x1, %xmm8
     29f: 66 44 0f fe c7               	paddd	%xmm7, %xmm8
     2a4: 66 41 0f 72 d0 06            	psrld	$0x6, %xmm8
     2aa: 66 41 0f 6f f8               	movdqa	%xmm8, %xmm7
     2af: 66 0f 72 f7 07               	pslld	$0x7, %xmm7
     2b4: 66 44 0f fa c7               	psubd	%xmm7, %xmm8
     2b9: 66 0f 6f fd                  	movdqa	%xmm5, %xmm7
     2bd: 66 0f f4 fa                  	pmuludq	%xmm2, %xmm7
     2c1: 66 0f 70 ff ed               	pshufd	$0xed, %xmm7, %xmm7     # xmm7 = xmm7[1,3,2,3]
     2c6: 66 44 0f 70 cd f5            	pshufd	$0xf5, %xmm5, %xmm9     # xmm9 = xmm5[1,1,3,3]
     2cc: 66 44 0f f4 ca               	pmuludq	%xmm2, %xmm9
     2d1: 66 45 0f 70 c9 ed            	pshufd	$0xed, %xmm9, %xmm9     # xmm9 = xmm9[1,3,2,3]
     2d7: 66 41 0f 62 f9               	punpckldq	%xmm9, %xmm7    # xmm7 = xmm7[0],xmm9[0],xmm7[1],xmm9[1]
     2dc: 66 0f fe f5                  	paddd	%xmm5, %xmm6
     2e0: 66 0f fa ef                  	psubd	%xmm7, %xmm5
     2e4: 66 0f 72 d5 01               	psrld	$0x1, %xmm5
     2e9: 66 0f fe ef                  	paddd	%xmm7, %xmm5
     2ed: 66 0f 72 d5 06               	psrld	$0x6, %xmm5
     2f2: 66 0f 6f fd                  	movdqa	%xmm5, %xmm7
     2f6: 66 0f 72 f7 07               	pslld	$0x7, %xmm7
     2fb: 66 0f fa ef                  	psubd	%xmm7, %xmm5
     2ff: 66 0f fe e0                  	paddd	%xmm0, %xmm4
     303: 66 41 0f fe e0               	paddd	%xmm8, %xmm4
     308: 66 0f fe ee                  	paddd	%xmm6, %xmm5
     30c: 66 0f fe c3                  	paddd	%xmm3, %xmm0
     310: 83 c0 f8                     	addl	$-0x8, %eax
     313: 0f 85 47 ff ff ff            	jne	0x260 <main+0xe0>
     319: 66 0f fe ec                  	paddd	%xmm4, %xmm5
     31d: 66 0f 70 c5 ee               	pshufd	$0xee, %xmm5, %xmm0     # xmm0 = xmm5[2,3,2,3]
     322: 66 0f fe c5                  	paddd	%xmm5, %xmm0
     326: 66 0f 70 c8 55               	pshufd	$0x55, %xmm0, %xmm1     # xmm1 = xmm0[1,1,1,1]
     32b: 66 0f fe c8                  	paddd	%xmm0, %xmm1
     32f: 66 0f 7e ca                  	movd	%xmm1, %edx
     333: 44 39 f9                     	cmpl	%r15d, %ecx
     336: 74 45                        	je	0x37d <main+0x1fd>
     338: 41 29 cf                     	subl	%ecx, %r15d
     33b: 8d 71 ff                     	leal	-0x1(%rcx), %esi
     33e: 49 b8 00 00 00 22 10 08 04 02	movabsq	$0x204081022000000, %r8 # imm = 0x204081022000000
     348: 0f 1f 84 00 00 00 00 00      	nopl	(%rax,%rax)
     350: 89 d7                        	movl	%edx, %edi
     352: 89 c8                        	movl	%ecx, %eax
     354: 49 f7 e0                     	mulq	%r8
     357: 89 d0                        	movl	%edx, %eax
     359: c1 e0 07                     	shll	$0x7, %eax
     35c: 29 c2                        	subl	%eax, %edx
     35e: 01 fa                        	addl	%edi, %edx
     360: 01 ca                        	addl	%ecx, %edx
     362: ff c1                        	incl	%ecx
     364: ff c6                        	incl	%esi
     366: 41 ff cf                     	decl	%r15d
     369: 75 e5                        	jne	0x350 <main+0x1d0>
     36b: 89 f0                        	movl	%esi, %eax
     36d: 49 f7 e0                     	mulq	%r8
     370: 89 d0                        	movl	%edx, %eax
     372: c1 e0 07                     	shll	$0x7, %eax
     375: 29 c2                        	subl	%eax, %edx
     377: 01 f7                        	addl	%esi, %edi
     379: 01 d7                        	addl	%edx, %edi
     37b: 89 fa                        	movl	%edi, %edx
     37d: 49 8b 3e                     	movq	(%r14), %rdi
     380: 48 85 ff                     	testq	%rdi, %rdi
     383: 75 34                        	jne	0x3b9 <main+0x239>
     385: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x38c <main+0x20c>
		0000000000000388:  R_X86_64_PC32	.L__unnamed_1-0x4
     38c: 89 d6                        	movl	%edx, %esi
     38e: 31 c0                        	xorl	%eax, %eax
     390: e8 00 00 00 00               	callq	0x395 <main+0x215>
		0000000000000391:  R_X86_64_PLT32	printf-0x4
     395: eb 11                        	jmp	0x3a8 <main+0x228>
     397: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x39e <main+0x21e>
		000000000000039a:  R_X86_64_PC32	.L__unnamed_2-0x4
     39e: e8 00 00 00 00               	callq	0x3a3 <main+0x223>
		000000000000039f:  R_X86_64_PLT32	puts-0x4
     3a3: bb 02 00 00 00               	movl	$0x2, %ebx
     3a8: 89 d8                        	movl	%ebx, %eax
     3aa: 48 8d 65 d8                  	leaq	-0x28(%rbp), %rsp
     3ae: 5b                           	popq	%rbx
     3af: 41 5c                        	popq	%r12
     3b1: 41 5d                        	popq	%r13
     3b3: 41 5e                        	popq	%r14
     3b5: 41 5f                        	popq	%r15
     3b7: 5d                           	popq	%rbp
     3b8: c3                           	retq
     3b9: 48 8b 05 00 00 00 00         	movq	(%rip), %rax            # 0x3c0 <main+0x240>
		00000000000003bc:  R_X86_64_REX_GOTPCRELX	stderr-0x4
     3c0: 48 8b 18                     	movq	(%rax), %rbx
     3c3: 48 89 de                     	movq	%rbx, %rsi
     3c6: e8 00 00 00 00               	callq	0x3cb <main+0x24b>
		00000000000003c7:  R_X86_64_PLT32	fputs-0x4
     3cb: 45 8b 7e 08                  	movl	0x8(%r14), %r15d
     3cf: 45 85 ff                     	testl	%r15d, %r15d
     3d2: 74 2c                        	je	0x400 <main+0x280>
     3d4: 41 83 ff 20                  	cmpl	$0x20, %r15d
     3d8: 41 bc 20 00 00 00            	movl	$0x20, %r12d
     3de: 45 0f 42 e7                  	cmovbl	%r15d, %r12d
     3e2: 45 31 ed                     	xorl	%r13d, %r13d
     3e5: 4b 8b 7c ee 10               	movq	0x10(%r14,%r13,8), %rdi
     3ea: 48 89 de                     	movq	%rbx, %rsi
     3ed: e8 00 00 00 00               	callq	0x3f2 <main+0x272>
		00000000000003ee:  R_X86_64_PLT32	fputs-0x4
     3f2: 49 ff c5                     	incq	%r13
     3f5: 4d 39 ec                     	cmpq	%r13, %r12
     3f8: 75 eb                        	jne	0x3e5 <main+0x265>
     3fa: 41 83 ff 21                  	cmpl	$0x21, %r15d
     3fe: 73 14                        	jae	0x414 <main+0x294>
     400: bf 0a 00 00 00               	movl	$0xa, %edi
     405: 48 89 de                     	movq	%rbx, %rsi
     408: e8 00 00 00 00               	callq	0x40d <main+0x28d>
		0000000000000409:  R_X86_64_PLT32	fputc-0x4
     40d: bb 01 00 00 00               	movl	$0x1, %ebx
     412: eb 94                        	jmp	0x3a8 <main+0x228>
     414: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x41b <main+0x29b>
		0000000000000417:  R_X86_64_PC32	.L__unnamed_3-0x4
     41b: be 2a 00 00 00               	movl	$0x2a, %esi
     420: ba 01 00 00 00               	movl	$0x1, %edx
     425: 48 89 d9                     	movq	%rbx, %rcx
     428: e8 00 00 00 00               	callq	0x42d <main+0x2ad>
		0000000000000429:  R_X86_64_PLT32	fwrite-0x4
     42d: eb d1                        	jmp	0x400 <main+0x280>
