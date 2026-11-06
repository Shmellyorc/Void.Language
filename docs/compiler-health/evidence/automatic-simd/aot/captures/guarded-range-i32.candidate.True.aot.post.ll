; ModuleID = '/guarded-range-i32.void'
source_filename = "/guarded-range-i32.void"
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
define i32 @__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void(i32 %0, ptr nofree readnone captures(none) %1) local_unnamed_addr #0 !dbg !3 {
block0:
  %2 = icmp slt i32 %0, 1, !dbg !7
  br i1 %2, label %common.ret, label %block8.preheader, !dbg !7

block8.preheader:                                 ; preds = %block0
  %min.iters.check = icmp ult i32 %0, 8, !dbg !8
  br i1 %min.iters.check, label %block8.preheader23, label %vector.ph, !dbg !8

vector.ph:                                        ; preds = %block8.preheader
  %n.vec = and i32 %0, 2147483640
  br label %vector.body, !dbg !8

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i32 [ 0, %vector.ph ], [ %index.next, %vector.body ], !dbg !8
  %vec.ind = phi <4 x i32> [ <i32 0, i32 1, i32 2, i32 3>, %vector.ph ], [ %vec.ind.next, %vector.body ]
  %vec.phi = phi <4 x i32> [ zeroinitializer, %vector.ph ], [ %5, %vector.body ]
  %vec.phi22 = phi <4 x i32> [ zeroinitializer, %vector.ph ], [ %6, %vector.body ]
  %step.add = add nuw <4 x i32> %vec.ind, splat (i32 4)
  %3 = urem <4 x i32> %vec.ind, splat (i32 127), !dbg !9
  %4 = urem <4 x i32> %step.add, splat (i32 127), !dbg !9
  %5 = add <4 x i32> %3, %vec.phi, !dbg !10
  %6 = add <4 x i32> %4, %vec.phi22, !dbg !10
  %index.next = add nuw i32 %index, 8, !dbg !8
  %vec.ind.next = add nuw <4 x i32> %vec.ind, splat (i32 8)
  %7 = icmp eq i32 %index.next, %n.vec, !dbg !8
  br i1 %7, label %middle.block, label %vector.body, !dbg !8, !llvm.loop !11

middle.block:                                     ; preds = %vector.body
  %bin.rdx = add <4 x i32> %6, %5, !dbg !8
  %8 = tail call i32 @llvm.vector.reduce.add.v4i32(<4 x i32> %bin.rdx), !dbg !8
  %cmp.n = icmp eq i32 %0, %n.vec, !dbg !8
  br i1 %cmp.n, label %common.ret, label %block8.preheader23, !dbg !8

block8.preheader23:                               ; preds = %block8.preheader, %middle.block
  %.021.ph = phi i32 [ 0, %block8.preheader ], [ %n.vec, %middle.block ]
  %.01820.ph = phi i32 [ 0, %block8.preheader ], [ %8, %middle.block ]
  br label %block8, !dbg !8

common.ret:                                       ; preds = %block8, %middle.block, %block0
  %common.ret.op = phi i32 [ 0, %block0 ], [ %8, %middle.block ], [ %9, %block8 ]
  ret i32 %common.ret.op, !dbg !14

block8:                                           ; preds = %block8.preheader23, %block8
  %.021 = phi i32 [ %10, %block8 ], [ %.021.ph, %block8.preheader23 ]
  %.01820 = phi i32 [ %9, %block8 ], [ %.01820.ph, %block8.preheader23 ]
  %.urem = urem i32 %.021, 127, !dbg !9
  %9 = add i32 %.urem, %.01820, !dbg !10
  %10 = add nuw nsw i32 %.021, 1, !dbg !8
  %.not = icmp eq i32 %10, %0, !dbg !8
  br i1 %.not, label %common.ret, label %block8, !dbg !8, !llvm.loop !15
}

; Function Attrs: nofree nounwind
define range(i32 0, 3) i32 @main(i32 %0, ptr nofree readonly captures(none) %1) local_unnamed_addr #1 {
entry:
  %2 = icmp eq i32 %0, 1
  br i1 %2, label %success, label %parse_input

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
  %input = trunc i64 %6 to i32
  br i1 %16, label %run, label %invalid_input

run:                                              ; preds = %parse_input
  %17 = alloca { ptr, i32, [32 x ptr] }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(272) %17, i8 0, i64 272, i1 false)
  %18 = icmp slt i64 %6, 1, !dbg !7
  br i1 %18, label %success, label %block8.i.preheader, !dbg !7

block8.i.preheader:                               ; preds = %run
  %min.iters.check = icmp ult i32 %input, 8, !dbg !8
  br i1 %min.iters.check, label %block8.i.preheader12, label %vector.ph, !dbg !8

