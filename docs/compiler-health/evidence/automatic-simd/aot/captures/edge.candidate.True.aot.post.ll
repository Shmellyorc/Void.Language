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

block5:                                           ; preds = %block5.5, %block0
  %.05263 = phi i32 [ 0, %block0 ], [ %10, %block5.5 ]
  %.05462 = phi float [ 1.000000e+00, %block0 ], [ %9, %block5.5 ]
  %2 = fmul float %.05462, 5.000000e-01, !dbg !9
  %3 = fmul float %2, 5.000000e-01, !dbg !9
  %4 = fmul float %3, 5.000000e-01, !dbg !9
  %5 = fmul float %4, 5.000000e-01, !dbg !9
  %6 = fmul float %5, 5.000000e-01, !dbg !9
  %.not.4 = icmp eq i32 %.05263, 144, !dbg !7
  br i1 %.not.4, label %block4, label %block5.5, !dbg !7

block5.5:                                         ; preds = %block5
  %7 = fmul float %6, 5.000000e-01, !dbg !9
  %8 = fmul float %7, 5.000000e-01, !dbg !9
  %9 = fmul float %8, 5.000000e-01, !dbg !9
  %10 = add nuw nsw i32 %.05263, 8, !dbg !7
  br label %block5, !dbg !7

block8:                                           ; preds = %block8.preheader, %block8
  %.068 = phi i32 [ %.0.be, %block8 ], [ 0, %block8.preheader ]
  %.05566 = phi i64 [ %12, %block8 ], [ 9223372036854775807, %block8.preheader ]
  %.05665 = phi i32 [ %11, %block8 ], [ 2147483647, %block8.preheader ]
  %11 = add i32 %.05665, 1, !dbg !10
  %12 = add i64 %.05566, 1, !dbg !11
  %.0.be = add i32 %.068, %.0.be.v, !dbg !8
  %.not58 = icmp eq i32 %.0.be, %0, !dbg !8
  br i1 %.not58, label %block10, label %block8, !dbg !8

block10:                                          ; preds = %block8
  %13 = icmp eq i32 %0, 1, !dbg !12
  br i1 %13, label %block13, label %block24, !dbg !12

block13:                                          ; preds = %block10
  %14 = icmp ne i32 %11, -2147483648, !dbg !13
  %15 = icmp ne i64 %12, -9223372036854775808
  %brmerge = select i1 %14, i1 true, i1 %15, !dbg !14
  br i1 %brmerge, label %common.ret, label %block24, !dbg !14

common.ret:                                       ; preds = %block13, %block24, %block27
  %common.ret.op = phi i32 [ 1, %block27 ], [ 2, %block13 ], [ 3, %block24 ]
  ret i32 %common.ret.op, !dbg !15

block24:                                          ; preds = %block13, %block4, %block10
  %16 = fmul float %6, 2.000000e+00, !dbg !16
  %17 = fcmp ugt float %16, 0.000000e+00, !dbg !17
  br i1 %17, label %block27, label %common.ret, !dbg !17

block27:                                          ; preds = %block24
  br label %common.ret, !dbg !18
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

block5.i:                                         ; preds = %block5.i.5, %run
  %.05263.i = phi i32 [ 0, %run ], [ %27, %block5.i.5 ]
  %.05462.i = phi float [ 1.000000e+00, %run ], [ %26, %block5.i.5 ]
  %19 = fmul float %.05462.i, 5.000000e-01, !dbg !9
  %20 = fmul float %19, 5.000000e-01, !dbg !9
  %21 = fmul float %20, 5.000000e-01, !dbg !9
  %22 = fmul float %21, 5.000000e-01, !dbg !9
  %23 = fmul float %22, 5.000000e-01, !dbg !9
  %.not.i.4 = icmp eq i32 %.05263.i, 144, !dbg !7
  br i1 %.not.i.4, label %block4.i, label %block5.i.5, !dbg !7

block5.i.5:                                       ; preds = %block5.i
  %24 = fmul float %23, 5.000000e-01, !dbg !9
  %25 = fmul float %24, 5.000000e-01, !dbg !9
  %26 = fmul float %25, 5.000000e-01, !dbg !9
  %27 = add nuw nsw i32 %.05263.i, 8, !dbg !7
  br label %block5.i, !dbg !7

