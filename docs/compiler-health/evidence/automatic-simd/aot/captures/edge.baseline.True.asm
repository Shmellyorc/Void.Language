
/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/captures/edge.baseline.True.o:	file format elf64-x86-64

Disassembly of section .text:

0000000000000000 <__vd_2f656467652e766f69643a3a656e747279__void>:
       0: f3 0f 10 05 00 00 00 00      	movss	(%rip), %xmm0           # 0x8 <__vd_2f656467652e766f69643a3a656e747279__void+0x8>
		0000000000000004:  R_X86_64_PC32	.LCPI0_0-0x4
       8: b8 95 00 00 00               	movl	$0x95, %eax
       d: f3 0f 10 0d 00 00 00 00      	movss	(%rip), %xmm1           # 0x15 <__vd_2f656467652e766f69643a3a656e747279__void+0x15>
		0000000000000011:  R_X86_64_PC32	.LCPI0_1-0x4
      15: 66 66 2e 0f 1f 84 00 00 00 00 00     	nopw	%cs:(%rax,%rax)
      20: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
      24: ff c8                        	decl	%eax
      26: 75 f8                        	jne	0x20 <__vd_2f656467652e766f69643a3a656e747279__void+0x20>
      28: 85 ff                        	testl	%edi, %edi
      2a: 74 44                        	je	0x70 <__vd_2f656467652e766f69643a3a656e747279__void+0x70>
      2c: 89 f9                        	movl	%edi, %ecx
      2e: c1 f9 1f                     	sarl	$0x1f, %ecx
      31: 83 c9 01                     	orl	$0x1, %ecx
      34: 31 d2                        	xorl	%edx, %edx
      36: 48 c7 c0 ff ff ff ff         	movq	$-0x1, %rax
      3d: 0f 1f 00                     	nopl	(%rax)
      40: 01 ca                        	addl	%ecx, %edx
      42: 48 ff c0                     	incq	%rax
      45: 39 d7                        	cmpl	%edx, %edi
      47: 75 f7                        	jne	0x40 <__vd_2f656467652e766f69643a3a656e747279__void+0x40>
      49: 83 ff 01                     	cmpl	$0x1, %edi
      4c: 75 22                        	jne	0x70 <__vd_2f656467652e766f69643a3a656e747279__void+0x70>
      4e: 85 c0                        	testl	%eax, %eax
      50: 0f 95 c1                     	setne	%cl
      53: 48 85 c0                     	testq	%rax, %rax
      56: 0f 95 c2                     	setne	%dl
      59: 84 c0                        	testb	%al, %al
      5b: 40 0f 95 c6                  	setne	%sil
      5f: 40 08 ce                     	orb	%cl, %sil
      62: 40 08 d6                     	orb	%dl, %sil
      65: b8 02 00 00 00               	movl	$0x2, %eax
      6a: 40 80 fe 01                  	cmpb	$0x1, %sil
      6e: 74 16                        	je	0x86 <__vd_2f656467652e766f69643a3a656e747279__void+0x86>
      70: f3 0f 58 c0                  	addss	%xmm0, %xmm0
      74: b8 03 00 00 00               	movl	$0x3, %eax
      79: 0f 57 c9                     	xorps	%xmm1, %xmm1
      7c: 0f 2e c8                     	ucomiss	%xmm0, %xmm1
      7f: 73 05                        	jae	0x86 <__vd_2f656467652e766f69643a3a656e747279__void+0x86>
      81: b8 01 00 00 00               	movl	$0x1, %eax
      86: c3                           	retq
      87: 66 0f 1f 84 00 00 00 00 00   	nopw	(%rax,%rax)

