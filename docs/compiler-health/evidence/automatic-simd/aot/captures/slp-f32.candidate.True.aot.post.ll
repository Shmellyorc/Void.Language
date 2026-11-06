; ModuleID = '/slp-f32.void'
source_filename = "/slp-f32.void"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@__vd_types__void = local_unnamed_addr constant [0 x { ptr, ptr, ptr, i64, ptr, i64, ptr, i32 }] zeroinitializer
@__vd_type_count__void = local_unnamed_addr constant i64 0
@__vd_witnesses__void = local_unnamed_addr constant [0 x { ptr, ptr, ptr, ptr }] zeroinitializer
@__vd_witness_count__void = local_unnamed_addr constant i64 0
@0 = private unnamed_addr constant [46 x i8] c"VOID: expected one optional decimal i32 input\00", align 1
@stderr = external local_unnamed_addr global ptr
@1 = private unnamed_addr constant [43 x i8] c"\0ASource call trace limited to 32 callers.\0A\00", align 1
@2 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define range(i32 1, 3) i32 @__vd_2f736c702d6633322e766f69643a3a656e747279__void(i32 %0, ptr nofree readnone captures(none) %1) local_unnamed_addr #0 !dbg !3 {
block0:
  %.not39 = icmp eq i32 %0, 0, !dbg !7
  br i1 %.not39, label %block4, label %block2.preheader, !dbg !7

block2.preheader:                                 ; preds = %block0
  %.inv = icmp sgt i32 %0, -1
  %.0.be.v = select i1 %.inv, i32 1, i32 -1
  br label %block2, !dbg !7

block2:                                           ; preds = %block2.preheader, %block2
  %.044 = phi i32 [ %.0.be, %block2 ], [ 0, %block2.preheader ]
  %.03443 = phi float [ %3, %block2 ], [ 1.250000e-01, %block2.preheader ]
  %.03542 = phi float [ %5, %block2 ], [ 2.500000e-01, %block2.preheader ]
  %.03641 = phi float [ %9, %block2 ], [ 1.000000e+00, %block2.preheader ]
  %.03740 = phi float [ %7, %block2 ], [ 5.000000e-01, %block2.preheader ]
  %2 = fmul float %.03443, f0x3F800001, !dbg !8
  %3 = fadd float %2, 1.250000e-01, !dbg !9
  %4 = fmul float %.03542, f0x3F800001, !dbg !10
  %5 = fadd float %4, 2.500000e-01, !dbg !11
  %6 = fmul float %.03740, f0x3F800001, !dbg !12
  %7 = fadd float %6, 5.000000e-01, !dbg !13
  %8 = fmul float %.03641, f0x3F800001, !dbg !14
  %9 = fadd float %8, 1.000000e+00, !dbg !15
  %.0.be = add i32 %.044, %.0.be.v, !dbg !7
  %.not = icmp eq i32 %.0.be, %0, !dbg !7
  br i1 %.not, label %block4, label %block2, !dbg !7

block4:                                           ; preds = %block2, %block0
  %.037.lcssa = phi float [ 5.000000e-01, %block0 ], [ %7, %block2 ], !dbg !16
  %.036.lcssa = phi float [ 1.000000e+00, %block0 ], [ %9, %block2 ], !dbg !16
  %.035.lcssa = phi float [ 2.500000e-01, %block0 ], [ %5, %block2 ], !dbg !16
  %.034.lcssa = phi float [ 1.250000e-01, %block0 ], [ %3, %block2 ], !dbg !16
  %10 = fmul float %.034.lcssa, 2.000000e+00, !dbg !17
  %11 = fcmp une float %.035.lcssa, %10, !dbg !18
  br i1 %11, label %common.ret, label %block11, !dbg !18

common.ret:                                       ; preds = %block11, %block4
  %common.ret.op = phi i32 [ 2, %block4 ], [ %spec.select, %block11 ]
  ret i32 %common.ret.op, !dbg !16

block11:                                          ; preds = %block4
  %12 = fmul float %.034.lcssa, 4.000000e+00, !dbg !19
  %13 = fcmp une float %.037.lcssa, %12, !dbg !20
  %14 = fmul float %.034.lcssa, 8.000000e+00
  %15 = fcmp une float %.036.lcssa, %14
  %or.cond = select i1 %13, i1 true, i1 %15, !dbg !20
  %spec.select = select i1 %or.cond, i32 2, i32 1, !dbg !20
  br label %common.ret, !dbg !20
}

