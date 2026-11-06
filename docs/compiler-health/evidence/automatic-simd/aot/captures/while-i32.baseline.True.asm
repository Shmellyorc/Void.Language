
/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/captures/while-i32.baseline.True.o:	file format elf64-x86-64

Disassembly of section .text:

0000000000000000 <__vd_2f7768696c652d6933322e766f69643a3a656e747279__void>:
       0: 85 ff                        	testl	%edi, %edi
       2: 7e 4c                        	jle	0x50 <__vd_2f7768696c652d6933322e766f69643a3a656e747279__void+0x50>
       4: 31 d2                        	xorl	%edx, %edx
       6: be ff ff ff ff               	movl	$0xffffffff, %esi       # imm = 0xFFFFFFFF
       b: 49 b8 00 00 00 22 10 08 04 02	movabsq	$0x204081022000000, %r8 # imm = 0x204081022000000
      15: 45 31 c9                     	xorl	%r9d, %r9d
      18: 0f 1f 84 00 00 00 00 00      	nopl	(%rax,%rax)
      20: 89 d1                        	movl	%edx, %ecx
      22: 44 89 c8                     	movl	%r9d, %eax
      25: 49 f7 e0                     	mulq	%r8
      28: 89 d0                        	movl	%edx, %eax
      2a: c1 e0 07                     	shll	$0x7, %eax
      2d: 29 c2                        	subl	%eax, %edx
      2f: 01 ca                        	addl	%ecx, %edx
      31: 44 01 ca                     	addl	%r9d, %edx
      34: 41 ff c1                     	incl	%r9d
      37: ff c6                        	incl	%esi
      39: ff cf                        	decl	%edi
      3b: 75 e3                        	jne	0x20 <__vd_2f7768696c652d6933322e766f69643a3a656e747279__void+0x20>
      3d: 89 f0                        	movl	%esi, %eax
      3f: 49 f7 e0                     	mulq	%r8
      42: 89 d0                        	movl	%edx, %eax
      44: c1 e0 07                     	shll	$0x7, %eax
      47: 29 c2                        	subl	%eax, %edx
      49: 01 f1                        	addl	%esi, %ecx
      4b: 01 d1                        	addl	%edx, %ecx
      4d: 89 c8                        	movl	%ecx, %eax
      4f: c3                           	retq
      50: 31 c9                        	xorl	%ecx, %ecx
      52: 89 c8                        	movl	%ecx, %eax
      54: c3                           	retq
      55: 66 66 2e 0f 1f 84 00 00 00 00 00     	nopw	%cs:(%rax,%rax)

