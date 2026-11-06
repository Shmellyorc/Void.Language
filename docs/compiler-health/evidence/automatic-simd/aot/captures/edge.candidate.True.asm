
/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/captures/edge.candidate.True.o:	file format elf64-x86-64

Disassembly of section .text:

0000000000000000 <__vd_2f656467652e766f69643a3a656e747279__void>:
       0: f3 0f 10 05 00 00 00 00      	movss	(%rip), %xmm0           # 0x8 <__vd_2f656467652e766f69643a3a656e747279__void+0x8>
		0000000000000004:  R_X86_64_PC32	.LCPI0_0-0x4
       8: b8 90 00 00 00               	movl	$0x90, %eax
       d: f3 0f 10 0d 00 00 00 00      	movss	(%rip), %xmm1           # 0x15 <__vd_2f656467652e766f69643a3a656e747279__void+0x15>
		0000000000000011:  R_X86_64_PC32	.LCPI0_1-0x4
      15: 66 66 2e 0f 1f 84 00 00 00 00 00     	nopw	%cs:(%rax,%rax)
      20: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      24: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      28: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      2c: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      30: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      34: 85 c0                        	testl	%eax, %eax
      36: 74 11                        	je	0x49 <__vd_2f656467652e766f69643a3a656e747279__void+0x49>
      38: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      3c: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      40: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      44: 83 c0 f8                     	addl	$-0x8, %eax
      47: eb d7                        	jmp	0x20 <__vd_2f656467652e766f69643a3a656e747279__void+0x20>
      49: 85 ff                        	testl	%edi, %edi
      4b: 74 38                        	je	0x85 <__vd_2f656467652e766f69643a3a656e747279__void+0x85>
      4d: 89 f9                        	movl	%edi, %ecx
      4f: c1 f9 1f                     	sarl	$0x1f, %ecx
      52: 83 c9 01                     	orl	$0x1, %ecx
      55: 31 d2                        	xorl	%edx, %edx
      57: b8 01 00 00 00               	movl	$0x1, %eax
      5c: 0f 1f 40 00                  	nopl	(%rax)
      60: 01 ca                        	addl	%ecx, %edx
      62: 48 ff c8                     	decq	%rax
      65: 39 d7                        	cmpl	%edx, %edi
      67: 75 f7                        	jne	0x60 <__vd_2f656467652e766f69643a3a656e747279__void+0x60>
      69: 83 ff 01                     	cmpl	$0x1, %edi
      6c: 75 17                        	jne	0x85 <__vd_2f656467652e766f69643a3a656e747279__void+0x85>
      6e: 85 c0                        	testl	%eax, %eax
      70: 0f 95 c1                     	setne	%cl
      73: 48 85 c0                     	testq	%rax, %rax
      76: 0f 95 c2                     	setne	%dl
      79: 08 ca                        	orb	%cl, %dl
      7b: b8 02 00 00 00               	movl	$0x2, %eax
      80: 80 fa 01                     	cmpb	$0x1, %dl
      83: 74 16                        	je	0x9b <__vd_2f656467652e766f69643a3a656e747279__void+0x9b>
      85: f3 0f 58 c0                  	addss	%xmm0, %xmm0
      89: b8 03 00 00 00               	movl	$0x3, %eax
      8e: 0f 57 c9                     	xorps	%xmm1, %xmm1
      91: 0f 2e c8                     	ucomiss	%xmm0, %xmm1
      94: 73 05                        	jae	0x9b <__vd_2f656467652e766f69643a3a656e747279__void+0x9b>
      96: b8 01 00 00 00               	movl	$0x1, %eax
      9b: c3                           	retq
      9c: 0f 1f 40 00                  	nopl	(%rax)