; Function Attrs: nofree nounwind
define range(i32 0, 3) i32 @main(i32 %0, ptr nofree readonly captures(none) %1) local_unnamed_addr #1 {
entry:
  %2 = icmp eq i32 %0, 1
  br i1 %2, label %run.thread, label %parse_input

run.thread:                                       ; preds = %entry
  %3 = alloca { ptr, i32, [32 x ptr] }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(272) %3, i8 0, i64 272, i1 false)
  br label %block4.i, !dbg !7

parse_input:                                      ; preds = %entry
  %4 = alloca ptr, align 8
  %5 = getelementptr i8, ptr %1, i64 8
  %6 = load ptr, ptr %5, align 8
  %7 = call i64 @strtoll(ptr %6, ptr nonnull %4, i32 10)
  %8 = load ptr, ptr %4, align 8
  %9 = icmp eq i32 %0, 2
  %10 = icmp ne ptr %8, %6
  %11 = and i1 %9, %10
  %12 = load i8, ptr %8, align 1
  %13 = icmp eq i8 %12, 0
  %14 = and i1 %11, %13
  %15 = add i64 %7, 2147483648
  %16 = icmp ult i64 %15, 4294967296
  %17 = and i1 %16, %14
  %input = trunc nsw i64 %7 to i32
  br i1 %17, label %run, label %invalid_input

run:                                              ; preds = %parse_input
  %18 = alloca { ptr, i32, [32 x ptr] }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(272) %18, i8 0, i64 272, i1 false)
  %.not39.i = icmp eq i64 %7, 0, !dbg !7
  br i1 %.not39.i, label %block4.i, label %block2.preheader.i, !dbg !7

block2.preheader.i:                               ; preds = %run
  %.inv.i = icmp sgt i64 %7, -1
  %.0.be.v.i = select i1 %.inv.i, i32 1, i32 -1
  br label %block2.i, !dbg !7

block2.i:                                         ; preds = %block2.i, %block2.preheader.i
  %.044.i = phi i32 [ %.0.be.i, %block2.i ], [ 0, %block2.preheader.i ]
  %.03443.i = phi float [ %20, %block2.i ], [ 1.250000e-01, %block2.preheader.i ]
  %.03542.i = phi float [ %22, %block2.i ], [ 2.500000e-01, %block2.preheader.i ]
  %.03641.i = phi float [ %26, %block2.i ], [ 1.000000e+00, %block2.preheader.i ]
  %.03740.i = phi float [ %24, %block2.i ], [ 5.000000e-01, %block2.preheader.i ]
  %19 = fmul float %.03443.i, f0x3F800001, !dbg !8
  %20 = fadd float %19, 1.250000e-01, !dbg !9
  %21 = fmul float %.03542.i, f0x3F800001, !dbg !10
  %22 = fadd float %21, 2.500000e-01, !dbg !11
  %23 = fmul float %.03740.i, f0x3F800001, !dbg !12
  %24 = fadd float %23, 5.000000e-01, !dbg !13
  %25 = fmul float %.03641.i, f0x3F800001, !dbg !14
  %26 = fadd float %25, 1.000000e+00, !dbg !15
  %.0.be.i = add i32 %.044.i, %.0.be.v.i, !dbg !7
  %.not.i = icmp eq i32 %.0.be.i, %input, !dbg !7
  br i1 %.not.i, label %block4.i, label %block2.i, !dbg !7

block4.i:                                         ; preds = %block2.i, %run.thread, %run
  %27 = phi ptr [ %18, %run ], [ %3, %run.thread ], [ %18, %block2.i ]
  %.037.lcssa.i = phi float [ 5.000000e-01, %run ], [ 5.000000e-01, %run.thread ], [ %24, %block2.i ], !dbg !16
  %.036.lcssa.i = phi float [ 1.000000e+00, %run ], [ 1.000000e+00, %run.thread ], [ %26, %block2.i ], !dbg !16
  %.035.lcssa.i = phi float [ 2.500000e-01, %run ], [ 2.500000e-01, %run.thread ], [ %22, %block2.i ], !dbg !16
  %.034.lcssa.i = phi float [ 1.250000e-01, %run ], [ 1.250000e-01, %run.thread ], [ %20, %block2.i ], !dbg !16
  %28 = fmul float %.034.lcssa.i, 2.000000e+00, !dbg !17
  %29 = fcmp une float %.035.lcssa.i, %28, !dbg !18
  br i1 %29, label %__vd_2f736c702d6633322e766f69643a3a656e747279__void.exit, label %block11.i, !dbg !18