vector.ph:                                        ; preds = %block8.i.preheader
  %n.vec = and i32 %input, -8
  br label %vector.body, !dbg !8

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i32 [ 0, %vector.ph ], [ %index.next, %vector.body ], !dbg !8
  %vec.ind = phi <4 x i32> [ <i32 0, i32 1, i32 2, i32 3>, %vector.ph ], [ %vec.ind.next, %vector.body ]
  %vec.phi = phi <4 x i32> [ zeroinitializer, %vector.ph ], [ %21, %vector.body ]
  %vec.phi11 = phi <4 x i32> [ zeroinitializer, %vector.ph ], [ %22, %vector.body ]
  %step.add = add nuw <4 x i32> %vec.ind, splat (i32 4)
  %19 = urem <4 x i32> %vec.ind, splat (i32 127), !dbg !9
  %20 = urem <4 x i32> %step.add, splat (i32 127), !dbg !9
  %21 = add <4 x i32> %19, %vec.phi, !dbg !10
  %22 = add <4 x i32> %20, %vec.phi11, !dbg !10
  %index.next = add nuw i32 %index, 8, !dbg !8
  %vec.ind.next = add nuw <4 x i32> %vec.ind, splat (i32 8)
  %23 = icmp eq i32 %index.next, %n.vec, !dbg !8
  br i1 %23, label %middle.block, label %vector.body, !dbg !8, !llvm.loop !16

middle.block:                                     ; preds = %vector.body
  %bin.rdx = add <4 x i32> %22, %21, !dbg !8
  %24 = tail call i32 @llvm.vector.reduce.add.v4i32(<4 x i32> %bin.rdx), !dbg !8
  %cmp.n = icmp eq i32 %n.vec, %input, !dbg !8
  br i1 %cmp.n, label %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit, label %block8.i.preheader12, !dbg !8

block8.i.preheader12:                             ; preds = %block8.i.preheader, %middle.block
  %.021.i.ph = phi i32 [ 0, %block8.i.preheader ], [ %n.vec, %middle.block ]
  %.01820.i.ph = phi i32 [ 0, %block8.i.preheader ], [ %24, %middle.block ]
  br label %block8.i, !dbg !8

block8.i:                                         ; preds = %block8.i.preheader12, %block8.i
  %.021.i = phi i32 [ %26, %block8.i ], [ %.021.i.ph, %block8.i.preheader12 ]
  %.01820.i = phi i32 [ %25, %block8.i ], [ %.01820.i.ph, %block8.i.preheader12 ]
  %.urem.i = urem i32 %.021.i, 127, !dbg !9
  %25 = add i32 %.urem.i, %.01820.i, !dbg !10
  %26 = add nuw nsw i32 %.021.i, 1, !dbg !8
  %.not.i = icmp eq i32 %26, %input, !dbg !8
  br i1 %.not.i, label %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit, label %block8.i, !dbg !8, !llvm.loop !17

__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit: ; preds = %block8.i, %middle.block
  %.lcssa = phi i32 [ %24, %middle.block ], [ %25, %block8.i ], !dbg !10
  %.pr = load ptr, ptr %17, align 8
  %.not = icmp eq ptr %.pr, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %27 = tail call i32 @puts(ptr nonnull dereferenceable(1) @0)
  br label %common.ret

runtime_error:                                    ; preds = %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit
  %28 = load ptr, ptr @stderr, align 8
  %fputs = tail call i32 @fputs(ptr nonnull %.pr, ptr %28) #5
  %29 = getelementptr inbounds nuw i8, ptr %17, i64 8
  %30 = load i32, ptr %29, align 8
  %.not9 = icmp eq i32 %30, 0
  br i1 %.not9, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = tail call i32 @llvm.umin.i32(i32 %30, i32 32)
  %31 = getelementptr inbounds nuw i8, ptr %17, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %entry, %run, %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit
  %common.ret.op.i8 = phi i32 [ %.lcssa, %__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void.exit ], [ 0, %run ], [ 0, %entry ]
  %32 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i32 %common.ret.op.i8)
  br label %common.ret

print_caller:                                     ; preds = %print_caller.lr.ph, %print_caller
  %indvars.iv = phi i64 [ 0, %print_caller.lr.ph ], [ %indvars.iv.next, %print_caller ]
  %33 = getelementptr [8 x i8], ptr %31, i64 %indvars.iv
  %34 = load ptr, ptr %33, align 8
  %fputs5 = tail call i32 @fputs(ptr %34, ptr %28) #5
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %failure_exit, label %print_caller

failure_exit:                                     ; preds = %print_caller
  %35 = icmp ugt i32 %30, 32
  br i1 %35, label %trace_limit, label %exit

trace_limit:                                      ; preds = %failure_exit
  %36 = tail call i64 @fwrite(ptr nonnull @1, i64 42, i64 1, ptr %28) #5
  br label %exit

exit:                                             ; preds = %runtime_error, %trace_limit, %failure_exit
  %fputc = tail call i32 @fputc(i32 10, ptr %28)
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

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.vector.reduce.add.v4i32(<4 x i32>) #3

attributes #0 = { nofree norecurse nosync nounwind memory(none) }
attributes #1 = { nofree nounwind }
attributes #2 = { mustprogress nocallback nofree nounwind willreturn }
attributes #3 = { nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }
attributes #4 = { nocallback nofree nosync nounwind willreturn memory(argmem: write) }
attributes #5 = { cold }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "VOID source compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "guarded-range-i32.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 2, column: 10, scope: !3)
!8 = !DILocation(line: 5, column: 5, scope: !3)
!9 = !DILocation(line: 6, column: 20, scope: !3)
!10 = !DILocation(line: 6, column: 9, scope: !3)
!11 = distinct !{!11, !12, !13}
!12 = !{!"llvm.loop.isvectorized", i32 1}
!13 = !{!"llvm.loop.unroll.runtime.disable"}
!14 = !DILocation(line: 0, scope: !3)
!15 = distinct !{!15, !13, !12}
!16 = distinct !{!16, !12, !13}
!17 = distinct !{!17, !13, !12}