00000000000000a0 <main>:
      a0: 55                           	pushq	%rbp
      a1: 48 89 e5                     	movq	%rsp, %rbp
      a4: 41 57                        	pushq	%r15
      a6: 41 56                        	pushq	%r14
      a8: 41 55                        	pushq	%r13
      aa: 41 54                        	pushq	%r12
      ac: 53                           	pushq	%rbx
      ad: 50                           	pushq	%rax
      ae: 45 31 f6                     	xorl	%r14d, %r14d
      b1: 83 ff 01                     	cmpl	$0x1, %edi
      b4: 74 50                        	je	0x106 <main+0x66>
      b6: 89 fb                        	movl	%edi, %ebx
      b8: 49 89 e4                     	movq	%rsp, %r12
      bb: 49 8d 44 24 f0               	leaq	-0x10(%r12), %rax
      c0: 48 89 c4                     	movq	%rax, %rsp
      c3: 4c 8b 7e 08                  	movq	0x8(%rsi), %r15
      c7: 4c 89 ff                     	movq	%r15, %rdi
      ca: 48 89 c6                     	movq	%rax, %rsi
      cd: ba 0a 00 00 00               	movl	$0xa, %edx
      d2: e8 00 00 00 00               	callq	0xd7 <main+0x37>
		00000000000000d3:  R_X86_64_PLT32	strtoll-0x4
      d7: 49 89 c6                     	movq	%rax, %r14
      da: 49 63 c6                     	movslq	%r14d, %rax
      dd: 4c 39 f0                     	cmpq	%r14, %rax
      e0: 0f 85 ee 00 00 00            	jne	0x1d4 <main+0x134>
      e6: 83 fb 02                     	cmpl	$0x2, %ebx
      e9: 0f 85 e5 00 00 00            	jne	0x1d4 <main+0x134>
      ef: 49 8b 44 24 f0               	movq	-0x10(%r12), %rax
      f4: 4c 39 f8                     	cmpq	%r15, %rax
      f7: 0f 84 d7 00 00 00            	je	0x1d4 <main+0x134>
      fd: 80 38 00                     	cmpb	$0x0, (%rax)
     100: 0f 85 ce 00 00 00            	jne	0x1d4 <main+0x134>
     106: 48 89 e3                     	movq	%rsp, %rbx
     109: 48 81 c3 f0 fe ff ff         	addq	$-0x110, %rbx           # imm = 0xFEF0
     110: 48 89 dc                     	movq	%rbx, %rsp
     113: ba 10 01 00 00               	movl	$0x110, %edx            # imm = 0x110
     118: 48 89 df                     	movq	%rbx, %rdi
     11b: 31 f6                        	xorl	%esi, %esi
     11d: e8 00 00 00 00               	callq	0x122 <main+0x82>
		000000000000011e:  R_X86_64_PLT32	memset-0x4
     122: f3 0f 10 05 00 00 00 00      	movss	(%rip), %xmm0           # 0x12a <main+0x8a>
		0000000000000126:  R_X86_64_PC32	.LCPI1_0-0x4
     12a: b8 90 00 00 00               	movl	$0x90, %eax
     12f: f3 0f 10 0d 00 00 00 00      	movss	(%rip), %xmm1           # 0x137 <main+0x97>
		0000000000000133:  R_X86_64_PC32	.LCPI1_1-0x4
     137: 66 0f 1f 84 00 00 00 00 00   	nopw	(%rax,%rax)
     140: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     144: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     148: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     14c: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     150: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     154: 85 c0                        	testl	%eax, %eax
     156: 74 11                        	je	0x169 <main+0xc9>
     158: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     15c: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     160: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     164: 83 c0 f8                     	addl	$-0x8, %eax
     167: eb d7                        	jmp	0x140 <main+0xa0>
     169: 45 85 f6                     	testl	%r14d, %r14d
     16c: 74 36                        	je	0x1a4 <main+0x104>
     16e: 44 89 f1                     	movl	%r14d, %ecx
     171: c1 f9 1f                     	sarl	$0x1f, %ecx
     174: 83 c9 01                     	orl	$0x1, %ecx
     177: 31 d2                        	xorl	%edx, %edx
     179: b8 01 00 00 00               	movl	$0x1, %eax
     17e: 66 90                        	nop
     180: 01 ca                        	addl	%ecx, %edx
     182: 48 ff c8                     	decq	%rax
     185: 41 39 d6                     	cmpl	%edx, %r14d
     188: 75 f6                        	jne	0x180 <main+0xe0>
     18a: 41 83 fe 01                  	cmpl	$0x1, %r14d
     18e: 75 14                        	jne	0x1a4 <main+0x104>
     190: 85 c0                        	testl	%eax, %eax
     192: 0f 95 c1                     	setne	%cl
     195: 48 85 c0                     	testq	%rax, %rax
     198: 0f 95 c0                     	setne	%al
     19b: 08 c8                        	orb	%cl, %al
     19d: be 02 00 00 00               	movl	$0x2, %esi
     1a2: 75 16                        	jne	0x1ba <main+0x11a>
     1a4: f3 0f 58 c0                  	addss	%xmm0, %xmm0
     1a8: be 03 00 00 00               	movl	$0x3, %esi
     1ad: 0f 57 c9                     	xorps	%xmm1, %xmm1
     1b0: 0f 2e c8                     	ucomiss	%xmm0, %xmm1
     1b3: 73 05                        	jae	0x1ba <main+0x11a>
     1b5: be 01 00 00 00               	movl	$0x1, %esi
     1ba: 48 8b 3b                     	movq	(%rbx), %rdi
     1bd: 48 85 ff                     	testq	%rdi, %rdi
     1c0: 75 34                        	jne	0x1f6 <main+0x156>
     1c2: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x1c9 <main+0x129>
		00000000000001c5:  R_X86_64_PC32	.L__unnamed_1-0x4
     1c9: 31 db                        	xorl	%ebx, %ebx
     1cb: 31 c0                        	xorl	%eax, %eax
     1cd: e8 00 00 00 00               	callq	0x1d2 <main+0x132>
		00000000000001ce:  R_X86_64_PLT32	printf-0x4
     1d2: eb 11                        	jmp	0x1e5 <main+0x145>
     1d4: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x1db <main+0x13b>
		00000000000001d7:  R_X86_64_PC32	.L__unnamed_2-0x4
     1db: e8 00 00 00 00               	callq	0x1e0 <main+0x140>
		00000000000001dc:  R_X86_64_PLT32	puts-0x4
     1e0: bb 02 00 00 00               	movl	$0x2, %ebx
     1e5: 89 d8                        	movl	%ebx, %eax
     1e7: 48 8d 65 d8                  	leaq	-0x28(%rbp), %rsp
     1eb: 5b                           	popq	%rbx
     1ec: 41 5c                        	popq	%r12
     1ee: 41 5d                        	popq	%r13
     1f0: 41 5e                        	popq	%r14
     1f2: 41 5f                        	popq	%r15
     1f4: 5d                           	popq	%rbp
     1f5: c3                           	retq
     1f6: 48 8b 05 00 00 00 00         	movq	(%rip), %rax            # 0x1fd <main+0x15d>
		00000000000001f9:  R_X86_64_REX_GOTPCRELX	stderr-0x4
     1fd: 4c 8b 30                     	movq	(%rax), %r14
     200: 4c 89 f6                     	movq	%r14, %rsi
     203: e8 00 00 00 00               	callq	0x208 <main+0x168>
		0000000000000204:  R_X86_64_PLT32	fputs-0x4
     208: 44 8b 7b 08                  	movl	0x8(%rbx), %r15d
     20c: 45 85 ff                     	testl	%r15d, %r15d
     20f: 74 2c                        	je	0x23d <main+0x19d>
     211: 41 83 ff 20                  	cmpl	$0x20, %r15d
     215: 41 bc 20 00 00 00            	movl	$0x20, %r12d
     21b: 45 0f 42 e7                  	cmovbl	%r15d, %r12d
     21f: 45 31 ed                     	xorl	%r13d, %r13d
     222: 4a 8b 7c eb 10               	movq	0x10(%rbx,%r13,8), %rdi
     227: 4c 89 f6                     	movq	%r14, %rsi
     22a: e8 00 00 00 00               	callq	0x22f <main+0x18f>
		000000000000022b:  R_X86_64_PLT32	fputs-0x4
     22f: 49 ff c5                     	incq	%r13
     232: 4d 39 ec                     	cmpq	%r13, %r12
     235: 75 eb                        	jne	0x222 <main+0x182>
     237: 41 83 ff 21                  	cmpl	$0x21, %r15d
     23b: 73 14                        	jae	0x251 <main+0x1b1>
     23d: bf 0a 00 00 00               	movl	$0xa, %edi
     242: 4c 89 f6                     	movq	%r14, %rsi
     245: e8 00 00 00 00               	callq	0x24a <main+0x1aa>
		0000000000000246:  R_X86_64_PLT32	fputc-0x4
     24a: bb 01 00 00 00               	movl	$0x1, %ebx
     24f: eb 94                        	jmp	0x1e5 <main+0x145>
     251: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x258 <main+0x1b8>
		0000000000000254:  R_X86_64_PC32	.L__unnamed_3-0x4
     258: be 2a 00 00 00               	movl	$0x2a, %esi
     25d: ba 01 00 00 00               	movl	$0x1, %edx
     262: 4c 89 f1                     	movq	%r14, %rcx
     265: e8 00 00 00 00               	callq	0x26a <main+0x1ca>
		0000000000000266:  R_X86_64_PLT32	fwrite-0x4
     26a: eb d1                        	jmp	0x23d <main+0x19d>
