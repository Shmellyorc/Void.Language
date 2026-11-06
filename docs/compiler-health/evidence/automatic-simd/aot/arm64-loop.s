	.file	"guarded-range-i32.void"
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0                          // -- Begin function __vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void
.LCPI0_0:
	.word	0                               // 0x0
	.word	1                               // 0x1
	.word	2                               // 0x2
	.word	3                               // 0x3
	.text
	.globl	__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void
	.p2align	2
	.type	__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void,@function
__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void: // @__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void
.Lfunc_begin0:
	.cfi_sections .debug_frame
	.cfi_startproc
// %bb.0:                               // %block0
	.file	1 "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures" "guarded-range-i32.void"
	.loc	1 2 10 prologue_end             // guarded-range-i32.void:2:10
	cmp	w0, #1
	b.lt	.LBB0_3
// %bb.1:                               // %iter.check
	.loc	1 5 5                           // guarded-range-i32.void:5:5
	cmp	w0, #3
	b.hi	.LBB0_4
// %bb.2:
	.loc	1 0 5 is_stmt 0                 // guarded-range-i32.void:0:5
	mov	w9, wzr
	mov	w8, wzr
	b	.LBB0_14
.LBB0_3:
	mov	w0, wzr
	ret
.LBB0_4:                                // %vector.main.loop.iter.check
	.loc	1 5 5                           // guarded-range-i32.void:5:5
	cmp	w0, #16
	adrp	x10, .LCPI0_0
	b.hs	.LBB0_6
// %bb.5:
	.loc	1 0 5                           // guarded-range-i32.void:0:5
	mov	w9, wzr
	mov	w8, wzr
	.loc	1 5 5                           // guarded-range-i32.void:5:5
	b	.LBB0_10
.LBB0_6:                                // %vector.ph
	.loc	1 0 5                           // guarded-range-i32.void:0:5
	mov	w8, #2065                       // =0x811
	movi	v0.2d, #0000000000000000
	movi	v1.4s, #4
	movk	w8, #516, lsl #16
	movi	v2.4s, #8
	movi	v3.4s, #12
	movi	v4.4s, #127
	movi	v5.4s, #16
	ldr	q18, [x10, :lo12:.LCPI0_0]
	movi	v6.2d, #0000000000000000
	movi	v7.2d, #0000000000000000
	and	w11, w0, #0xc
	movi	v16.2d, #0000000000000000
	dup	v17.4s, w8
	and	w9, w0, #0x7ffffff0
	and	w8, w0, #0x7ffffff0
.LBB0_7:                                // %vector.body
                                        // =>This Inner Loop Header: Depth=1
	add	v19.4s, v18.4s, v1.4s
	add	v20.4s, v18.4s, v2.4s
	.loc	1 5 5 is_stmt 1                 // guarded-range-i32.void:5:5
	subs	w8, w8, #16
	add	v21.4s, v18.4s, v3.4s
	.loc	1 6 20                          // guarded-range-i32.void:6:20
	umull2	v22.2d, v18.4s, v17.4s
	umull	v23.2d, v18.2s, v17.2s
	umull2	v24.2d, v19.4s, v17.4s
	umull	v25.2d, v19.2s, v17.2s
	umull2	v26.2d, v20.4s, v17.4s
	umull	v27.2d, v20.2s, v17.2s
	umull2	v28.2d, v21.4s, v17.4s
	umull	v29.2d, v21.2s, v17.2s
	uzp2	v22.4s, v23.4s, v22.4s
	uzp2	v23.4s, v25.4s, v24.4s
	uzp2	v24.4s, v27.4s, v26.4s
	uzp2	v25.4s, v29.4s, v28.4s
	sub	v26.4s, v18.4s, v22.4s
	sub	v27.4s, v19.4s, v23.4s
	sub	v28.4s, v20.4s, v24.4s
	sub	v29.4s, v21.4s, v25.4s
	usra	v22.4s, v26.4s, #1
	mov	v26.16b, v18.16b
	add	v18.4s, v18.4s, v5.4s
	usra	v23.4s, v27.4s, #1
	usra	v24.4s, v28.4s, #1
	usra	v25.4s, v29.4s, #1
	ushr	v22.4s, v22.4s, #6
	ushr	v23.4s, v23.4s, #6
	ushr	v24.4s, v24.4s, #6
	ushr	v25.4s, v25.4s, #6
	mls	v26.4s, v22.4s, v4.4s
	mls	v19.4s, v23.4s, v4.4s
	mls	v20.4s, v24.4s, v4.4s
	mls	v21.4s, v25.4s, v4.4s
	.loc	1 6 9 is_stmt 0                 // guarded-range-i32.void:6:9
	add	v0.4s, v26.4s, v0.4s
	add	v6.4s, v19.4s, v6.4s
	add	v7.4s, v20.4s, v7.4s
	add	v16.4s, v21.4s, v16.4s
	.loc	1 5 5 is_stmt 1                 // guarded-range-i32.void:5:5
	b.ne	.LBB0_7