block8.i:                                         ; preds = %block8.i, %block8.preheader.i
  %.068.i = phi i32 [ %.0.be.i, %block8.i ], [ 0, %block8.preheader.i ]
  %.05566.i = phi i64 [ %29, %block8.i ], [ 9223372036854775807, %block8.preheader.i ]
  %.05665.i = phi i32 [ %28, %block8.i ], [ 2147483647, %block8.preheader.i ]
  %28 = add i32 %.05665.i, 1, !dbg !10
  %29 = add i64 %.05566.i, 1, !dbg !11
  %.0.be.i = add i32 %.068.i, %.0.be.v.i, !dbg !8
  %.not58.i = icmp eq i32 %.0.be.i, %17, !dbg !8
  br i1 %.not58.i, label %block10.i, label %block8.i, !dbg !8

block10.i:                                        ; preds = %block8.i
  %30 = icmp eq i32 %17, 1, !dbg !12
  br i1 %30, label %block13.i, label %block24.i, !dbg !12

block13.i:                                        ; preds = %block10.i
  %31 = icmp ne i32 %28, -2147483648, !dbg !13
  %32 = icmp ne i64 %29, -9223372036854775808
  %brmerge.i = select i1 %31, i1 true, i1 %32, !dbg !14
  br i1 %brmerge.i, label %__vd_2f656467652e766f69643a3a656e747279__void.exit, label %block24.i, !dbg !14

block24.i:                                        ; preds = %block13.i, %block10.i, %block4.i
  %33 = fmul float %23, 2.000000e+00, !dbg !16
  %34 = fcmp ugt float %33, 0.000000e+00, !dbg !17
  br i1 %34, label %block27.i, label %__vd_2f656467652e766f69643a3a656e747279__void.exit, !dbg !17

block27.i:                                        ; preds = %block24.i
  br label %__vd_2f656467652e766f69643a3a656e747279__void.exit, !dbg !18

__vd_2f656467652e766f69643a3a656e747279__void.exit: ; preds = %block13.i, %block24.i, %block27.i
  %common.ret.op.i = phi i32 [ 1, %block27.i ], [ 2, %block13.i ], [ 3, %block24.i ]
  %35 = load ptr, ptr %18, align 8
  %.not = icmp eq ptr %35, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %36 = tail call i32 @puts(ptr nonnull dereferenceable(1) @0)
  br label %common.ret

runtime_error:                                    ; preds = %__vd_2f656467652e766f69643a3a656e747279__void.exit
  %37 = load ptr, ptr @stderr, align 8
  %fputs = tail call i32 @fputs(ptr nonnull %35, ptr %37) #5
  %38 = getelementptr inbounds nuw i8, ptr %18, i64 8
  %39 = load i32, ptr %38, align 8
  %.not8 = icmp eq i32 %39, 0
  br i1 %.not8, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = tail call i32 @llvm.umin.i32(i32 %39, i32 32)
  %40 = getelementptr inbounds nuw i8, ptr %18, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %__vd_2f656467652e766f69643a3a656e747279__void.exit
  %41 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i32 %common.ret.op.i)
  br label %common.ret

print_caller:                                     ; preds = %print_caller.lr.ph, %print_caller
  %indvars.iv = phi i64 [ 0, %print_caller.lr.ph ], [ %indvars.iv.next, %print_caller ]
  %42 = getelementptr [8 x i8], ptr %40, i64 %indvars.iv
  %43 = load ptr, ptr %42, align 8
  %fputs5 = tail call i32 @fputs(ptr %43, ptr %37) #5
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %failure_exit, label %print_caller

failure_exit:                                     ; preds = %print_caller
  %44 = icmp ugt i32 %39, 32
  br i1 %44, label %trace_limit, label %exit

trace_limit:                                      ; preds = %failure_exit
  %45 = tail call i64 @fwrite(ptr nonnull @1, i64 42, i64 1, ptr %37) #5
  br label %exit

exit:                                             ; preds = %runtime_error, %trace_limit, %failure_exit
  %fputc = tail call i32 @fputc(i32 10, ptr %37)
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
!10 = !DILocation(line: 11, column: 9, scope: !3)
!11 = !DILocation(line: 12, column: 9, scope: !3)
!12 = !DILocation(line: 13, column: 10, scope: !3)
!13 = !DILocation(line: 14, column: 24, scope: !3)
!14 = !DILocation(line: 14, column: 14, scope: !3)
!15 = !DILocation(line: 0, scope: !3)
!16 = !DILocation(line: 16, column: 31, scope: !3)
!17 = !DILocation(line: 16, column: 37, scope: !3)
!18 = !DILocation(line: 21, column: 12, scope: !3)