block11.i:                                        ; preds = %block4.i
  %30 = fmul float %.034.lcssa.i, 4.000000e+00, !dbg !19
  %31 = fcmp une float %.037.lcssa.i, %30, !dbg !20
  %32 = fmul float %.034.lcssa.i, 8.000000e+00
  %33 = fcmp une float %.036.lcssa.i, %32
  %or.cond.i = select i1 %31, i1 true, i1 %33, !dbg !20
  %spec.select.i = select i1 %or.cond.i, i32 2, i32 1, !dbg !20
  br label %__vd_2f736c702d6633322e766f69643a3a656e747279__void.exit, !dbg !20

__vd_2f736c702d6633322e766f69643a3a656e747279__void.exit: ; preds = %block4.i, %block11.i
  %common.ret.op.i = phi i32 [ 2, %block4.i ], [ %spec.select.i, %block11.i ]
  %34 = load ptr, ptr %27, align 8
  %.not = icmp eq ptr %34, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %35 = tail call i32 @puts(ptr nonnull dereferenceable(1) @0)
  br label %common.ret

runtime_error:                                    ; preds = %__vd_2f736c702d6633322e766f69643a3a656e747279__void.exit
  %36 = load ptr, ptr @stderr, align 8
  %fputs = tail call i32 @fputs(ptr nonnull %34, ptr %36) #5
  %37 = getelementptr inbounds nuw i8, ptr %27, i64 8
  %38 = load i32, ptr %37, align 8
  %.not10 = icmp eq i32 %38, 0
  br i1 %.not10, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = tail call i32 @llvm.umin.i32(i32 %38, i32 32)
  %39 = getelementptr inbounds nuw i8, ptr %27, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %__vd_2f736c702d6633322e766f69643a3a656e747279__void.exit
  %40 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i32 %common.ret.op.i)
  br label %common.ret

print_caller:                                     ; preds = %print_caller.lr.ph, %print_caller
  %indvars.iv = phi i64 [ 0, %print_caller.lr.ph ], [ %indvars.iv.next, %print_caller ]
  %41 = getelementptr [8 x i8], ptr %39, i64 %indvars.iv
  %42 = load ptr, ptr %41, align 8
  %fputs5 = tail call i32 @fputs(ptr %42, ptr %36) #5
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %failure_exit, label %print_caller

failure_exit:                                     ; preds = %print_caller
  %43 = icmp ugt i32 %38, 32
  br i1 %43, label %trace_limit, label %exit

trace_limit:                                      ; preds = %failure_exit
  %44 = tail call i64 @fwrite(ptr nonnull @1, i64 42, i64 1, ptr %36) #5
  br label %exit

exit:                                             ; preds = %runtime_error, %trace_limit, %failure_exit
  %fputc = tail call i32 @fputc(i32 10, ptr %36)
  br label %common.ret
}

; Function Attrs: mustprogress nocallback nofree nounwind willreturn
declare i64 @strtoll(ptr readonly, ptr captures(none), i32) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare noundef i32 @puts(ptr noundef readonly captures(none)) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr noundef readonly captures(none), ...) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @fputs(ptr noundef readonly captures(none), ptr noundef captures(none)) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i64 @fwrite(ptr noundef readonly captures(none), i64 noundef, i64 noundef, ptr noundef captures(none)) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @fputc(i32 noundef, ptr noundef captures(none)) local_unnamed_addr #1

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.umin.i32(i32, i32) #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #4

attributes #0 = { nofree norecurse nosync nounwind memory(none) }
attributes #1 = { nofree nounwind }
attributes #2 = { mustprogress nocallback nofree nounwind willreturn }
attributes #3 = { nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }
attributes #4 = { nocallback nofree nosync nounwind willreturn memory(argmem: write) }
attributes #5 = { cold }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "VOID source compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "slp-f32.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f736c702d6633322e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 6, column: 5, scope: !3)
!8 = !DILocation(line: 7, column: 15, scope: !3)
!9 = !DILocation(line: 7, column: 27, scope: !3)
!10 = !DILocation(line: 8, column: 15, scope: !3)
!11 = !DILocation(line: 8, column: 27, scope: !3)
!12 = !DILocation(line: 9, column: 15, scope: !3)
!13 = !DILocation(line: 9, column: 27, scope: !3)
!14 = !DILocation(line: 10, column: 15, scope: !3)
!15 = !DILocation(line: 10, column: 27, scope: !3)
!16 = !DILocation(line: 0, scope: !3)
!17 = !DILocation(line: 11, column: 15, scope: !3)
!18 = !DILocation(line: 11, column: 10, scope: !3)
!19 = !DILocation(line: 11, column: 29, scope: !3)
!20 = !DILocation(line: 11, column: 24, scope: !3)