// %bb.8:                               // %middle.block
	add	v0.4s, v6.4s, v0.4s
	cmp	w0, w9
	add	v0.4s, v7.4s, v0.4s
	add	v0.4s, v16.4s, v0.4s
	addv	s0, v0.4s
	fmov	w8, s0
	b.eq	.LBB0_13
// %bb.9:                               // %vec.epilog.iter.check
	.loc	1 0 5 is_stmt 0                 // guarded-range-i32.void:0:5
	cbz	w11, .LBB0_14
.LBB0_10:                               // %vec.epilog.ph
	movi	v0.2d, #0000000000000000
	dup	v3.4s, w9
	ldr	q4, [x10, :lo12:.LCPI0_0]
	movi	v1.4s, #127
	movi	v2.4s, #4
	mov	w11, w9
	and	w9, w0, #0x7ffffffc
	orr	v4.16b, v3.16b, v4.16b
	mov	v0.s[0], w8
	mov	w8, #2065                       // =0x811
	movk	w8, #516, lsl #16
	dup	v3.4s, w8
	sub	w8, w11, w9
.LBB0_11:                               // %vec.epilog.vector.body
                                        // =>This Inner Loop Header: Depth=1
	.loc	1 6 20 is_stmt 1                // guarded-range-i32.void:6:20
	umull2	v5.2d, v4.4s, v3.4s
	umull	v6.2d, v4.2s, v3.2s
	.loc	1 5 5                           // guarded-range-i32.void:5:5
	adds	w8, w8, #4
	.loc	1 6 20                          // guarded-range-i32.void:6:20
	uzp2	v5.4s, v6.4s, v5.4s
	sub	v6.4s, v4.4s, v5.4s
	usra	v5.4s, v6.4s, #1
	add	v6.4s, v4.4s, v2.4s
	ushr	v5.4s, v5.4s, #6
	mls	v4.4s, v5.4s, v1.4s
	.loc	1 6 9 is_stmt 0                 // guarded-range-i32.void:6:9
	add	v0.4s, v4.4s, v0.4s
	mov	v4.16b, v6.16b
	.loc	1 5 5 is_stmt 1                 // guarded-range-i32.void:5:5
	b.ne	.LBB0_11
// %bb.12:                              // %vec.epilog.middle.block
	addv	s0, v0.4s
	cmp	w0, w9
	fmov	w8, s0
	b.ne	.LBB0_14
.LBB0_13:                               // %common.ret
	.loc	1 0 0 is_stmt 0                 // guarded-range-i32.void:0
	mov	w0, w8
	ret
.LBB0_14:                               // %block8.preheader
	mov	x12, #570425344                 // =0x22000000
	.loc	1 5 5 is_stmt 1                 // guarded-range-i32.void:5:5
	sub	w11, w0, w9
	sub	w10, w9, #1
	movk	x12, #2064, lsl #32
	movk	x12, #516, lsl #48
