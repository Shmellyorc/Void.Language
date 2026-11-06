; ModuleID = '/fixed-i32.void'
source_filename = "/fixed-i32.void"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@__vd_types__void = constant [0 x { ptr, ptr, ptr, i64, ptr, i64, ptr, i32 }] zeroinitializer
@__vd_type_count__void = local_unnamed_addr constant i64 0
@__vd_witnesses__void = constant [0 x { ptr, ptr, ptr, ptr }] zeroinitializer
@__vd_witness_count__void = local_unnamed_addr constant i64 0
@gc.offsets = private constant [3 x i64] [i64 0, i64 8, i64 16]
@gc.offsets.1 = private constant [1 x i64] zeroinitializer
@gc.offsets.2 = private constant [18 x i64] [i64 0, i64 8, i64 16, i64 24, i64 32, i64 40, i64 48, i64 56, i64 64, i64 72, i64 80, i64 88, i64 96, i64 104, i64 112, i64 120, i64 128, i64 136]
@gc.slot.template = private unnamed_addr constant [3 x { ptr, i64, ptr, i64 }] [{ ptr, i64, ptr, i64 } { ptr null, i64 24, ptr @gc.offsets, i64 3 }, { ptr, i64, ptr, i64 } { ptr null, i64 8, ptr @gc.offsets.1, i64 1 }, { ptr, i64, ptr, i64 } { ptr null, i64 144, ptr @gc.offsets.2, i64 18 }], align 8
@0 = private unnamed_addr constant [177 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:2:9: runtime error: Array operation failed\0AIn function /fixed-i32.void::entry\0A\00", align 1
@1 = private unnamed_addr constant [177 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:3:9: runtime error: Array operation failed\0AIn function /fixed-i32.void::entry\0A\00", align 1
@2 = private unnamed_addr constant [177 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:4:9: runtime error: Array operation failed\0AIn function /fixed-i32.void::entry\0A\00", align 1
@3 = private unnamed_addr constant [231 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:6:11: runtime error: Fixed array index outside extent\0A        a[i] = (i % 31) as i32\0A          ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@4 = private unnamed_addr constant [241 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:6:11: runtime error: Array operation failed during index access\0A        a[i] = (i % 31) as i32\0A          ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@5 = private unnamed_addr constant [221 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:6:11: runtime error: Array operation failed\0A        a[i] = (i % 31) as i32\0A          ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@6 = private unnamed_addr constant [177 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:6:9: runtime error: Array operation failed\0AIn function /fixed-i32.void::entry\0A\00", align 1
@7 = private unnamed_addr constant [240 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:7:11: runtime error: Array operation failed during index access\0A        b[i] = (i % 7) as i32\0A          ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@8 = private unnamed_addr constant [220 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:7:11: runtime error: Array operation failed\0A        b[i] = (i % 7) as i32\0A          ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@9 = private unnamed_addr constant [177 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:7:9: runtime error: Array operation failed\0AIn function /fixed-i32.void::entry\0A\00", align 1
@10 = private unnamed_addr constant [235 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:9:13: runtime error: Fixed array index outside extent\0A        out[i] = a[i] * 3 + b[i]\0A            ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@11 = private unnamed_addr constant [245 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:9:13: runtime error: Array operation failed during index access\0A        out[i] = a[i] * 3 + b[i]\0A            ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@12 = private unnamed_addr constant [251 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:9:19: runtime error: Array operation failed during index access\0A        out[i] = a[i] * 3 + b[i]\0A                  ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@13 = private unnamed_addr constant [262 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:9:30: runtime error: Array operation failed during index access\0A        out[i] = a[i] * 3 + b[i]\0A                             ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@14 = private unnamed_addr constant [225 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:9:13: runtime error: Array operation failed\0A        out[i] = a[i] * 3 + b[i]\0A            ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@15 = private unnamed_addr constant [177 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:9:9: runtime error: Array operation failed\0AIn function /fixed-i32.void::entry\0A\00", align 1
@16 = private unnamed_addr constant [236 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:12:15: runtime error: Fixed array index outside extent\0A        if out[i] != expected:\0A              ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@17 = private unnamed_addr constant [246 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/fixed-i32.void:12:15: runtime error: Array operation failed during index access\0A        if out[i] != expected:\0A              ^\0AIn function /fixed-i32.void::entry\0A\00", align 1
@18 = private unnamed_addr constant [46 x i8] c"VOID: expected one optional decimal i32 input\00", align 1
@stderr = external local_unnamed_addr global ptr
@19 = private unnamed_addr constant [43 x i8] c"\0ASource call trace limited to 32 callers.\0A\00", align 1
@20 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define range(i32 0, 3) i32 @__vd_2f66697865642d6933322e766f69643a3a656e747279__void(i32 %0, ptr %1) !dbg !3 {
block0:
  %2 = alloca { ptr, ptr, ptr }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(24) %2, i8 0, i64 24, i1 false)
  %3 = getelementptr inbounds nuw i8, ptr %2, i64 8
  %4 = getelementptr inbounds nuw i8, ptr %2, i64 16
  %5 = alloca ptr, align 8
  store ptr null, ptr %5, align 8
  %6 = alloca { ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(144) %6, i8 0, i64 144, i1 false)
  %7 = getelementptr inbounds nuw i8, ptr %6, i64 8
  %8 = getelementptr inbounds nuw i8, ptr %6, i64 16
  %9 = getelementptr inbounds nuw i8, ptr %6, i64 24
  %10 = getelementptr inbounds nuw i8, ptr %6, i64 32
  %11 = getelementptr inbounds nuw i8, ptr %6, i64 40
  %12 = getelementptr inbounds nuw i8, ptr %6, i64 48
  %13 = getelementptr inbounds nuw i8, ptr %6, i64 56
  %14 = getelementptr inbounds nuw i8, ptr %6, i64 64
  %15 = getelementptr inbounds nuw i8, ptr %6, i64 72
  %16 = getelementptr inbounds nuw i8, ptr %6, i64 80
  %17 = getelementptr inbounds nuw i8, ptr %6, i64 88
  %18 = getelementptr inbounds nuw i8, ptr %6, i64 96
  %19 = getelementptr inbounds nuw i8, ptr %6, i64 104
  %20 = getelementptr inbounds nuw i8, ptr %6, i64 112
  %21 = getelementptr inbounds nuw i8, ptr %6, i64 120
  %22 = getelementptr inbounds nuw i8, ptr %6, i64 128
  %23 = getelementptr inbounds nuw i8, ptr %6, i64 136
  %24 = alloca [3 x { ptr, i64, ptr, i64 }], align 8
  call void @llvm.memcpy.p0.p0.i64(ptr noundef nonnull align 8 dereferenceable(96) %24, ptr noundef nonnull align 8 dereferenceable(96) @gc.slot.template, i64 96, i1 false)
  store ptr %2, ptr %24, align 8
  %25 = getelementptr inbounds nuw i8, ptr %24, i64 32
  store ptr %5, ptr %25, align 8
  %26 = getelementptr inbounds nuw i8, ptr %24, i64 64
  store ptr %6, ptr %26, align 8
  %27 = alloca [40 x i8], align 8
  call void @vd_gc_frame_enter(ptr nonnull %27, ptr nonnull %24, i64 3)
  call void @vd_gc_poll()
  %28 = call i1 @vd_worker_checkpoint(ptr %1)
  br i1 %28, label %worker.cancelled, label %worker.continue

block4:                                           ; preds = %worker.continue44
  br i1 %69, label %worker.cancelled45, label %worker.continue46, !dbg !7

block5:                                           ; preds = %worker.continue44
  br i1 %69, label %worker.cancelled47, label %worker.continue48, !dbg !8

block6:                                           ; preds = %worker.continue48
  br i1 %72, label %worker.cancelled49, label %block1.backedge, !dbg !7

block7:                                           ; preds = %worker.continue48
  br i1 %72, label %worker.cancelled51, label %block1.backedge, !dbg !7

block11:                                          ; preds = %worker.continue86
  br i1 %106, label %worker.cancelled87, label %block15.preheader, !dbg !8

block15.preheader:                                ; preds = %block11
  call void @vd_gc_poll(), !dbg !8
  %29 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !8
  br i1 %29, label %worker.cancelled95, label %worker.continue96, !dbg !8

block12:                                          ; preds = %worker.continue86
  br i1 %106, label %worker.cancelled89, label %worker.continue90, !dbg !9

block13:                                          ; preds = %worker.continue90
  br i1 %107, label %worker.cancelled91, label %block8.backedge, !dbg !8

block14:                                          ; preds = %worker.continue90
  br i1 %107, label %worker.cancelled93, label %block8.backedge, !dbg !8

block18:                                          ; preds = %worker.continue112
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !10
  %not. = xor i1 %129, true, !dbg !10
  %. = zext i1 %not. to i32, !dbg !10
  br label %common.ret, !dbg !10

block19:                                          ; preds = %numeric_valid110
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !10
  %.127 = select i1 %127, i32 0, i32 2, !dbg !10
  br label %common.ret, !dbg !10

block20:                                          ; preds = %numeric_valid110
  br i1 %127, label %worker.cancelled117, label %worker.continue118, !dbg !11

block22:                                          ; preds = %worker.continue112
  br i1 %129, label %worker.cancelled121, label %worker.continue122, !dbg !11

block23:                                          ; preds = %worker.continue122
  br i1 %132, label %worker.cancelled123, label %block15.backedge, !dbg !9

block24:                                          ; preds = %worker.continue122
  br i1 %132, label %worker.cancelled125, label %block15.backedge, !dbg !9

common.ret:                                       ; preds = %block19, %block18, %worker.cancelled125, %worker.cancelled123, %worker.cancelled121, %worker.cancelled119, %worker.cancelled117, %worker.cancelled111, %numeric_error109, %numeric_error105, %numeric_error103, %worker.cancelled97, %worker.cancelled95, %worker.cancelled93, %worker.cancelled91, %worker.cancelled89, %worker.cancelled87, %worker.cancelled85, %numeric_error83, %numeric_error81, %numeric_error79, %numeric_error75, %numeric_error71, %numeric_error67, %numeric_error63, %numeric_error59, %numeric_error57, %worker.cancelled55, %worker.cancelled53, %worker.cancelled51, %worker.cancelled49, %worker.cancelled47, %worker.cancelled45, %worker.cancelled43, %numeric_error41, %numeric_error39, %numeric_error37, %numeric_error31, %numeric_error27, %numeric_error25, %numeric_error23, %numeric_error17, %numeric_error15, %worker.cancelled13, %worker.cancelled11, %numeric_error9, %numeric_error7, %numeric_error5, %numeric_error3, %numeric_error1, %numeric_error, %worker.cancelled
  %common.ret.op = phi i32 [ 0, %worker.cancelled ], [ 0, %numeric_error ], [ 0, %numeric_error1 ], [ 0, %numeric_error3 ], [ 0, %numeric_error5 ], [ 0, %numeric_error7 ], [ 0, %numeric_error9 ], [ 0, %worker.cancelled11 ], [ 0, %worker.cancelled13 ], [ 0, %numeric_error15 ], [ 0, %numeric_error17 ], [ 0, %worker.cancelled121 ], [ 0, %numeric_error23 ], [ 0, %numeric_error25 ], [ 0, %numeric_error27 ], [ 0, %worker.cancelled119 ], [ 0, %numeric_error31 ], [ 0, %worker.cancelled117 ], [ 0, %numeric_error37 ], [ 0, %numeric_error39 ], [ 0, %numeric_error41 ], [ 0, %worker.cancelled43 ], [ 0, %worker.cancelled45 ], [ 0, %worker.cancelled47 ], [ 0, %worker.cancelled49 ], [ 0, %worker.cancelled51 ], [ 0, %worker.cancelled53 ], [ 0, %worker.cancelled55 ], [ 0, %numeric_error57 ], [ 0, %numeric_error59 ], [ 0, %worker.cancelled123 ], [ 0, %numeric_error63 ], [ 0, %worker.cancelled125 ], [ 0, %numeric_error67 ], [ %., %block18 ], [ 0, %numeric_error71 ], [ %.127, %block19 ], [ 0, %numeric_error75 ], [ 0, %worker.cancelled111 ], [ 0, %numeric_error79 ], [ 0, %numeric_error81 ], [ 0, %numeric_error83 ], [ 0, %worker.cancelled85 ], [ 0, %worker.cancelled87 ], [ 0, %worker.cancelled89 ], [ 0, %worker.cancelled91 ], [ 0, %worker.cancelled93 ], [ 0, %worker.cancelled95 ], [ 0, %worker.cancelled97 ], [ 0, %numeric_error103 ], [ 0, %numeric_error105 ], [ 0, %numeric_error109 ]
  ret i32 %common.ret.op, !dbg !10

worker.cancelled:                                 ; preds = %block0
  call void @vd_gc_frame_leave(ptr nonnull %27)
  br label %common.ret

worker.continue:                                  ; preds = %block0
  call void @vd_array_default(ptr nonnull %6, i32 2, i64 1025, i64 0, ptr %1), !dbg !12
  %30 = load ptr, ptr %1, align 8, !dbg !12
  %.not = icmp eq ptr %30, null, !dbg !12
  br i1 %.not, label %numeric_valid, label %numeric_error, !dbg !12

numeric_error:                                    ; preds = %worker.continue
  store ptr @0, ptr %1, align 8, !dbg !12
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !12
  br label %common.ret, !dbg !12

numeric_valid:                                    ; preds = %worker.continue
  call void @vd_array_copy(ptr nonnull %5, ptr nonnull %6, i32 2, i64 1025, ptr nonnull %1), !dbg !12
  %31 = load ptr, ptr %1, align 8, !dbg !12
  %.not194 = icmp eq ptr %31, null, !dbg !12
  br i1 %.not194, label %numeric_valid2, label %numeric_error1, !dbg !12

numeric_error1:                                   ; preds = %numeric_valid
  store ptr @0, ptr %1, align 8, !dbg !12
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !12
  br label %common.ret, !dbg !12

numeric_valid2:                                   ; preds = %numeric_valid
  %32 = load ptr, ptr %5, align 8, !dbg !12
  store ptr %32, ptr %2, align 8, !dbg !12
  call void @vd_array_default(ptr nonnull %7, i32 2, i64 1025, i64 0, ptr nonnull %1), !dbg !13
  %33 = load ptr, ptr %1, align 8, !dbg !13
  %.not195 = icmp eq ptr %33, null, !dbg !13
  br i1 %.not195, label %numeric_valid4, label %numeric_error3, !dbg !13

numeric_error3:                                   ; preds = %numeric_valid2
  store ptr @1, ptr %1, align 8, !dbg !13
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !13
  br label %common.ret, !dbg !13

numeric_valid4:                                   ; preds = %numeric_valid2
  call void @vd_array_copy(ptr nonnull %5, ptr nonnull %7, i32 2, i64 1025, ptr nonnull %1), !dbg !13
  %34 = load ptr, ptr %1, align 8, !dbg !13
  %.not196 = icmp eq ptr %34, null, !dbg !13
  br i1 %.not196, label %numeric_valid6, label %numeric_error5, !dbg !13

numeric_error5:                                   ; preds = %numeric_valid4
  store ptr @1, ptr %1, align 8, !dbg !13
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !13
  br label %common.ret, !dbg !13

numeric_valid6:                                   ; preds = %numeric_valid4
  %35 = load ptr, ptr %5, align 8, !dbg !13
  store ptr %35, ptr %3, align 8, !dbg !13
  call void @vd_array_default(ptr nonnull %8, i32 2, i64 1025, i64 0, ptr nonnull %1), !dbg !14
  %36 = load ptr, ptr %1, align 8, !dbg !14
  %.not197 = icmp eq ptr %36, null, !dbg !14
  br i1 %.not197, label %numeric_valid8, label %numeric_error7, !dbg !14

numeric_error7:                                   ; preds = %numeric_valid6
  store ptr @2, ptr %1, align 8, !dbg !14
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !14
  br label %common.ret, !dbg !14

numeric_valid8:                                   ; preds = %numeric_valid6
  call void @vd_array_copy(ptr nonnull %5, ptr nonnull %8, i32 2, i64 1025, ptr nonnull %1), !dbg !14
  %37 = load ptr, ptr %1, align 8, !dbg !14
  %.not198 = icmp eq ptr %37, null, !dbg !14
  br i1 %.not198, label %numeric_valid10, label %numeric_error9, !dbg !14

numeric_error9:                                   ; preds = %numeric_valid8
  store ptr @2, ptr %1, align 8, !dbg !14
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !14
  br label %common.ret, !dbg !14

numeric_valid10:                                  ; preds = %numeric_valid8
  %38 = load ptr, ptr %5, align 8, !dbg !14
  store ptr %38, ptr %4, align 8, !dbg !14
  %39 = icmp sgt i32 %0, -1, !dbg !7
  call void @vd_gc_poll(), !dbg !7
  %40 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %40, label %worker.cancelled11, label %worker.continue12, !dbg !7

worker.cancelled11:                               ; preds = %block1.backedge, %numeric_valid10
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

worker.continue12:                                ; preds = %numeric_valid10, %block1.backedge
  %.0189218 = phi i32 [ %73, %block1.backedge ], [ 0, %numeric_valid10 ]
  call void @vd_gc_poll(), !dbg !7
  %41 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %41, label %worker.cancelled13, label %worker.continue14, !dbg !7

worker.cancelled13:                               ; preds = %worker.continue12
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

worker.continue14:                                ; preds = %worker.continue12
  %42 = load ptr, ptr %2, align 8, !dbg !15
  store ptr %42, ptr %9, align 8, !dbg !15
  %43 = sext i32 %.0189218 to i64, !dbg !16
  %44 = icmp slt i32 %.0189218, 0, !dbg !16
  %45 = add nsw i64 %43, 1025, !dbg !16
  %46 = select i1 %44, i64 %45, i64 %43, !dbg !16
  %47 = icmp ugt i64 %46, 1024, !dbg !16
  br i1 %47, label %numeric_error15, label %numeric_valid16, !dbg !16

numeric_error15:                                  ; preds = %worker.continue14
  store ptr @3, ptr %1, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !16
  br label %common.ret, !dbg !16

numeric_valid16:                                  ; preds = %worker.continue14
  %48 = call i64 @vd_array_read(ptr nonnull %9, i64 %46, i32 2, i64 1025, ptr %1), !dbg !16
  %49 = load ptr, ptr %1, align 8, !dbg !16
  %.not199 = icmp eq ptr %49, null, !dbg !16
  br i1 %.not199, label %numeric_valid18, label %numeric_error17, !dbg !16

numeric_error17:                                  ; preds = %numeric_valid16
  store ptr @4, ptr %1, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !16
  br label %common.ret, !dbg !16

numeric_valid18:                                  ; preds = %numeric_valid16
  %50 = load ptr, ptr %2, align 8, !dbg !15
  store ptr %50, ptr %10, align 8, !dbg !15
  call void @vd_array_copy(ptr nonnull %11, ptr nonnull %10, i32 2, i64 1025, ptr nonnull %1), !dbg !16
  %51 = load ptr, ptr %1, align 8, !dbg !16
  %.not200 = icmp eq ptr %51, null, !dbg !16
  br i1 %.not200, label %numeric_valid24, label %numeric_error23, !dbg !16

numeric_error23:                                  ; preds = %numeric_valid18
  store ptr @5, ptr %1, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !16
  br label %common.ret, !dbg !16

numeric_valid24:                                  ; preds = %numeric_valid18
  %52 = srem i32 %.0189218, 31, !dbg !17
  %53 = zext i32 %52 to i64, !dbg !16
  call void @vd_array_write(ptr nonnull %11, i64 %46, i64 %53, i32 2, i64 1025, ptr nonnull %1), !dbg !16
  %54 = load ptr, ptr %1, align 8, !dbg !16
  %.not201 = icmp eq ptr %54, null, !dbg !16
  br i1 %.not201, label %numeric_valid26, label %numeric_error25, !dbg !16

numeric_error25:                                  ; preds = %numeric_valid24
  store ptr @5, ptr %1, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !16
  br label %common.ret, !dbg !16

numeric_valid26:                                  ; preds = %numeric_valid24
  call void @vd_array_copy(ptr nonnull %5, ptr nonnull %11, i32 2, i64 1025, ptr nonnull %1), !dbg !15
  %55 = load ptr, ptr %1, align 8, !dbg !15
  %.not202 = icmp eq ptr %55, null, !dbg !15
  br i1 %.not202, label %numeric_valid28, label %numeric_error27, !dbg !15

numeric_error27:                                  ; preds = %numeric_valid26
  store ptr @6, ptr %1, align 8, !dbg !15
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !15
  br label %common.ret, !dbg !15

numeric_valid28:                                  ; preds = %numeric_valid26
  %56 = load ptr, ptr %5, align 8, !dbg !15
  store ptr %56, ptr %2, align 8, !dbg !15
  %57 = load ptr, ptr %3, align 8, !dbg !18
  store ptr %57, ptr %12, align 8, !dbg !18
  %58 = call i64 @vd_array_read(ptr nonnull %12, i64 %46, i32 2, i64 1025, ptr nonnull %1), !dbg !19
  %59 = load ptr, ptr %1, align 8, !dbg !19
  %.not203 = icmp eq ptr %59, null, !dbg !19
  br i1 %.not203, label %numeric_valid32, label %numeric_error31, !dbg !19

numeric_error31:                                  ; preds = %numeric_valid28
  store ptr @7, ptr %1, align 8, !dbg !19
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !19
  br label %common.ret, !dbg !19

numeric_valid32:                                  ; preds = %numeric_valid28
  %60 = load ptr, ptr %3, align 8, !dbg !18
  store ptr %60, ptr %13, align 8, !dbg !18
  call void @vd_array_copy(ptr nonnull %14, ptr nonnull %13, i32 2, i64 1025, ptr nonnull %1), !dbg !19
  %61 = load ptr, ptr %1, align 8, !dbg !19
  %.not204 = icmp eq ptr %61, null, !dbg !19
  br i1 %.not204, label %numeric_valid38, label %numeric_error37, !dbg !19

numeric_error37:                                  ; preds = %numeric_valid32
  store ptr @8, ptr %1, align 8, !dbg !19
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !19
  br label %common.ret, !dbg !19

numeric_valid38:                                  ; preds = %numeric_valid32
  %62 = srem i32 %.0189218, 7, !dbg !20
  %63 = zext i32 %62 to i64, !dbg !19
  call void @vd_array_write(ptr nonnull %14, i64 %46, i64 %63, i32 2, i64 1025, ptr nonnull %1), !dbg !19
  %64 = load ptr, ptr %1, align 8, !dbg !19
  %.not205 = icmp eq ptr %64, null, !dbg !19
  br i1 %.not205, label %numeric_valid40, label %numeric_error39, !dbg !19

numeric_error39:                                  ; preds = %numeric_valid38
  store ptr @8, ptr %1, align 8, !dbg !19
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !19
  br label %common.ret, !dbg !19

numeric_valid40:                                  ; preds = %numeric_valid38
  call void @vd_array_copy(ptr nonnull %5, ptr nonnull %14, i32 2, i64 1025, ptr nonnull %1), !dbg !18
  %65 = load ptr, ptr %1, align 8, !dbg !18
  %.not206 = icmp eq ptr %65, null, !dbg !18
  br i1 %.not206, label %numeric_valid42, label %numeric_error41, !dbg !18

numeric_error41:                                  ; preds = %numeric_valid40
  store ptr @9, ptr %1, align 8, !dbg !18
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !18
  br label %common.ret, !dbg !18

numeric_valid42:                                  ; preds = %numeric_valid40
  %66 = load ptr, ptr %5, align 8, !dbg !18
  store ptr %66, ptr %3, align 8, !dbg !18
  call void @vd_gc_poll(), !dbg !18
  %67 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !18
  br i1 %67, label %worker.cancelled43, label %worker.continue44, !dbg !18

worker.cancelled43:                               ; preds = %numeric_valid42
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !18
  br label %common.ret, !dbg !18

worker.continue44:                                ; preds = %numeric_valid42
  %68 = icmp eq i32 %.0189218, %0, !dbg !7
  call void @vd_gc_poll(), !dbg !10
  %69 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !10
  br i1 %68, label %block4, label %block5, !dbg !7

worker.cancelled45:                               ; preds = %block4
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

worker.continue46:                                ; preds = %block4
  %70 = icmp sgt i32 %0, 0, !dbg !8
  call void @vd_gc_poll(), !dbg !7
  %71 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %71, label %worker.cancelled53, label %worker.continue54, !dbg !7

worker.cancelled47:                               ; preds = %block5
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !8
  br label %common.ret, !dbg !8

worker.continue48:                                ; preds = %block5
  call void @vd_gc_poll(), !dbg !7
  %72 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %39, label %block6, label %block7, !dbg !7

worker.cancelled49:                               ; preds = %block6
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

block1.backedge:                                  ; preds = %block6, %block7
  %.sink = phi i32 [ -1, %block7 ], [ 1, %block6 ]
  %73 = add i32 %.0189218, %.sink, !dbg !7
  call void @vd_gc_poll(), !dbg !7
  %74 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %74, label %worker.cancelled11, label %worker.continue12, !dbg !7

worker.cancelled51:                               ; preds = %block7
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

worker.cancelled53:                               ; preds = %block8.backedge, %worker.continue46
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

worker.continue54:                                ; preds = %worker.continue46, %block8.backedge
  %.0188219 = phi i32 [ %108, %block8.backedge ], [ 1, %worker.continue46 ]
  call void @vd_gc_poll(), !dbg !7
  %75 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %75, label %worker.cancelled55, label %worker.continue56, !dbg !7

worker.cancelled55:                               ; preds = %worker.continue54
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !7
  br label %common.ret, !dbg !7

worker.continue56:                                ; preds = %worker.continue54
  %76 = load ptr, ptr %4, align 8, !dbg !21
  store ptr %76, ptr %15, align 8, !dbg !21
  %77 = sext i32 %.0188219 to i64, !dbg !22
  %78 = icmp slt i32 %.0188219, 0, !dbg !22
  %79 = add nsw i64 %77, 1025, !dbg !22
  %80 = select i1 %78, i64 %79, i64 %77, !dbg !22
  %81 = icmp ugt i64 %80, 1024, !dbg !22
  br i1 %81, label %numeric_error57, label %numeric_valid58, !dbg !22

numeric_error57:                                  ; preds = %worker.continue56
  store ptr @10, ptr %1, align 8, !dbg !22
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !22
  br label %common.ret, !dbg !22

numeric_valid58:                                  ; preds = %worker.continue56
  %82 = call i64 @vd_array_read(ptr nonnull %15, i64 %80, i32 2, i64 1025, ptr %1), !dbg !22
  %83 = load ptr, ptr %1, align 8, !dbg !22
  %.not207 = icmp eq ptr %83, null, !dbg !22
  br i1 %.not207, label %numeric_valid60, label %numeric_error59, !dbg !22

numeric_error59:                                  ; preds = %numeric_valid58
  store ptr @11, ptr %1, align 8, !dbg !22
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !22
  br label %common.ret, !dbg !22

numeric_valid60:                                  ; preds = %numeric_valid58
  %84 = load ptr, ptr %2, align 8, !dbg !23
  store ptr %84, ptr %16, align 8, !dbg !23
  %85 = call i64 @vd_array_read(ptr nonnull %16, i64 %80, i32 2, i64 1025, ptr nonnull %1), !dbg !24
  %86 = load ptr, ptr %1, align 8, !dbg !24
  %.not208 = icmp eq ptr %86, null, !dbg !24
  br i1 %.not208, label %numeric_valid64, label %numeric_error63, !dbg !24

numeric_error63:                                  ; preds = %numeric_valid60
  store ptr @12, ptr %1, align 8, !dbg !24
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !24
  br label %common.ret, !dbg !24

numeric_valid64:                                  ; preds = %numeric_valid60
  %87 = load ptr, ptr %2, align 8, !dbg !23
  store ptr %87, ptr %17, align 8, !dbg !23
  %88 = call i64 @vd_array_read(ptr nonnull %17, i64 %80, i32 2, i64 1025, ptr nonnull %1), !dbg !24
  %89 = load ptr, ptr %1, align 8, !dbg !24
  %.not209 = icmp eq ptr %89, null, !dbg !24
  br i1 %.not209, label %numeric_valid68, label %numeric_error67, !dbg !24

numeric_error67:                                  ; preds = %numeric_valid64
  store ptr @12, ptr %1, align 8, !dbg !24
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !24
  br label %common.ret, !dbg !24

numeric_valid68:                                  ; preds = %numeric_valid64
  %90 = mul i64 %88, 3, !dbg !25
  %91 = load ptr, ptr %3, align 8, !dbg !26
  store ptr %91, ptr %18, align 8, !dbg !26
  %92 = call i64 @vd_array_read(ptr nonnull %18, i64 %80, i32 2, i64 1025, ptr nonnull %1), !dbg !27
  %93 = load ptr, ptr %1, align 8, !dbg !27
  %.not210 = icmp eq ptr %93, null, !dbg !27
  br i1 %.not210, label %numeric_valid72, label %numeric_error71, !dbg !27

numeric_error71:                                  ; preds = %numeric_valid68
  store ptr @13, ptr %1, align 8, !dbg !27
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !27
  br label %common.ret, !dbg !27

numeric_valid72:                                  ; preds = %numeric_valid68
  %94 = load ptr, ptr %3, align 8, !dbg !26
  store ptr %94, ptr %19, align 8, !dbg !26
  %95 = call i64 @vd_array_read(ptr nonnull %19, i64 %80, i32 2, i64 1025, ptr nonnull %1), !dbg !27
  %96 = load ptr, ptr %1, align 8, !dbg !27
  %.not211 = icmp eq ptr %96, null, !dbg !27
  br i1 %.not211, label %numeric_valid76, label %numeric_error75, !dbg !27

numeric_error75:                                  ; preds = %numeric_valid72
  store ptr @13, ptr %1, align 8, !dbg !27
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !27
  br label %common.ret, !dbg !27

numeric_valid76:                                  ; preds = %numeric_valid72
  %97 = load ptr, ptr %4, align 8, !dbg !21
  store ptr %97, ptr %20, align 8, !dbg !21
  call void @vd_array_copy(ptr nonnull %21, ptr nonnull %20, i32 2, i64 1025, ptr nonnull %1), !dbg !22
  %98 = load ptr, ptr %1, align 8, !dbg !22
  %.not212 = icmp eq ptr %98, null, !dbg !22
  br i1 %.not212, label %numeric_valid80, label %numeric_error79, !dbg !22

numeric_error79:                                  ; preds = %numeric_valid76
  store ptr @14, ptr %1, align 8, !dbg !22
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !22
  br label %common.ret, !dbg !22

numeric_valid80:                                  ; preds = %numeric_valid76
  %99 = add i64 %95, %90, !dbg !28
  %100 = and i64 %99, 4294967295, !dbg !22
  call void @vd_array_write(ptr nonnull %21, i64 %80, i64 %100, i32 2, i64 1025, ptr nonnull %1), !dbg !22
  %101 = load ptr, ptr %1, align 8, !dbg !22
  %.not213 = icmp eq ptr %101, null, !dbg !22
  br i1 %.not213, label %numeric_valid82, label %numeric_error81, !dbg !22

numeric_error81:                                  ; preds = %numeric_valid80
  store ptr @14, ptr %1, align 8, !dbg !22
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !22
  br label %common.ret, !dbg !22

numeric_valid82:                                  ; preds = %numeric_valid80
  call void @vd_array_copy(ptr nonnull %5, ptr nonnull %21, i32 2, i64 1025, ptr nonnull %1), !dbg !21
  %102 = load ptr, ptr %1, align 8, !dbg !21
  %.not214 = icmp eq ptr %102, null, !dbg !21
  br i1 %.not214, label %numeric_valid84, label %numeric_error83, !dbg !21

numeric_error83:                                  ; preds = %numeric_valid82
  store ptr @15, ptr %1, align 8, !dbg !21
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !21
  br label %common.ret, !dbg !21

numeric_valid84:                                  ; preds = %numeric_valid82
  %103 = load ptr, ptr %5, align 8, !dbg !21
  store ptr %103, ptr %4, align 8, !dbg !21
  call void @vd_gc_poll(), !dbg !21
  %104 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !21
  br i1 %104, label %worker.cancelled85, label %worker.continue86, !dbg !21

worker.cancelled85:                               ; preds = %numeric_valid84
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !21
  br label %common.ret, !dbg !21

worker.continue86:                                ; preds = %numeric_valid84
  %105 = icmp eq i32 %.0188219, %0, !dbg !8
  call void @vd_gc_poll(), !dbg !10
  %106 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !10
  br i1 %105, label %block11, label %block12, !dbg !8

worker.cancelled87:                               ; preds = %block11
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !8
  br label %common.ret, !dbg !8

worker.cancelled89:                               ; preds = %block12
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !9
  br label %common.ret, !dbg !9

worker.continue90:                                ; preds = %block12
  call void @vd_gc_poll(), !dbg !8
  %107 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !8
  br i1 %70, label %block13, label %block14, !dbg !8

worker.cancelled91:                               ; preds = %block13
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !8
  br label %common.ret, !dbg !8

block8.backedge:                                  ; preds = %block13, %block14
  %.sink224 = phi i32 [ -1, %block14 ], [ 1, %block13 ]
  %108 = add i32 %.0188219, %.sink224, !dbg !8
  call void @vd_gc_poll(), !dbg !7
  %109 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %109, label %worker.cancelled53, label %worker.continue54, !dbg !7

worker.cancelled93:                               ; preds = %block14
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !8
  br label %common.ret, !dbg !8

worker.cancelled95:                               ; preds = %block15.backedge, %block15.preheader
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !8
  br label %common.ret, !dbg !8

worker.continue96:                                ; preds = %block15.preheader, %block15.backedge
  %.0220 = phi i32 [ %133, %block15.backedge ], [ 1, %block15.preheader ]
  call void @vd_gc_poll(), !dbg !8
  %110 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !8
  br i1 %110, label %worker.cancelled97, label %worker.continue98, !dbg !8

worker.cancelled97:                               ; preds = %worker.continue96
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !8
  br label %common.ret, !dbg !8

worker.continue98:                                ; preds = %worker.continue96
  %111 = srem i32 %.0220, 31, !dbg !29
  %112 = mul nsw i32 %111, 3, !dbg !30
  %113 = srem i32 %.0220, 7, !dbg !31
  %114 = add nsw i32 %112, %113, !dbg !32
  %115 = load ptr, ptr %4, align 8, !dbg !33
  store ptr %115, ptr %22, align 8, !dbg !33
  %116 = sext i32 %.0220 to i64, !dbg !34
  %117 = icmp slt i32 %.0220, 0, !dbg !34
  %118 = add nsw i64 %116, 1025, !dbg !34
  %119 = select i1 %117, i64 %118, i64 %116, !dbg !34
  %120 = icmp ugt i64 %119, 1024, !dbg !34
  br i1 %120, label %numeric_error103, label %numeric_valid104, !dbg !34

numeric_error103:                                 ; preds = %worker.continue98
  store ptr @16, ptr %1, align 8, !dbg !34
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !34
  br label %common.ret, !dbg !34

numeric_valid104:                                 ; preds = %worker.continue98
  %121 = call i64 @vd_array_read(ptr nonnull %22, i64 %119, i32 2, i64 1025, ptr %1), !dbg !34
  %122 = load ptr, ptr %1, align 8, !dbg !34
  %.not215 = icmp eq ptr %122, null, !dbg !34
  br i1 %.not215, label %numeric_valid106, label %numeric_error105, !dbg !34

numeric_error105:                                 ; preds = %numeric_valid104
  store ptr @17, ptr %1, align 8, !dbg !34
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !34
  br label %common.ret, !dbg !34

numeric_valid106:                                 ; preds = %numeric_valid104
  %123 = load ptr, ptr %4, align 8, !dbg !33
  store ptr %123, ptr %23, align 8, !dbg !33
  %124 = call i64 @vd_array_read(ptr nonnull %23, i64 %119, i32 2, i64 1025, ptr nonnull %1), !dbg !34
  %125 = load ptr, ptr %1, align 8, !dbg !34
  %.not216 = icmp eq ptr %125, null, !dbg !34
  br i1 %.not216, label %numeric_valid110, label %numeric_error109, !dbg !34

numeric_error109:                                 ; preds = %numeric_valid106
  store ptr @17, ptr %1, align 8, !dbg !34
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !34
  br label %common.ret, !dbg !34

numeric_valid110:                                 ; preds = %numeric_valid106
  %126 = trunc i64 %124 to i32, !dbg !34
  %.not217 = icmp eq i32 %114, %126, !dbg !35
  call void @vd_gc_poll(), !dbg !10
  %127 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !10
  br i1 %.not217, label %block20, label %block19, !dbg !35

worker.cancelled111:                              ; preds = %worker.continue120
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !35
  br label %common.ret, !dbg !35

worker.continue112:                               ; preds = %worker.continue120
  %128 = icmp eq i32 %.0220, %0, !dbg !9
  call void @vd_gc_poll(), !dbg !10
  %129 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !10
  br i1 %128, label %block18, label %block22, !dbg !9

worker.cancelled117:                              ; preds = %block20
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !11
  br label %common.ret, !dbg !11

worker.continue118:                               ; preds = %block20
  call void @vd_gc_poll(), !dbg !11
  %130 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !11
  br i1 %130, label %worker.cancelled119, label %worker.continue120, !dbg !11

worker.cancelled119:                              ; preds = %worker.continue118
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !11
  br label %common.ret, !dbg !11

worker.continue120:                               ; preds = %worker.continue118
  call void @vd_gc_poll(), !dbg !35
  %131 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !35
  br i1 %131, label %worker.cancelled111, label %worker.continue112, !dbg !35

worker.cancelled121:                              ; preds = %block22
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !11
  br label %common.ret, !dbg !11

worker.continue122:                               ; preds = %block22
  call void @vd_gc_poll(), !dbg !9
  %132 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !9
  br i1 %70, label %block23, label %block24, !dbg !9

worker.cancelled123:                              ; preds = %block23
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !9
  br label %common.ret, !dbg !9

block15.backedge:                                 ; preds = %block23, %block24
  %.sink225 = phi i32 [ -1, %block24 ], [ 1, %block23 ]
  %133 = add i32 %.0220, %.sink225, !dbg !9
  call void @vd_gc_poll(), !dbg !8
  %134 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !8
  br i1 %134, label %worker.cancelled95, label %worker.continue96, !dbg !8

worker.cancelled125:                              ; preds = %block24
  call void @vd_gc_frame_leave(ptr nonnull %27), !dbg !9
  br label %common.ret, !dbg !9
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #1

declare void @vd_gc_frame_enter(ptr, ptr, i64) local_unnamed_addr

declare void @vd_gc_poll() local_unnamed_addr

declare i1 @vd_worker_checkpoint(ptr) local_unnamed_addr

declare void @vd_gc_frame_leave(ptr) local_unnamed_addr

declare void @vd_array_default(ptr, i32, i64, i64, ptr) local_unnamed_addr

declare void @vd_array_copy(ptr, ptr, i32, i64, ptr) local_unnamed_addr

declare i64 @vd_array_read(ptr, i64, i32, i64, ptr) local_unnamed_addr

declare void @vd_array_write(ptr, i64, i64, i32, i64, ptr) local_unnamed_addr

define range(i32 0, 3) i32 @main(i32 %0, ptr nofree readonly captures(none) %1) local_unnamed_addr {
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
  %19 = call i32 @vd_runtime_entry_metadata(ptr nonnull @__vd_2f66697865642d6933322e766f69643a3a656e747279__void, i32 %17, ptr nonnull %18, ptr nonnull @__vd_types__void, i64 0, ptr nonnull @__vd_witnesses__void, i64 0)
  %20 = load ptr, ptr %18, align 8
  %.not = icmp eq ptr %20, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %21 = tail call i32 @puts(ptr nonnull dereferenceable(1) @18)
  br label %common.ret

runtime_error:                                    ; preds = %run
  %22 = load ptr, ptr @stderr, align 8
  %fputs = call i32 @fputs(ptr nonnull %20, ptr %22) #5
  %23 = getelementptr inbounds nuw i8, ptr %18, i64 8
  %24 = load i32, ptr %23, align 8
  %.not6 = icmp eq i32 %24, 0
  br i1 %.not6, label %exit, label %print_caller.lr.ph

print_caller.lr.ph:                               ; preds = %runtime_error
  %invariant.umin = call i32 @llvm.umin.i32(i32 %24, i32 32)
  %25 = getelementptr inbounds nuw i8, ptr %18, i64 16
  %wide.trip.count = zext nneg i32 %invariant.umin to i64
  br label %print_caller

success:                                          ; preds = %run
  %26 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @20, i32 %19)
  br label %common.ret

print_caller:                                     ; preds = %print_caller.lr.ph, %print_caller
  %indvars.iv = phi i64 [ 0, %print_caller.lr.ph ], [ %indvars.iv.next, %print_caller ]
  %27 = getelementptr [8 x i8], ptr %25, i64 %indvars.iv
  %28 = load ptr, ptr %27, align 8
  %fputs5 = call i32 @fputs(ptr %28, ptr %22) #5
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %failure_exit, label %print_caller

failure_exit:                                     ; preds = %print_caller
  %29 = icmp ugt i32 %24, 32
  br i1 %29, label %trace_limit, label %exit

trace_limit:                                      ; preds = %failure_exit
  %30 = call i64 @fwrite(ptr nonnull @19, i64 42, i64 1, ptr %22) #5
  br label %exit

exit:                                             ; preds = %runtime_error, %trace_limit, %failure_exit
  %fputc = call i32 @fputc(i32 10, ptr %22)
  br label %common.ret
}

; Function Attrs: mustprogress nocallback nofree nounwind willreturn
declare i64 @strtoll(ptr readonly, ptr captures(none), i32) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare noundef i32 @puts(ptr noundef readonly captures(none)) local_unnamed_addr #3

declare i32 @vd_runtime_entry_metadata(ptr, i32, ptr, ptr, i64, ptr, i64) local_unnamed_addr

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr noundef readonly captures(none), ...) local_unnamed_addr #3

; Function Attrs: nofree nounwind
declare noundef i32 @fputs(ptr noundef readonly captures(none), ptr noundef captures(none)) local_unnamed_addr #3

; Function Attrs: nofree nounwind
declare noundef i64 @fwrite(ptr noundef readonly captures(none), i64 noundef, i64 noundef, ptr noundef captures(none)) local_unnamed_addr #3

; Function Attrs: nofree nounwind
declare noundef i32 @fputc(i32 noundef, ptr noundef captures(none)) local_unnamed_addr #3

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.umin.i32(i32, i32) #4

attributes #0 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: write) }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress nocallback nofree nounwind willreturn }
attributes #3 = { nofree nounwind }
attributes #4 = { nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }
attributes #5 = { cold }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "VOID source compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "fixed-i32.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f66697865642d6933322e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 5, column: 5, scope: !3)
!8 = !DILocation(line: 8, column: 5, scope: !3)
!9 = !DILocation(line: 10, column: 5, scope: !3)
!10 = !DILocation(line: 0, scope: !3)
!11 = !DILocation(line: 13, column: 20, scope: !3)
!12 = !DILocation(line: 2, column: 9, scope: !3)
!13 = !DILocation(line: 3, column: 9, scope: !3)
!14 = !DILocation(line: 4, column: 9, scope: !3)
!15 = !DILocation(line: 6, column: 9, scope: !3)
!16 = !DILocation(line: 6, column: 11, scope: !3)
!17 = !DILocation(line: 6, column: 19, scope: !3)
!18 = !DILocation(line: 7, column: 9, scope: !3)
!19 = !DILocation(line: 7, column: 11, scope: !3)
!20 = !DILocation(line: 7, column: 19, scope: !3)
!21 = !DILocation(line: 9, column: 9, scope: !3)
!22 = !DILocation(line: 9, column: 13, scope: !3)
!23 = !DILocation(line: 9, column: 18, scope: !3)
!24 = !DILocation(line: 9, column: 19, scope: !3)
!25 = !DILocation(line: 9, column: 23, scope: !3)
!26 = !DILocation(line: 9, column: 29, scope: !3)
!27 = !DILocation(line: 9, column: 30, scope: !3)
!28 = !DILocation(line: 9, column: 27, scope: !3)
!29 = !DILocation(line: 11, column: 33, scope: !3)
!30 = !DILocation(line: 11, column: 47, scope: !3)
!31 = !DILocation(line: 11, column: 57, scope: !3)
!32 = !DILocation(line: 11, column: 51, scope: !3)
!33 = !DILocation(line: 12, column: 12, scope: !3)
!34 = !DILocation(line: 12, column: 15, scope: !3)
!35 = !DILocation(line: 12, column: 19, scope: !3)
