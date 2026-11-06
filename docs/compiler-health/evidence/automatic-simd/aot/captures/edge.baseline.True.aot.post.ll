; ModuleID = '/edge.void'
source_filename = "/edge.void"
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
define range(i32 1, 4) i32 @__vd_2f656467652e766f69643a3a656e747279__void(i32 %0, ptr nofree readnone captures(none) %1) local_unnamed_addr #0 !dbg !3 {
block0:
  br label %block5, !dbg !7

block4:                                           ; preds = %block5
  %.not5864 = icmp eq i32 %0, 0, !dbg !8
  br i1 %.not5864, label %block24, label %block8.preheader, !dbg !8

block8.preheader:                                 ; preds = %block4
  %.inv = icmp sgt i32 %0, -1
  %.0.be.v = select i1 %.inv, i32 1, i32 -1
  br label %block8, !dbg !8

block5:                                           ; preds = %block0, %block5
  %.05263 = phi i32 [ 0, %block0 ], [ %3, %block5 ]
  %.05462 = phi float [ 1.000000e+00, %block0 ], [ %2, %block5 ]
  %2 = fmul float %.05462, 5.000000e-01, !dbg !9
  %3 = add nuw nsw i32 %.05263, 1, !dbg !7
  %.not = icmp eq i32 %3, 149, !dbg !7
  br i1 %.not, label %block4, label %block5, !dbg !7

block8:                                           ; preds = %block8.preheader, %block8
  %.068 = phi i32 [ %.0.be, %block8 ], [ 0, %block8.preheader ]
  %.05367 = phi i8 [ %4, %block8 ], [ -1, %block8.preheader ]
  %.05566 = phi i64 [ %6, %block8 ], [ 9223372036854775807, %block8.preheader ]
  %.05665 = phi i32 [ %5, %block8 ], [ 2147483647, %block8.preheader ]
  %4 = add i8 %.05367, 1, !dbg !10
  %5 = add i32 %.05665, 1, !dbg !11
  %6 = add i64 %.05566, 1, !dbg !12
  %.0.be = add i32 %.068, %.0.be.v, !dbg !8
  %.not58 = icmp eq i32 %.0.be, %0, !dbg !8
  br i1 %.not58, label %block10, label %block8, !dbg !8

block10:                                          ; preds = %block8
  %7 = icmp eq i32 %0, 1, !dbg !13
  br i1 %7, label %block13, label %block24, !dbg !13

block13:                                          ; preds = %block10
  %8 = icmp ne i32 %5, -2147483648, !dbg !14
  %9 = icmp ne i64 %6, -9223372036854775808
  %10 = icmp ne i8 %4, 0, !dbg !15
  %11 = select i1 %10, i1 true, i1 %8, !dbg !15
  %brmerge = select i1 %11, i1 true, i1 %9, !dbg !15
  br i1 %brmerge, label %common.ret, label %block24, !dbg !15

common.ret:                                       ; preds = %block13, %block24, %block27
  %common.ret.op = phi i32 [ 1, %block27 ], [ 2, %block13 ], [ 3, %block24 ]
  ret i32 %common.ret.op, !dbg !16

block24:                                          ; preds = %block13, %block4, %block10
  %12 = fmul float %2, 2.000000e+00, !dbg !17
  %13 = fcmp ugt float %12, 0.000000e+00, !dbg !18
  br i1 %13, label %block27, label %common.ret, !dbg !18

block27:                                          ; preds = %block24
  br label %common.ret, !dbg !19
}

; Function Attrs: nofree nounwind
define range(i32 0, 3) i32 @main(i32 %0, ptr nofree readonly captures(none) %1) local_unnamed_addr #1 {
entry:
  %2 = icmp eq i32 %0, 1
  br i1 %2, label %run, label %parse_input

parse_input:                                      ; preds = %entry
  %3 = alloca ptr, align 8
  %4 = getelementptr i8, ptr %1, i64 8
  %5 = load ptr, ptr %4, align 8
  %6 = call i64 @strtoll(ptr %5, ptr nonnull %3, i32 10)
  %7 = load ptr, ptr %3, align 8
  %8 = icmp eq i32 %0, 2
  %9 = icmp ne ptr %7, %5
  %10 = and i1 %8, %9
  %11 = load i8, ptr %7, align 1
  %12 = icmp eq i8 %11, 0
  %13 = and i1 %10, %12
  %14 = add i64 %6, 2147483648
  %15 = icmp ult i64 %14, 4294967296
  %16 = and i1 %15, %13
  %input = trunc nsw i64 %6 to i32
  br i1 %16, label %run, label %invalid_input

run:                                              ; preds = %parse_input, %entry
  %17 = phi i32 [ 0, %entry ], [ %input, %parse_input ]
  %18 = alloca { ptr, i32, [32 x ptr] }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(272) %18, i8 0, i64 272, i1 false)
  br label %block5.i, !dbg !7

block4.i:                                         ; preds = %block5.i
  %.not5864.i = icmp eq i32 %17, 0, !dbg !8
  br i1 %.not5864.i, label %block24.i, label %block8.preheader.i, !dbg !8

block8.preheader.i:                               ; preds = %block4.i
  %.inv.i = icmp sgt i32 %17, -1
  %.0.be.v.i = select i1 %.inv.i, i32 1, i32 -1
  br label %block8.i, !dbg !8

block5.i:                                         ; preds = %block5.i, %run
  %.05263.i = phi i32 [ 0, %run ], [ %20, %block5.i ]
  %.05462.i = phi float [ 1.000000e+00, %run ], [ %19, %block5.i ]
  %19 = fmul float %.05462.i, 5.000000e-01, !dbg !9
  %20 = add nuw nsw i32 %.05263.i, 1, !dbg !7
  %.not.i = icmp eq i32 %20, 149, !dbg !7
  br i1 %.not.i, label %block4.i, label %block5.i, !dbg !7

block8.i:                                         ; preds = %block8.i, %block8.preheader.i
  %.068.i = phi i32 [ %.0.be.i, %block8.i ], [ 0, %block8.preheader.i ]
  %.05367.i = phi i8 [ %21, %block8.i ], [ -1, %block8.preheader.i ]
  %.05566.i = phi i64 [ %23, %block8.i ], [ 9223372036854775807, %block8.preheader.i ]
  %.05665.i = phi i32 [ %22, %block8.i ], [ 2147483647, %block8.preheader.i ]
  %21 = add i8 %.05367.i, 1, !dbg !10
  %22 = add i32 %.05665.i, 1, !dbg !11
  %23 = add i64 %.05566.i, 1, !dbg !12
  %.0.be.i = add i32 %.068.i, %.0.be.v.i, !dbg !8
  %.not58.i = icmp eq i32 %.0.be.i, %17, !dbg !8
  br i1 %.not58.i, label %block10.i, label %block8.i, !dbg !8

block10.i:                                        ; preds = %block8.i
  %24 = icmp eq i32 %17, 1, !dbg !13
  br i1 %24, label %block13.i, label %block24.i, !dbg !13

block13.i:                                        ; preds = %block10.i
  %25 = icmp ne i32 %22, -2147483648, !dbg !14
  %26 = icmp ne i64 %23, -9223372036854775808
  %27 = icmp ne i8 %21, 0, !dbg !15
  %28 = select i1 %27, i1 true, i1 %25, !dbg !15
  %brmerge.i = select i1 %28, i1 true, i1 %26, !dbg !15
  br i1 %brmerge.i, label %__vd_2f656467652e766f69643a3a656e747279__void.exit, label %block24.i, !dbg !15

block24.i:                                        ; preds = %block13.i, %block10.i, %block4.i
  %29 = fmul float %19, 2.000000e+00, !dbg !17
  %30 = fcmp ugt float %29, 0.000000e+00, !dbg !18
  br i1 %30, label %block27.i, label %__vd_2f656467652e766f69643a3a656e747279__void.exit, !dbg !18

block27.i:                                        ; preds = %block24.i
  br label %__vd_2f656467652e766f69643a3a656e747279__void.exit, !dbg !19

__vd_2f656467652e766f69643a3a656e747279__void.exit: ; preds = %block13.i, %block24.i, %block27.i
  %common.ret.op.i = phi i32 [ 1, %block27.i ], [ 2, %block13.i ], [ 3, %block24.i ]
  %31 = load ptr, ptr %18, align 8
  %.not = icmp eq ptr %31, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %32 = tail call i32 @puts(ptr nonnull dereferenceable(1) @0)
  br label %common.ret

runtime_error:                                    ; preds = %__vd_2f656467652e766f69643a3a656e747279__void.exit
  %33 = load ptr, ptr @stderr, align 8
  %fputs = tail call i32 @fputs(ptr nonnull %31, ptr %33) #5
  %34 = getelementptr inbounds nuw i8, ptr %18, i64 8
  %35 = load i32, ptr %34, align 8
  %.not9 = icmp eq i32 %35, 0
  br i1 %.not9, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = tail call i32 @llvm.umin.i32(i32 %35, i32 32)
  %36 = getelementptr inbounds nuw i8, ptr %18, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %__vd_2f656467652e766f69643a3a656e747279__void.exit
  %37 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i32 %common.ret.op.i)
  br label %common.ret

print_caller:                                     ; preds = %print_caller.lr.ph, %print_caller
  %indvars.iv = phi i64 [ 0, %print_caller.lr.ph ], [ %indvars.iv.next, %print_caller ]
  %38 = getelementptr [8 x i8], ptr %36, i64 %indvars.iv
  %39 = load ptr, ptr %38, align 8
  %fputs5 = tail call i32 @fputs(ptr %39, ptr %33) #5
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %failure_exit, label %print_caller

failure_exit:                                     ; preds = %print_caller
  %40 = icmp ugt i32 %35, 32
  br i1 %40, label %trace_limit, label %exit

trace_limit:                                      ; preds = %failure_exit
  %41 = tail call i64 @fwrite(ptr nonnull @1, i64 42, i64 1, ptr %33) #5
  br label %exit

exit:                                             ; preds = %runtime_error, %trace_limit, %failure_exit
  %fputc = tail call i32 @fputc(i32 10, ptr %33)
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
!1 = !DIFile(filename: "edge.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f656467652e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 7, column: 5, scope: !3)
!8 = !DILocation(line: 9, column: 5, scope: !3)
!9 = !DILocation(line: 8, column: 9, scope: !3)
!10 = !DILocation(line: 10, column: 9, scope: !3)
!11 = !DILocation(line: 11, column: 9, scope: !3)
!12 = !DILocation(line: 12, column: 9, scope: !3)
!13 = !DILocation(line: 13, column: 10, scope: !3)
!14 = !DILocation(line: 14, column: 24, scope: !3)
!15 = !DILocation(line: 14, column: 14, scope: !3)
!16 = !DILocation(line: 0, scope: !3)
!17 = !DILocation(line: 16, column: 31, scope: !3)
!18 = !DILocation(line: 16, column: 37, scope: !3)
!19 = !DILocation(line: 21, column: 12, scope: !3)