.LBB0_15:                               // %block8
                                        // =>This Inner Loop Header: Depth=1
	.loc	1 6 20                          // guarded-range-i32.void:6:20
	mov	w13, w9
	.loc	1 5 5                           // guarded-range-i32.void:5:5
	subs	w11, w11, #1
	add	w10, w10, #1
	.loc	1 6 20                          // guarded-range-i32.void:6:20
	umulh	x14, x13, x12
	mov	w13, w8
	sub	w8, w14, w14, lsl #7
	add	w14, w13, w9
	.loc	1 5 5                           // guarded-range-i32.void:5:5
	add	w9, w9, #1
	add	w8, w14, w8
	b.ne	.LBB0_15
// %bb.16:                              // %common.ret.loopexit
	.loc	1 0 0 is_stmt 0                 // guarded-range-i32.void:0
	umulh	x8, x10, x12
	add	w9, w10, w13
	sub	w8, w8, w8, lsl #7
	add	w0, w9, w8
	ret
.Ltmp0:
.Lfunc_end0:
	.size	__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void, .Lfunc_end0-__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void
	.cfi_endproc
                                        // -- End function
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0                          // -- Begin function main
.LCPI1_0:
	.word	0                               // 0x0
	.word	1                               // 0x1
	.word	2                               // 0x2
	.word	3                               // 0x3
	.text
	.globl	main
	.p2align	2
	.type	main,@function
main:                                   // @main
.Lfunc_begin1:
	.cfi_startproc
