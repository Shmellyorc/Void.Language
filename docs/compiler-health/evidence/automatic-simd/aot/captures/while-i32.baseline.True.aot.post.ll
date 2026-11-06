; ModuleID = '/while-i32.void'
source_filename = "/while-i32.void"
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
define i32 @__vd_2f7768696c652d6933322e766f69643a3a656e747279__void(i32 %0, ptr nofree readnone captures(none) %1) local_unnamed_addr #0 !dbg !3 {
block0:
  %2 = icmp sgt i32 %0, 0, !dbg !7
  br i1 %2, label %block2, label %block3, !dbg !7

block2:                                           ; preds = %block0, %block2
  %.014 = phi i32 [ %5, %block2 ], [ 0, %block0 ]
  %.01213 = phi i32 [ %4, %block2 ], [ 0, %block0 ]
  %3 = urem i32 %.014, 127, !dbg !8
  %4 = add i32 %3, %.01213, !dbg !9
  %5 = add nuw nsw i32 %.014, 1, !dbg !10
  %exitcond.not = icmp eq i32 %5, %0, !dbg !7
  br i1 %exitcond.not, label %block3, label %block2, !dbg !7

block3:                                           ; preds = %block2, %block0
  %.012.lcssa = phi i32 [ 0, %block0 ], [ %4, %block2 ], !dbg !11
  ret i32 %.012.lcssa, !dbg !12
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
  %input = trunc nsw i64 %6 to i32
  br i1 %16, label %run, label %invalid_input

run:                                              ; preds = %parse_input
  %17 = alloca { ptr, i32, [32 x ptr] }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(272) %17, i8 0, i64 272, i1 false)
  %18 = icmp sgt i64 %6, 0, !dbg !7
  br i1 %18, label %block2.i, label %success, !dbg !7

block2.i:                                         ; preds = %run, %block2.i
  %.014.i = phi i32 [ %21, %block2.i ], [ 0, %run ]
  %.01213.i = phi i32 [ %20, %block2.i ], [ 0, %run ]
  %19 = urem i32 %.014.i, 127, !dbg !8
  %20 = add i32 %19, %.01213.i, !dbg !9
  %21 = add nuw nsw i32 %.014.i, 1, !dbg !10
  %exitcond.not.i = icmp eq i32 %21, %input, !dbg !7
  br i1 %exitcond.not.i, label %__vd_2f7768696c652d6933322e766f69643a3a656e747279__void.exit, label %block2.i, !dbg !7

__vd_2f7768696c652d6933322e766f69643a3a656e747279__void.exit: ; preds = %block2.i
  %.pr = load ptr, ptr %17, align 8
  %.not = icmp eq ptr %.pr, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %22 = tail call i32 @puts(ptr nonnull dereferenceable(1) @0)
  br label %common.ret

runtime_error:                                    ; preds = %__vd_2f7768696c652d6933322e766f69643a3a656e747279__void.exit
  %23 = load ptr, ptr @stderr, align 8
  %fputs = tail call i32 @fputs(ptr nonnull %.pr, ptr %23) #5
  %24 = getelementptr inbounds nuw i8, ptr %17, i64 8
  %25 = load i32, ptr %24, align 8
  %.not9 = icmp eq i32 %25, 0
  br i1 %.not9, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = tail call i32 @llvm.umin.i32(i32 %25, i32 32)
  %26 = getelementptr inbounds nuw i8, ptr %17, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %entry, %run, %__vd_2f7768696c652d6933322e766f69643a3a656e747279__void.exit
  %.012.lcssa.i8 = phi i32 [ %20, %__vd_2f7768696c652d6933322e766f69643a3a656e747279__void.exit ], [ 0, %run ], [ 0, %entry ]
  %27 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i32 %.012.lcssa.i8)
  br label %common.ret

print_caller:                                     ; preds = %print_caller.lr.ph, %print_caller
  %indvars.iv = phi i64 [ 0, %print_caller.lr.ph ], [ %indvars.iv.next, %print_caller ]
  %28 = getelementptr [8 x i8], ptr %26, i64 %indvars.iv
  %29 = load ptr, ptr %28, align 8
  %fputs5 = tail call i32 @fputs(ptr %29, ptr %23) #5
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %failure_exit, label %print_caller

failure_exit:                                     ; preds = %print_caller
  %30 = icmp ugt i32 %25, 32
  br i1 %30, label %trace_limit, label %exit

trace_limit:                                      ; preds = %failure_exit
  %31 = tail call i64 @fwrite(ptr nonnull @1, i64 42, i64 1, ptr %23) #5
  br label %exit

exit:                                             ; preds = %runtime_error, %trace_limit, %failure_exit
  %fputc = tail call i32 @fputc(i32 10, ptr %23)
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
!1 = !DIFile(filename: "while-i32.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f7768696c652d6933322e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 4, column: 11, scope: !3)
!8 = !DILocation(line: 5, column: 20, scope: !3)
!9 = !DILocation(line: 5, column: 9, scope: !3)
!10 = !DILocation(line: 6, column: 9, scope: !3)
!11 = !DILocation(line: 0, scope: !3)
!12 = !DILocation(line: 7, column: 12, scope: !3)