0000000000000090 <main>:
      90: 55                           	pushq	%rbp
      91: 48 89 e5                     	movq	%rsp, %rbp
      94: 41 57                        	pushq	%r15
      96: 41 56                        	pushq	%r14
      98: 41 55                        	pushq	%r13
      9a: 41 54                        	pushq	%r12
      9c: 53                           	pushq	%rbx
      9d: 50                           	pushq	%rax
      9e: 45 31 f6                     	xorl	%r14d, %r14d
      a1: 83 ff 01                     	cmpl	$0x1, %edi
      a4: 74 50                        	je	0xf6 <main+0x66>
      a6: 89 fb                        	movl	%edi, %ebx
      a8: 49 89 e4                     	movq	%rsp, %r12
      ab: 49 8d 44 24 f0               	leaq	-0x10(%r12), %rax
      b0: 48 89 c4                     	movq	%rax, %rsp
      b3: 4c 8b 7e 08                  	movq	0x8(%rsi), %r15
      b7: 4c 89 ff                     	movq	%r15, %rdi
      ba: 48 89 c6                     	movq	%rax, %rsi
      bd: ba 0a 00 00 00               	movl	$0xa, %edx
      c2: e8 00 00 00 00               	callq	0xc7 <main+0x37>
		00000000000000c3:  R_X86_64_PLT32	strtoll-0x4
      c7: 49 89 c6                     	movq	%rax, %r14
      ca: 49 63 c6                     	movslq	%r14d, %rax
      cd: 4c 39 f0                     	cmpq	%r14, %rax
      d0: 0f 85 d5 00 00 00            	jne	0x1ab <main+0x11b>
      d6: 83 fb 02                     	cmpl	$0x2, %ebx
      d9: 0f 85 cc 00 00 00            	jne	0x1ab <main+0x11b>
      df: 49 8b 44 24 f0               	movq	-0x10(%r12), %rax
      e4: 4c 39 f8                     	cmpq	%r15, %rax
      e7: 0f 84 be 00 00 00            	je	0x1ab <main+0x11b>
      ed: 80 38 00                     	cmpb	$0x0, (%rax)
      f0: 0f 85 b5 00 00 00            	jne	0x1ab <main+0x11b>
      f6: 48 89 e3                     	movq	%rsp, %rbx
      f9: 48 81 c3 f0 fe ff ff         	addq	$-0x110, %rbx           # imm = 0xFEF0
     100: 48 89 dc                     	movq	%rbx, %rsp
     103: ba 10 01 00 00               	movl	$0x110, %edx            # imm = 0x110
     108: 48 89 df                     	movq	%rbx, %rdi
     10b: 31 f6                        	xorl	%esi, %esi
     10d: e8 00 00 00 00               	callq	0x112 <main+0x82>
		000000000000010e:  R_X86_64_PLT32	memset-0x4
     112: f3 0f 10 05 00 00 00 00      	movss	(%rip), %xmm0           # 0x11a <main+0x8a>
		0000000000000116:  R_X86_64_PC32	.LCPI1_0-0x4
     11a: b8 95 00 00 00               	movl	$0x95, %eax
     11f: f3 0f 10 0d 00 00 00 00      	movss	(%rip), %xmm1           # 0x127 <main+0x97>
		0000000000000123:  R_X86_64_PC32	.LCPI1_1-0x4
     127: 66 0f 1f 84 00 00 00 00 00   	nopw	(%rax,%rax)
     130: f3 0f 59 c1                  	mulss	%xmm1, %xmm0
     134: ff c8                        	decl	%eax
     136: 75 f8                        	jne	0x130 <main+0xa0>
     138: 45 85 f6                     	testl	%r14d, %r14d
     13b: 74 3e                        	je	0x17b <main+0xeb>
     13d: 44 89 f1                     	movl	%r14d, %ecx
     140: c1 f9 1f                     	sarl	$0x1f, %ecx
     143: 83 c9 01                     	orl	$0x1, %ecx
     146: 31 d2                        	xorl	%edx, %edx
     148: 48 c7 c0 ff ff ff ff         	movq	$-0x1, %rax
     14f: 90                           	nop
     150: 01 ca                        	addl	%ecx, %edx
     152: 48 ff c0                     	incq	%rax
     155: 41 39 d6                     	cmpl	%edx, %r14d
     158: 75 f6                        	jne	0x150 <main+0xc0>
     15a: 41 83 fe 01                  	cmpl	$0x1, %r14d
     15e: 75 1b                        	jne	0x17b <main+0xeb>
     160: 85 c0                        	testl	%eax, %eax
     162: 0f 95 c1                     	setne	%cl
     165: 48 85 c0                     	testq	%rax, %rax
     168: 0f 95 c2                     	setne	%dl
     16b: 84 c0                        	testb	%al, %al
     16d: 0f 95 c0                     	setne	%al
     170: 08 c8                        	orb	%cl, %al
     172: 08 d0                        	orb	%dl, %al
     174: be 02 00 00 00               	movl	$0x2, %esi
     179: 75 16                        	jne	0x191 <main+0x101>
     17b: f3 0f 58 c0                  	addss	%xmm0, %xmm0
     17f: be 03 00 00 00               	movl	$0x3, %esi
     184: 0f 57 c9                     	xorps	%xmm1, %xmm1
     187: 0f 2e c8                     	ucomiss	%xmm0, %xmm1
     18a: 73 05                        	jae	0x191 <main+0x101>
     18c: be 01 00 00 00               	movl	$0x1, %esi
     191: 48 8b 3b                     	movq	(%rbx), %rdi
     194: 48 85 ff                     	testq	%rdi, %rdi
     197: 75 34                        	jne	0x1cd <main+0x13d>
     199: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x1a0 <main+0x110>
		000000000000019c:  R_X86_64_PC32	.L__unnamed_1-0x4
     1a0: 31 db                        	xorl	%ebx, %ebx
     1a2: 31 c0                        	xorl	%eax, %eax
     1a4: e8 00 00 00 00               	callq	0x1a9 <main+0x119>
		00000000000001a5:  R_X86_64_PLT32	printf-0x4
     1a9: eb 11                        	jmp	0x1bc <main+0x12c>
     1ab: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x1b2 <main+0x122>
		00000000000001ae:  R_X86_64_PC32	.L__unnamed_2-0x4
     1b2: e8 00 00 00 00               	callq	0x1b7 <main+0x127>
		00000000000001b3:  R_X86_64_PLT32	puts-0x4
     1b7: bb 02 00 00 00               	movl	$0x2, %ebx
     1bc: 89 d8                        	movl	%ebx, %eax
     1be: 48 8d 65 d8                  	leaq	-0x28(%rbp), %rsp
     1c2: 5b                           	popq	%rbx
     1c3: 41 5c                        	popq	%r12
     1c5: 41 5d                        	popq	%r13
     1c7: 41 5e                        	popq	%r14
     1c9: 41 5f                        	popq	%r15
     1cb: 5d                           	popq	%rbp
     1cc: c3                           	retq
     1cd: 48 8b 05 00 00 00 00         	movq	(%rip), %rax            # 0x1d4 <main+0x144>
		00000000000001d0:  R_X86_64_REX_GOTPCRELX	stderr-0x4
     1d4: 4c 8b 30                     	movq	(%rax), %r14
     1d7: 4c 89 f6                     	movq	%r14, %rsi
     1da: e8 00 00 00 00               	callq	0x1df <main+0x14f>
		00000000000001db:  R_X86_64_PLT32	fputs-0x4
     1df: 44 8b 7b 08                  	movl	0x8(%rbx), %r15d
     1e3: 45 85 ff                     	testl	%r15d, %r15d
     1e6: 74 2c                        	je	0x214 <main+0x184>
     1e8: 41 83 ff 20                  	cmpl	$0x20, %r15d
     1ec: 41 bc 20 00 00 00            	movl	$0x20, %r12d
     1f2: 45 0f 42 e7                  	cmovbl	%r15d, %r12d
     1f6: 45 31 ed                     	xorl	%r13d, %r13d
     1f9: 4a 8b 7c eb 10               	movq	0x10(%rbx,%r13,8), %rdi
     1fe: 4c 89 f6                     	movq	%r14, %rsi
     201: e8 00 00 00 00               	callq	0x206 <main+0x176>
		0000000000000202:  R_X86_64_PLT32	fputs-0x4
     206: 49 ff c5                     	incq	%r13
     209: 4d 39 ec                     	cmpq	%r13, %r12
     20c: 75 eb                        	jne	0x1f9 <main+0x169>
     20e: 41 83 ff 21                  	cmpl	$0x21, %r15d
     212: 73 14                        	jae	0x228 <main+0x198>
     214: bf 0a 00 00 00               	movl	$0xa, %edi
     219: 4c 89 f6                     	movq	%r14, %rsi
     21c: e8 00 00 00 00               	callq	0x221 <main+0x191>
		000000000000021d:  R_X86_64_PLT32	fputc-0x4
     221: bb 01 00 00 00               	movl	$0x1, %ebx
     226: eb 94                        	jmp	0x1bc <main+0x12c>
     228: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x22f <main+0x19f>
		000000000000022b:  R_X86_64_PC32	.L__unnamed_3-0x4
     22f: be 2a 00 00 00               	movl	$0x2a, %esi
     234: ba 01 00 00 00               	movl	$0x1, %edx
     239: 4c 89 f1                     	movq	%r14, %rcx
     23c: e8 00 00 00 00               	callq	0x241 <main+0x1b1>
		000000000000023d:  R_X86_64_PLT32	fwrite-0x4
     241: eb d1                        	jmp	0x214 <main+0x184>