// %bb.0:                               // %entry
	stp	x29, x30, [sp, #-48]!           // 16-byte Folded Spill
	stp	x22, x21, [sp, #16]             // 16-byte Folded Spill
	stp	x20, x19, [sp, #32]             // 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 48
	.cfi_offset w19, -8
	.cfi_offset w20, -16
	.cfi_offset w21, -24
	.cfi_offset w22, -32
	.cfi_offset w30, -40
	.cfi_offset w29, -48
	cmp	w0, #1
	b.ne	.LBB1_3
.LBB1_1:
	mov	w1, wzr
.LBB1_2:                                // %success
	adrp	x0, .L__unnamed_1
	add	x0, x0, :lo12:.L__unnamed_1
	bl	printf
	mov	w0, wzr
	b	.LBB1_11
.LBB1_3:                                // %parse_input
	mov	x21, sp
	mov	w19, w0
	sub	x8, x21, #16
	mov	sp, x8
	ldr	x20, [x1, #8]
	sub	x1, x21, #16
	mov	w2, #10                         // =0xa
	mov	x0, x20
	bl	strtoll
	cmp	x0, w0, sxtw
	b.ne	.LBB1_10
// %bb.4:                               // %parse_input
	cmp	w19, #2
	b.ne	.LBB1_10
// %bb.5:                               // %parse_input
	ldur	x8, [x21, #-16]
	cmp	x8, x20
	b.eq	.LBB1_10
// %bb.6:                               // %parse_input
	ldrb	w8, [x8]
	cbnz	w8, .LBB1_10
// %bb.7:                               // %run
	mov	x8, sp
	sub	x20, x8, #272
	mov	sp, x20
	movi	v0.2d, #0000000000000000
	cmp	x0, #1
	stp	q0, q0, [x8, #-256]
	stp	q0, q0, [x8, #-224]
	stp	q0, q0, [x8, #-192]
	stp	q0, q0, [x8, #-160]
	stp	q0, q0, [x8, #-128]
	stp	q0, q0, [x8, #-96]
	stp	q0, q0, [x8, #-64]
	stp	q0, q0, [x8, #-32]
	str	q0, [x20]
	b.lt	.LBB1_1
// %bb.8:                               // %iter.check
	cmp	w0, #4
	b.hs	.LBB1_12
// %bb.9:
	mov	w8, wzr
	mov	w1, wzr
	b	.LBB1_21
.LBB1_10:                               // %invalid_input
	adrp	x0, .L__unnamed_2
	add	x0, x0, :lo12:.L__unnamed_2
	bl	puts
	mov	w0, #2                          // =0x2
.LBB1_11:                               // %common.ret
	mov	sp, x29
	ldp	x20, x19, [sp, #32]             // 16-byte Folded Reload
	ldp	x22, x21, [sp, #16]             // 16-byte Folded Reload
	ldp	x29, x30, [sp], #48             // 16-byte Folded Reload
	ret
.LBB1_12:                               // %vector.main.loop.iter.check
	cmp	w0, #16
	adrp	x9, .LCPI1_0
	b.hs	.LBB1_14
// %bb.13:
	mov	w8, wzr
	mov	w1, wzr
	b	.LBB1_18
.LBB1_14:                               // %vector.ph
	mov	w8, #2065                       // =0x811
	movi	v1.4s, #4
	movi	v2.4s, #8
	movk	w8, #516, lsl #16
	movi	v3.4s, #12
	movi	v4.4s, #127
	movi	v5.4s, #16
	movi	v6.2d, #0000000000000000
	ldr	q18, [x9, :lo12:.LCPI1_0]
	movi	v7.2d, #0000000000000000
	movi	v16.2d, #0000000000000000
	and	w10, w0, #0xc
	dup	v17.4s, w8
	and	w8, w0, #0xfffffff0
	and	w11, w0, #0xfffffff0
.LBB1_15:                               // %vector.body
                                        // =>This Inner Loop Header: Depth=1
	add	v19.4s, v18.4s, v1.4s
	add	v20.4s, v18.4s, v2.4s
	subs	w11, w11, #16
	add	v21.4s, v18.4s, v3.4s
	umull2	v22.2d, v18.4s, v17.4s
	umull	v23.2d, v18.2s, v17.2s
	umull2	v24.2d, v19.4s, v17.4s
	umull	v25.2d, v19.2s, v17.2s
	umull2	v26.2d, v20.4s, v17.4s
	umull	v27.2d, v20.2s, v17.2s
	umull2	v28.2d, v21.4s, v17.4s
	umull	v29.2d, v21.2s, v17.2s
	uzp2	v22.4s, v23.4s, v22.4s
	uzp2	v23.4s, v25.4s, v24.4s
	uzp2	v24.4s, v27.4s, v26.4s
	uzp2	v25.4s, v29.4s, v28.4s
	sub	v26.4s, v18.4s, v22.4s
	sub	v27.4s, v19.4s, v23.4s
	sub	v28.4s, v20.4s, v24.4s
	sub	v29.4s, v21.4s, v25.4s
	usra	v22.4s, v26.4s, #1
	mov	v26.16b, v18.16b
	add	v18.4s, v18.4s, v5.4s
	usra	v23.4s, v27.4s, #1
	usra	v24.4s, v28.4s, #1
	usra	v25.4s, v29.4s, #1
	ushr	v22.4s, v22.4s, #6
	ushr	v23.4s, v23.4s, #6
	ushr	v24.4s, v24.4s, #6
	ushr	v25.4s, v25.4s, #6
	mls	v26.4s, v22.4s, v4.4s
	mls	v19.4s, v23.4s, v4.4s
	mls	v20.4s, v24.4s, v4.4s
	mls	v21.4s, v25.4s, v4.4s
	add	v0.4s, v26.4s, v0.4s
	add	v6.4s, v19.4s, v6.4s
	add	v7.4s, v20.4s, v7.4s
	add	v16.4s, v21.4s, v16.4s
	b.ne	.LBB1_15
// %bb.16:                              // %middle.block
	add	v0.4s, v6.4s, v0.4s
	cmp	w8, w0
	add	v0.4s, v7.4s, v0.4s
	add	v0.4s, v16.4s, v0.4s
	addv	s0, v0.4s
	fmov	w1, s0
	b.eq	.LBB1_24
// %bb.17:                              // %vec.epilog.iter.check
	cbz	w10, .LBB1_21
.LBB1_18:                               // %vec.epilog.ph
	movi	v0.2d, #0000000000000000
	dup	v3.4s, w8
	mov	w10, w8
	ldr	q4, [x9, :lo12:.LCPI1_0]
	mov	w8, #2065                       // =0x811
	movi	v1.4s, #127
	movk	w8, #516, lsl #16
	movi	v2.4s, #4
	orr	v4.16b, v3.16b, v4.16b
	dup	v3.4s, w8
	and	w8, w0, #0xfffffffc
	mov	v0.s[0], w1
	sub	w9, w10, w8
.LBB1_19:                               // %vec.epilog.vector.body
                                        // =>This Inner Loop Header: Depth=1
	umull2	v5.2d, v4.4s, v3.4s
	umull	v6.2d, v4.2s, v3.2s
	adds	w9, w9, #4
	uzp2	v5.4s, v6.4s, v5.4s
	sub	v6.4s, v4.4s, v5.4s
	usra	v5.4s, v6.4s, #1
	add	v6.4s, v4.4s, v2.4s
	ushr	v5.4s, v5.4s, #6
	mls	v4.4s, v5.4s, v1.4s
	add	v0.4s, v4.4s, v0.4s
	mov	v4.16b, v6.16b
	b.ne	.LBB1_19
// %bb.20:                              // %vec.epilog.middle.block
	addv	s0, v0.4s
	cmp	w8, w0
	fmov	w1, s0
	b.eq	.LBB1_24
.LBB1_21:                               // %block8.i.preheader
	mov	x11, #570425344                 // =0x22000000
	sub	w10, w0, w8
	sub	w9, w8, #1
	movk	x11, #2064, lsl #32
	movk	x11, #516, lsl #48
.LBB1_22:                               // %block8.i
                                        // =>This Inner Loop Header: Depth=1
	mov	w12, w8
	add	w14, w1, w8
	subs	w10, w10, #1
	umulh	x13, x12, x11
	mov	w12, w1
	add	w8, w8, #1
	add	w9, w9, #1
	sub	w13, w13, w13, lsl #7
	add	w1, w14, w13
	b.ne	.LBB1_22
// %bb.23:                              // %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit.loopexit
	umulh	x8, x9, x11
	add	w9, w9, w12
	sub	w8, w8, w8, lsl #7
	add	w1, w9, w8
.LBB1_24:                               // %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit
	ldr	x0, [x20]
	cbz	x0, .LBB1_2
// %bb.25:                              // %runtime_error
	adrp	x8, :got:stderr
	ldr	x8, [x8, :got_lo12:stderr]
	ldr	x19, [x8]
	mov	x1, x19
	bl	fputs
	ldr	w21, [x20, #8]
	cbz	w21, .LBB1_29
// %bb.26:                              // %print_caller.lr.ph
	cmp	w21, #32
	mov	w8, #32                         // =0x20
	add	x20, x20, #16
	csel	w22, w21, w8, lo
.LBB1_27:                               // %print_caller
                                        // =>This Inner Loop Header: Depth=1
	ldr	x0, [x20], #8
	mov	x1, x19
	bl	fputs
	subs	x22, x22, #1
	b.ne	.LBB1_27
// %bb.28:                              // %failure_exit
	cmp	w21, #33
	b.hs	.LBB1_30
.LBB1_29:                               // %exit
	mov	w0, #10                         // =0xa
	mov	x1, x19
	bl	fputc
	mov	w0, #1                          // =0x1
	b	.LBB1_11
.LBB1_30:                               // %trace_limit
	adrp	x0, .L__unnamed_3
	add	x0, x0, :lo12:.L__unnamed_3
	mov	w1, #42                         // =0x2a
	mov	w2, #1                          // =0x1
	mov	x3, x19
	bl	fwrite
	b	.LBB1_29
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc
                                        // -- End function
	.type	__vd_types__void,@object        // @__vd_types__void
	.section	.rodata,"a",@progbits
	.globl	__vd_types__void
	.p2align	3, 0x0
__vd_types__void:
	.size	__vd_types__void, 0

	.type	__vd_type_count__void,@object   // @__vd_type_count__void
	.globl	__vd_type_count__void
	.p2align	3, 0x0
__vd_type_count__void:
	.xword	0                               // 0x0
	.size	__vd_type_count__void, 8

	.type	__vd_witnesses__void,@object    // @__vd_witnesses__void
	.globl	__vd_witnesses__void
	.p2align	3, 0x0
__vd_witnesses__void:
	.size	__vd_witnesses__void, 0

	.type	__vd_witness_count__void,@object // @__vd_witness_count__void
	.globl	__vd_witness_count__void
	.p2align	3, 0x0
__vd_witness_count__void:
	.xword	0                               // 0x0
	.size	__vd_witness_count__void, 8

	.type	.L__unnamed_2,@object           // @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_2:
	.asciz	"VOID: expected one optional decimal i32 input"
	.size	.L__unnamed_2, 46

	.type	.L__unnamed_3,@object           // @1
.L__unnamed_3:
	.asciz	"\nSource call trace limited to 32 callers.\n"
	.size	.L__unnamed_3, 43

	.type	.L__unnamed_1,@object           // @2
.L__unnamed_1:
	.asciz	"%d\n"
	.size	.L__unnamed_1, 4

	.section	.debug_abbrev,"",@progbits
	.byte	1                               // Abbreviation Code
	.byte	17                              // DW_TAG_compile_unit
	.byte	1                               // DW_CHILDREN_yes
	.byte	37                              // DW_AT_producer
	.byte	14                              // DW_FORM_strp
	.byte	19                              // DW_AT_language
	.byte	5                               // DW_FORM_data2
	.byte	3                               // DW_AT_name
	.byte	14                              // DW_FORM_strp
	.byte	16                              // DW_AT_stmt_list
	.byte	23                              // DW_FORM_sec_offset
	.byte	27                              // DW_AT_comp_dir
	.byte	14                              // DW_FORM_strp
	.ascii	"\264B"                         // DW_AT_GNU_pubnames
	.byte	25                              // DW_FORM_flag_present
	.byte	17                              // DW_AT_low_pc
	.byte	1                               // DW_FORM_addr
	.byte	18                              // DW_AT_high_pc
	.byte	6                               // DW_FORM_data4
	.byte	0                               // EOM(1)
	.byte	0                               // EOM(2)
	.byte	2                               // Abbreviation Code
	.byte	46                              // DW_TAG_subprogram
	.byte	0                               // DW_CHILDREN_no
	.byte	17                              // DW_AT_low_pc
	.byte	1                               // DW_FORM_addr
	.byte	18                              // DW_AT_high_pc
	.byte	6                               // DW_FORM_data4
	.byte	64                              // DW_AT_frame_base
	.byte	24                              // DW_FORM_exprloc
	.byte	110                             // DW_AT_linkage_name
	.byte	14                              // DW_FORM_strp
	.byte	3                               // DW_AT_name
	.byte	14                              // DW_FORM_strp
	.byte	58                              // DW_AT_decl_file
	.byte	11                              // DW_FORM_data1
	.byte	59                              // DW_AT_decl_line
	.byte	11                              // DW_FORM_data1
	.byte	39                              // DW_AT_prototyped
	.byte	25                              // DW_FORM_flag_present
	.byte	73                              // DW_AT_type
	.byte	19                              // DW_FORM_ref4
	.byte	63                              // DW_AT_external
	.byte	25                              // DW_FORM_flag_present
	.byte	0                               // EOM(1)
	.byte	0                               // EOM(2)
	.byte	3                               // Abbreviation Code
	.byte	36                              // DW_TAG_base_type
	.byte	0                               // DW_CHILDREN_no
	.byte	3                               // DW_AT_name
	.byte	14                              // DW_FORM_strp
	.byte	62                              // DW_AT_encoding
	.byte	11                              // DW_FORM_data1
	.byte	11                              // DW_AT_byte_size
	.byte	11                              // DW_FORM_data1
	.byte	0                               // EOM(1)
	.byte	0                               // EOM(2)
	.byte	0                               // EOM(3)
	.section	.debug_info,"",@progbits
.Lcu_begin0:
	.word	.Ldebug_info_end0-.Ldebug_info_start0 // Length of Unit
.Ldebug_info_start0:
	.hword	4                               // DWARF version number
	.word	.debug_abbrev                   // Offset Into Abbrev. Section
	.byte	8                               // Address Size (in bytes)
	.byte	1                               // Abbrev [1] 0xb:0x44 DW_TAG_compile_unit
	.word	.Linfo_string0                  // DW_AT_producer
	.hword	2                               // DW_AT_language
	.word	.Linfo_string1                  // DW_AT_name
	.word	.Lline_table_start0             // DW_AT_stmt_list
	.word	.Linfo_string2                  // DW_AT_comp_dir
                                        // DW_AT_GNU_pubnames
	.xword	.Lfunc_begin0                   // DW_AT_low_pc
	.word	.Lfunc_end0-.Lfunc_begin0       // DW_AT_high_pc
	.byte	2                               // Abbrev [2] 0x2a:0x1d DW_TAG_subprogram
	.xword	.Lfunc_begin0                   // DW_AT_low_pc
	.word	.Lfunc_end0-.Lfunc_begin0       // DW_AT_high_pc
	.byte	1                               // DW_AT_frame_base
	.byte	111
	.word	.Linfo_string3                  // DW_AT_linkage_name
	.word	.Linfo_string4                  // DW_AT_name
	.byte	1                               // DW_AT_decl_file
	.byte	1                               // DW_AT_decl_line
                                        // DW_AT_prototyped
	.word	71                              // DW_AT_type
                                        // DW_AT_external
	.byte	3                               // Abbrev [3] 0x47:0x7 DW_TAG_base_type
	.word	.Linfo_string5                  // DW_AT_name
	.byte	5                               // DW_AT_encoding
	.byte	4                               // DW_AT_byte_size
	.byte	0                               // End Of Children Mark
.Ldebug_info_end0:
	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.asciz	"VOID source compiler"          // string offset=0 ; VOID source compiler
.Linfo_string1:
	.asciz	"guarded-range-i32.void"        // string offset=21 ; guarded-range-i32.void
.Linfo_string2:
	.asciz	"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures" // string offset=44 ; /home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures
.Linfo_string3:
	.asciz	"__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void" // string offset=127 ; __vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void
.Linfo_string4:
	.asciz	"entry"                         // string offset=199 ; entry
.Linfo_string5:
	.asciz	"i32"                           // string offset=205 ; i32
	.section	.debug_pubnames,"",@progbits
	.word	.LpubNames_end0-.LpubNames_start0 // Length of Public Names Info
.LpubNames_start0:
	.hword	2                               // DWARF Version
	.word	.Lcu_begin0                     // Offset of Compilation Unit Info
	.word	79                              // Compilation Unit Length
	.word	42                              // DIE offset
	.asciz	"entry"                         // External Name
	.word	0                               // End Mark
.LpubNames_end0:
	.section	.debug_pubtypes,"",@progbits
	.word	.LpubTypes_end0-.LpubTypes_start0 // Length of Public Types Info
.LpubTypes_start0:
	.hword	2                               // DWARF Version
	.word	.Lcu_begin0                     // Offset of Compilation Unit Info
	.word	79                              // Compilation Unit Length
	.word	71                              // DIE offset
	.asciz	"i32"                           // External Name
	.word	0                               // End Mark
.LpubTypes_end0:
	.section	".note.GNU-stack","",@progbits
	.section	.debug_line,"",@progbits
.Lline_table_start0:
