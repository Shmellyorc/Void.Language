; ModuleID = '/guarded-range-i32.void'
source_filename = "/guarded-range-i32.void"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@__vd_types__v1 = local_unnamed_addr constant [0 x { ptr, ptr, ptr, i64, ptr, i64, ptr, i32 }] zeroinitializer
@__vd_type_count__v1 = local_unnamed_addr constant i64 0
@__vd_witnesses__v1 = local_unnamed_addr constant [0 x { ptr, ptr, ptr, ptr }] zeroinitializer
@__vd_witness_count__v1 = local_unnamed_addr constant i64 0

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i32 @__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__v1(i32 %0, ptr nofree readnone captures(none) %1) local_unnamed_addr #0 !dbg !3 {
block0:
  %2 = icmp slt i32 %0, 1, !dbg !7
  br i1 %2, label %common.ret, label %block8, !dbg !7

common.ret:                                       ; preds = %block8, %block0
  %common.ret.op = phi i32 [ 0, %block0 ], [ %3, %block8 ]
  ret i32 %common.ret.op, !dbg !8

block8:                                           ; preds = %block0, %block8
  %.021 = phi i32 [ %4, %block8 ], [ 0, %block0 ]
  %.01820 = phi i32 [ %3, %block8 ], [ 0, %block0 ]
  %.urem = urem i32 %.021, 127, !dbg !9
  %3 = add i32 %.urem, %.01820, !dbg !10
  %4 = add nuw nsw i32 %.021, 1, !dbg !11
  %.not = icmp eq i32 %4, %0, !dbg !11
  br i1 %.not, label %common.ret, label %block8, !dbg !11
}

attributes #0 = { nofree norecurse nosync nounwind memory(none) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "VOID source compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "guarded-range-i32.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f677561726465642d72616e67652d6933322e766f69643a3a656e747279__v1", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 2, column: 10, scope: !3)
!8 = !DILocation(line: 0, scope: !3)
!9 = !DILocation(line: 6, column: 20, scope: !3)
!10 = !DILocation(line: 6, column: 9, scope: !3)
!11 = !DILocation(line: 5, column: 5, scope: !3)