0000000000000060 <main>:
      60: 55                           	pushq	%rbp
      61: 48 89 e5                     	movq	%rsp, %rbp
      64: 41 57                        	pushq	%r15
      66: 41 56                        	pushq	%r14
      68: 41 55                        	pushq	%r13
      6a: 41 54                        	pushq	%r12
      6c: 53                           	pushq	%rbx
      6d: 50                           	pushq	%rax
      6e: 48 89 f0                     	movq	%rsi, %rax
      71: 31 db                        	xorl	%ebx, %ebx
      73: be 00 00 00 00               	movl	$0x0, %esi
      78: 83 ff 01                     	cmpl	$0x1, %edi
      7b: 0f 84 c2 00 00 00            	je	0x143 <main+0xe3>
      81: 41 89 fe                     	movl	%edi, %r14d
      84: 49 89 e5                     	movq	%rsp, %r13
      87: 49 8d 75 f0                  	leaq	-0x10(%r13), %rsi
      8b: 48 89 f4                     	movq	%rsi, %rsp
      8e: 4c 8b 60 08                  	movq	0x8(%rax), %r12
      92: 4c 89 e7                     	movq	%r12, %rdi
      95: ba 0a 00 00 00               	movl	$0xa, %edx
      9a: e8 00 00 00 00               	callq	0x9f <main+0x3f>
		000000000000009b:  R_X86_64_PLT32	strtoll-0x4
      9f: 49 89 c7                     	movq	%rax, %r15
      a2: 49 63 c7                     	movslq	%r15d, %rax
      a5: 4c 39 f8                     	cmpq	%r15, %rax
      a8: 0f 85 a5 00 00 00            	jne	0x153 <main+0xf3>
      ae: 41 83 fe 02                  	cmpl	$0x2, %r14d
      b2: 0f 85 9b 00 00 00            	jne	0x153 <main+0xf3>
      b8: 49 8b 45 f0                  	movq	-0x10(%r13), %rax
      bc: 4c 39 e0                     	cmpq	%r12, %rax
      bf: 0f 84 8e 00 00 00            	je	0x153 <main+0xf3>
      c5: 80 38 00                     	cmpb	$0x0, (%rax)
      c8: 0f 85 85 00 00 00            	jne	0x153 <main+0xf3>
      ce: 49 89 e6                     	movq	%rsp, %r14
      d1: 49 81 c6 f0 fe ff ff         	addq	$-0x110, %r14           # imm = 0xFEF0
      d8: 4c 89 f4                     	movq	%r14, %rsp
      db: ba 10 01 00 00               	movl	$0x110, %edx            # imm = 0x110
      e0: 4c 89 f7                     	movq	%r14, %rdi
      e3: 31 f6                        	xorl	%esi, %esi
      e5: e8 00 00 00 00               	callq	0xea <main+0x8a>
		00000000000000e6:  R_X86_64_PLT32	memset-0x4
      ea: 31 f6                        	xorl	%esi, %esi
      ec: 4d 85 ff                     	testq	%r15, %r15
      ef: 7e 52                        	jle	0x143 <main+0xe3>
      f1: 31 d2                        	xorl	%edx, %edx
      f3: be ff ff ff ff               	movl	$0xffffffff, %esi       # imm = 0xFFFFFFFF
      f8: 49 b8 00 00 00 22 10 08 04 02	movabsq	$0x204081022000000, %r8 # imm = 0x204081022000000
     102: 31 ff                        	xorl	%edi, %edi
     104: 66 66 66 2e 0f 1f 84 00 00 00 00 00  	nopw	%cs:(%rax,%rax)
     110: 89 d1                        	movl	%edx, %ecx
     112: 89 f8                        	movl	%edi, %eax
     114: 49 f7 e0                     	mulq	%r8
     117: 89 d0                        	movl	%edx, %eax
     119: c1 e0 07                     	shll	$0x7, %eax
     11c: 29 c2                        	subl	%eax, %edx
     11e: 01 ca                        	addl	%ecx, %edx
     120: 01 fa                        	addl	%edi, %edx
     122: ff c7                        	incl	%edi
     124: ff c6                        	incl	%esi
     126: 41 ff cf                     	decl	%r15d
     129: 75 e5                        	jne	0x110 <main+0xb0>
     12b: 49 8b 3e                     	movq	(%r14), %rdi
     12e: 89 f0                        	movl	%esi, %eax
     130: 49 f7 e0                     	mulq	%r8
     133: 48 85 ff                     	testq	%rdi, %rdi
     136: 75 3d                        	jne	0x175 <main+0x115>
     138: 89 d0                        	movl	%edx, %eax
     13a: c1 e0 07                     	shll	$0x7, %eax
     13d: 29 c2                        	subl	%eax, %edx
     13f: 01 ce                        	addl	%ecx, %esi
     141: 01 d6                        	addl	%edx, %esi
     143: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x14a <main+0xea>
		0000000000000146:  R_X86_64_PC32	.L__unnamed_1-0x4
     14a: 31 c0                        	xorl	%eax, %eax
     14c: e8 00 00 00 00               	callq	0x151 <main+0xf1>
		000000000000014d:  R_X86_64_PLT32	printf-0x4
     151: eb 11                        	jmp	0x164 <main+0x104>
     153: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x15a <main+0xfa>
		0000000000000156:  R_X86_64_PC32	.L__unnamed_2-0x4
     15a: e8 00 00 00 00               	callq	0x15f <main+0xff>
		000000000000015b:  R_X86_64_PLT32	puts-0x4
     15f: bb 02 00 00 00               	movl	$0x2, %ebx
     164: 89 d8                        	movl	%ebx, %eax
     166: 48 8d 65 d8                  	leaq	-0x28(%rbp), %rsp
     16a: 5b                           	popq	%rbx
     16b: 41 5c                        	popq	%r12
     16d: 41 5d                        	popq	%r13
     16f: 41 5e                        	popq	%r14
     171: 41 5f                        	popq	%r15
     173: 5d                           	popq	%rbp
     174: c3                           	retq
     175: 48 8b 05 00 00 00 00         	movq	(%rip), %rax            # 0x17c <main+0x11c>
		0000000000000178:  R_X86_64_REX_GOTPCRELX	stderr-0x4
     17c: 48 8b 18                     	movq	(%rax), %rbx
     17f: 48 89 de                     	movq	%rbx, %rsi
     182: e8 00 00 00 00               	callq	0x187 <main+0x127>
		0000000000000183:  R_X86_64_PLT32	fputs-0x4
     187: 45 8b 7e 08                  	movl	0x8(%r14), %r15d
     18b: 45 85 ff                     	testl	%r15d, %r15d
     18e: 74 2c                        	je	0x1bc <main+0x15c>
     190: 41 83 ff 20                  	cmpl	$0x20, %r15d
     194: 41 bc 20 00 00 00            	movl	$0x20, %r12d
     19a: 45 0f 42 e7                  	cmovbl	%r15d, %r12d
     19e: 45 31 ed                     	xorl	%r13d, %r13d
     1a1: 4b 8b 7c ee 10               	movq	0x10(%r14,%r13,8), %rdi
     1a6: 48 89 de                     	movq	%rbx, %rsi
     1a9: e8 00 00 00 00               	callq	0x1ae <main+0x14e>
		00000000000001aa:  R_X86_64_PLT32	fputs-0x4
     1ae: 49 ff c5                     	incq	%r13
     1b1: 4d 39 ec                     	cmpq	%r13, %r12
     1b4: 75 eb                        	jne	0x1a1 <main+0x141>
     1b6: 41 83 ff 21                  	cmpl	$0x21, %r15d
     1ba: 73 14                        	jae	0x1d0 <main+0x170>
     1bc: bf 0a 00 00 00               	movl	$0xa, %edi
     1c1: 48 89 de                     	movq	%rbx, %rsi
     1c4: e8 00 00 00 00               	callq	0x1c9 <main+0x169>
		00000000000001c5:  R_X86_64_PLT32	fputc-0x4
     1c9: bb 01 00 00 00               	movl	$0x1, %ebx
     1ce: eb 94                        	jmp	0x164 <main+0x104>
     1d0: 48 8d 3d 00 00 00 00         	leaq	(%rip), %rdi            # 0x1d7 <main+0x177>
		00000000000001d3:  R_X86_64_PC32	.L__unnamed_3-0x4
     1d7: be 2a 00 00 00               	movl	$0x2a, %esi
     1dc: ba 01 00 00 00               	movl	$0x1, %edx
     1e1: 48 89 d9                     	movq	%rbx, %rcx
     1e4: e8 00 00 00 00               	callq	0x1e9 <main+0x189>
		00000000000001e5:  R_X86_64_PLT32	fwrite-0x4
     1e9: eb d1                        	jmp	0x1bc <main+0x15c>
