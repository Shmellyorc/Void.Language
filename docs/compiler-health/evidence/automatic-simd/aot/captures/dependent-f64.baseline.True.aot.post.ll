; ModuleID = '/dependent-f64.void'
source_filename = "/dependent-f64.void"
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
define range(i32 1, 3) i32 @__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void(i32 %0, ptr nofree readnone captures(none) %1) local_unnamed_addr #0 !dbg !3 {
block0:
  %.not16 = icmp eq i32 %0, 0, !dbg !7
  br i1 %.not16, label %block4, label %block2.preheader, !dbg !7

block2.preheader:                                 ; preds = %block0
  %.inv = icmp sgt i32 %0, -1
  %.0.be.v = select i1 %.inv, i32 1, i32 -1
  br label %block2, !dbg !7

block2:                                           ; preds = %block2.preheader, %block2
  %.018 = phi i32 [ %.0.be, %block2 ], [ 0, %block2.preheader ]
  %.01517 = phi double [ %3, %block2 ], [ 1.250000e-01, %block2.preheader ]
  %2 = fmul double %.01517, f0x3FF000001AD7F29B, !dbg !8
  %3 = fadd double %2, 1.250000e-01, !dbg !9
  %.0.be = add i32 %.018, %.0.be.v, !dbg !7
  %.not = icmp eq i32 %.0.be, %0, !dbg !7
  br i1 %.not, label %block4.loopexit, label %block2, !dbg !7

block4.loopexit:                                  ; preds = %block2
  %4 = fcmp ogt double %3, 0.000000e+00, !dbg !10
  %5 = select i1 %4, i32 1, i32 2, !dbg !10
  br label %block4, !dbg !10

block4:                                           ; preds = %block4.loopexit, %block0
  %.015.lcssa = phi i32 [ 1, %block0 ], [ %5, %block4.loopexit ], !dbg !11
  ret i32 %.015.lcssa, !dbg !11
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
  %.not16.i = icmp eq i64 %6, 0, !dbg !7
  br i1 %.not16.i, label %success, label %block2.preheader.i, !dbg !7

block2.preheader.i:                               ; preds = %run
  %.inv.i = icmp sgt i64 %6, -1
  %.0.be.v.i = select i1 %.inv.i, i32 1, i32 -1
  br label %block2.i, !dbg !7

block2.i:                                         ; preds = %block2.i, %block2.preheader.i
  %.018.i = phi i32 [ %.0.be.i, %block2.i ], [ 0, %block2.preheader.i ]
  %.01517.i = phi double [ %19, %block2.i ], [ 1.250000e-01, %block2.preheader.i ]
  %18 = fmul double %.01517.i, f0x3FF000001AD7F29B, !dbg !8
  %19 = fadd double %18, 1.250000e-01, !dbg !9
  %.0.be.i = add i32 %.018.i, %.0.be.v.i, !dbg !7
  %.not.i = icmp eq i32 %.0.be.i, %input, !dbg !7
  br i1 %.not.i, label %__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void.exit, label %block2.i, !dbg !7

__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void.exit: ; preds = %block2.i
  %20 = fcmp ogt double %19, 0.000000e+00, !dbg !10
  %21 = select i1 %20, i32 1, i32 2, !dbg !10
  %.pr = load ptr, ptr %17, align 8
  %.not = icmp eq ptr %.pr, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %22 = tail call i32 @puts(ptr nonnull dereferenceable(1) @0)
  br label %common.ret

runtime_error:                                    ; preds = %__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void.exit
  %23 = load ptr, ptr @stderr, align 8
  %fputs = tail call i32 @fputs(ptr nonnull %.pr, ptr %23) #5
  %24 = getelementptr inbounds nuw i8, ptr %17, i64 8
  %25 = load i32, ptr %24, align 8
  %.not10 = icmp eq i32 %25, 0
  br i1 %.not10, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = tail call i32 @llvm.umin.i32(i32 %25, i32 32)
  %26 = getelementptr inbounds nuw i8, ptr %17, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %entry, %run, %__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void.exit
  %.015.lcssa.i9 = phi i32 [ %21, %__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void.exit ], [ 1, %run ], [ 1, %entry ]
  %27 = tail call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @2, i32 %.015.lcssa.i9)
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
!1 = !DIFile(filename: "dependent-f64.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f646570656e64656e742d6636342e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 3, column: 5, scope: !3)
!8 = !DILocation(line: 4, column: 19, scope: !3)
!9 = !DILocation(line: 4, column: 31, scope: !3)
!10 = !DILocation(line: 5, column: 12, scope: !3)
!11 = !DILocation(line: 0, scope: !3)
