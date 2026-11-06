; ModuleID = '/alias-dependent.void'
source_filename = "/alias-dependent.void"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@__vd_types__void = constant [0 x { ptr, ptr, ptr, i64, ptr, i64, ptr, i32 }] zeroinitializer
@__vd_type_count__void = local_unnamed_addr constant i64 0
@__vd_witnesses__void = constant [0 x { ptr, ptr, ptr, ptr }] zeroinitializer
@__vd_witness_count__void = local_unnamed_addr constant i64 0
@gc.offsets = private constant [6 x i64] [i64 0, i64 8, i64 16, i64 24, i64 32, i64 40]
@gc.offsets.1 = private constant [9 x i64] [i64 0, i64 8, i64 16, i64 24, i64 32, i64 40, i64 48, i64 56, i64 64]
@gc.slot.template = private unnamed_addr constant [2 x { ptr, i64, ptr, i64 }] [{ ptr, i64, ptr, i64 } { ptr null, i64 48, ptr @gc.offsets, i64 6 }, { ptr, i64, ptr, i64 } { ptr null, i64 72, ptr @gc.offsets.1, i64 9 }], align 8
@0 = private unnamed_addr constant [190 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:2:20: runtime error: Array operation failed\0AIn function /alias-dependent.void::entry\0A\00", align 1
@1 = private unnamed_addr constant [230 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:5:11: runtime error: Array operation failed\0A        a[i] = b[i - 1] + 1\0A          ^\0AIn function /alias-dependent.void::entry\0A\00", align 1
@2 = private unnamed_addr constant [233 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:5:11: runtime error: List index outside extent\0A        a[i] = b[i - 1] + 1\0A          ^\0AIn function /alias-dependent.void::entry\0A\00", align 1
@3 = private unnamed_addr constant [189 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:5:9: runtime error: Array operation failed\0AIn function /alias-dependent.void::entry\0A\00", align 1
@4 = private unnamed_addr constant [256 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:5:17: runtime error: Array operation failed during index access\0A        a[i] = b[i - 1] + 1\0A                ^\0AIn function /alias-dependent.void::entry\0A\00", align 1
@5 = private unnamed_addr constant [204 x i8] c"Called from /alias-dependent.void::entry\0A/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:5:11: source call\0A        a[i] = b[i - 1] + 1\0A          ^\0A\00", align 1
@6 = private unnamed_addr constant [250 x i8] c"/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures/alias-dependent.void:7:13: runtime error: Array operation failed during index access\0A        if a[i] != i + 1:\0A            ^\0AIn function /alias-dependent.void::entry\0A\00", align 1
@7 = private unnamed_addr constant [46 x i8] c"VOID: expected one optional decimal i32 input\00", align 1
@stderr = external local_unnamed_addr global ptr
@8 = private unnamed_addr constant [43 x i8] c"\0ASource call trace limited to 32 callers.\0A\00", align 1
@9 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define range(i32 0, 3) i32 @__vd_2f616c6961732d646570656e64656e742e766f69643a3a656e747279__void(i32 %0, ptr %1) !dbg !3 {
block0:
  %2 = alloca { ptr, ptr, ptr, ptr, ptr, ptr }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(48) %2, i8 0, i64 48, i1 false)
  %3 = getelementptr inbounds nuw i8, ptr %2, i64 8
  %4 = getelementptr inbounds nuw i8, ptr %2, i64 16
  %5 = getelementptr inbounds nuw i8, ptr %2, i64 24
  %6 = getelementptr inbounds nuw i8, ptr %2, i64 32
  %7 = getelementptr inbounds nuw i8, ptr %2, i64 40
  %8 = alloca { ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }, align 8
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(72) %8, i8 0, i64 72, i1 false)
  %9 = getelementptr inbounds nuw i8, ptr %8, i64 8
  %10 = getelementptr inbounds nuw i8, ptr %8, i64 16
  %11 = getelementptr inbounds nuw i8, ptr %8, i64 24
  %12 = getelementptr inbounds nuw i8, ptr %8, i64 32
  %13 = getelementptr inbounds nuw i8, ptr %8, i64 40
  %14 = getelementptr inbounds nuw i8, ptr %8, i64 48
  %15 = getelementptr inbounds nuw i8, ptr %8, i64 56
  %16 = getelementptr inbounds nuw i8, ptr %8, i64 64
  %17 = alloca [2 x { ptr, i64, ptr, i64 }], align 8
  call void @llvm.memcpy.p0.p0.i64(ptr noundef nonnull align 8 dereferenceable(64) %17, ptr noundef nonnull align 8 dereferenceable(64) @gc.slot.template, i64 64, i1 false)
  store ptr %2, ptr %17, align 8
  %18 = getelementptr inbounds nuw i8, ptr %17, i64 32
  store ptr %8, ptr %18, align 8
  %19 = alloca [40 x i8], align 8
  call void @vd_gc_frame_enter(ptr nonnull %19, ptr nonnull %17, i64 2)
  call void @vd_gc_poll()
  %20 = call i1 @vd_worker_checkpoint(ptr %1)
  br i1 %20, label %worker.cancelled, label %worker.continue

block1:                                           ; preds = %worker.continue30
  call void @vd_gc_poll(), !dbg !7
  %21 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %21, label %worker.cancelled17, label %worker.continue18, !dbg !7

block2:                                           ; preds = %worker.continue18.preheader, %worker.continue18
  %22 = phi i1 [ %35, %worker.continue18.preheader ], [ %37, %worker.continue18 ]
  %.094115139 = phi i32 [ 1, %worker.continue18.preheader ], [ %36, %worker.continue18 ]
  br i1 %22, label %worker.cancelled19, label %worker.continue20, !dbg !7

block4:                                           ; preds = %worker.continue18
  br i1 %37, label %worker.cancelled31, label %block7, !dbg !7

block7:                                           ; preds = %block4, %worker.continue44
  %indvars.iv = phi i64 [ %indvars.iv.next, %worker.continue44 ], [ 0, %block4 ], !dbg !8
  call void @vd_gc_poll(), !dbg !7
  %23 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %23, label %worker.cancelled37, label %worker.continue38, !dbg !7

block8:                                           ; preds = %worker.continue38
  br i1 %62, label %worker.cancelled39, label %worker.continue40, !dbg !8

block10:                                          ; preds = %worker.continue38
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !9
  %not. = xor i1 %62, true, !dbg !9
  %. = zext i1 %not. to i32, !dbg !9
  br label %common.ret, !dbg !9

block11:                                          ; preds = %numeric_valid42
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !9
  %.57 = select i1 %67, i32 0, i32 2, !dbg !9
  br label %common.ret, !dbg !9

block12:                                          ; preds = %numeric_valid42
  br i1 %67, label %worker.cancelled49, label %worker.continue50, !dbg !10

common.ret:                                       ; preds = %block11, %block10, %worker.cancelled53, %worker.cancelled51, %worker.cancelled49, %worker.cancelled43, %numeric_error41, %worker.cancelled39, %worker.cancelled37, %worker.cancelled33, %worker.cancelled31, %worker.cancelled29, %error_return, %numeric_error27, %numeric_error25, %numeric_error23, %numeric_error21, %worker.cancelled19, %worker.cancelled17, %numeric_error15, %numeric_error13, %numeric_error11, %numeric_error9, %numeric_error7, %numeric_error5, %numeric_error3, %numeric_error1, %numeric_error, %worker.cancelled
  %common.ret.op = phi i32 [ 0, %worker.cancelled ], [ 0, %numeric_error ], [ 0, %numeric_error1 ], [ 0, %numeric_error3 ], [ 0, %numeric_error5 ], [ 0, %numeric_error7 ], [ 0, %numeric_error9 ], [ 0, %numeric_error11 ], [ 0, %numeric_error13 ], [ 0, %numeric_error15 ], [ 0, %worker.cancelled17 ], [ 0, %worker.cancelled19 ], [ 0, %numeric_error21 ], [ 0, %numeric_error23 ], [ 0, %numeric_error25 ], [ 0, %numeric_error27 ], [ 0, %error_return ], [ 0, %worker.cancelled29 ], [ 0, %worker.cancelled31 ], [ 0, %worker.cancelled33 ], [ 0, %worker.cancelled51 ], [ 0, %worker.cancelled37 ], [ 0, %worker.cancelled39 ], [ 0, %numeric_error41 ], [ 0, %worker.cancelled43 ], [ %.57, %block11 ], [ %., %block10 ], [ 0, %worker.cancelled49 ], [ 0, %worker.cancelled53 ]
  ret i32 %common.ret.op, !dbg !9

worker.cancelled:                                 ; preds = %block0
  call void @vd_gc_frame_leave(ptr nonnull %19)
  br label %common.ret

worker.continue:                                  ; preds = %block0
  call void @vd_list_new(ptr nonnull %8, i32 2, i64 8, ptr %1), !dbg !11
  %24 = load ptr, ptr %1, align 8, !dbg !11
  %.not = icmp eq ptr %24, null, !dbg !11
  br i1 %.not, label %numeric_valid, label %numeric_error, !dbg !11

numeric_error:                                    ; preds = %worker.continue
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid:                                    ; preds = %worker.continue
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 0, i64 1, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %25 = load ptr, ptr %1, align 8, !dbg !11
  %.not98 = icmp eq ptr %25, null, !dbg !11
  br i1 %.not98, label %numeric_valid2, label %numeric_error1, !dbg !11

numeric_error1:                                   ; preds = %numeric_valid
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid2:                                   ; preds = %numeric_valid
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 1, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %26 = load ptr, ptr %1, align 8, !dbg !11
  %.not99 = icmp eq ptr %26, null, !dbg !11
  br i1 %.not99, label %numeric_valid4, label %numeric_error3, !dbg !11

numeric_error3:                                   ; preds = %numeric_valid2
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid4:                                   ; preds = %numeric_valid2
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 2, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %27 = load ptr, ptr %1, align 8, !dbg !11
  %.not100 = icmp eq ptr %27, null, !dbg !11
  br i1 %.not100, label %numeric_valid6, label %numeric_error5, !dbg !11

numeric_error5:                                   ; preds = %numeric_valid4
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid6:                                   ; preds = %numeric_valid4
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 3, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %28 = load ptr, ptr %1, align 8, !dbg !11
  %.not101 = icmp eq ptr %28, null, !dbg !11
  br i1 %.not101, label %numeric_valid8, label %numeric_error7, !dbg !11

numeric_error7:                                   ; preds = %numeric_valid6
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid8:                                   ; preds = %numeric_valid6
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 4, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %29 = load ptr, ptr %1, align 8, !dbg !11
  %.not102 = icmp eq ptr %29, null, !dbg !11
  br i1 %.not102, label %numeric_valid10, label %numeric_error9, !dbg !11

numeric_error9:                                   ; preds = %numeric_valid8
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid10:                                  ; preds = %numeric_valid8
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 5, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %30 = load ptr, ptr %1, align 8, !dbg !11
  %.not103 = icmp eq ptr %30, null, !dbg !11
  br i1 %.not103, label %numeric_valid12, label %numeric_error11, !dbg !11

numeric_error11:                                  ; preds = %numeric_valid10
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid12:                                  ; preds = %numeric_valid10
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 6, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %31 = load ptr, ptr %1, align 8, !dbg !11
  %.not104 = icmp eq ptr %31, null, !dbg !11
  br i1 %.not104, label %numeric_valid14, label %numeric_error13, !dbg !11

numeric_error13:                                  ; preds = %numeric_valid12
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid14:                                  ; preds = %numeric_valid12
  call void @vd_list_write(ptr nonnull %8, i32 2, i64 7, i64 99, i64 0, i32 0, ptr nonnull %1), !dbg !11
  %32 = load ptr, ptr %1, align 8, !dbg !11
  %.not105 = icmp eq ptr %32, null, !dbg !11
  br i1 %.not105, label %numeric_valid16, label %numeric_error15, !dbg !11

numeric_error15:                                  ; preds = %numeric_valid14
  store ptr @0, ptr %1, align 8, !dbg !11
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !11
  br label %common.ret, !dbg !11

numeric_valid16:                                  ; preds = %numeric_valid14
  %33 = load ptr, ptr %8, align 8, !dbg !11
  store ptr %33, ptr %2, align 8, !dbg !12
  store ptr %33, ptr %9, align 8, !dbg !13
  store ptr %33, ptr %3, align 8, !dbg !14
  call void @vd_gc_poll(), !dbg !7
  %34 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %34, label %worker.cancelled17, label %worker.continue18.preheader, !dbg !7

worker.continue18.preheader:                      ; preds = %numeric_valid16
  call void @vd_gc_poll(), !dbg !7
  %35 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br label %block2, !dbg !7

worker.cancelled17:                               ; preds = %block1, %numeric_valid16
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !7
  br label %common.ret, !dbg !7

worker.continue18:                                ; preds = %block1
  %36 = add nuw nsw i32 %.094115139, 1, !dbg !7
  %.not106 = icmp eq i32 %36, 8, !dbg !7
  call void @vd_gc_poll(), !dbg !7
  %37 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !7
  br i1 %.not106, label %block4, label %block2, !dbg !7

worker.cancelled19:                               ; preds = %block2
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !7
  br label %common.ret, !dbg !7

worker.continue20:                                ; preds = %block2
  %38 = load ptr, ptr %2, align 8, !dbg !15
  store ptr %38, ptr %10, align 8, !dbg !15
  store ptr %38, ptr %4, align 8, !dbg !15
  store ptr %38, ptr %11, align 8, !dbg !15
  store ptr %38, ptr %5, align 8, !dbg !15
  %39 = call i64 @vd_list_count(ptr nonnull %11, i32 2, ptr %1), !dbg !16
  %40 = load ptr, ptr %1, align 8, !dbg !16
  %.not110 = icmp eq ptr %40, null, !dbg !16
  br i1 %.not110, label %numeric_valid22, label %numeric_error21, !dbg !16

numeric_error21:                                  ; preds = %worker.continue20
  store ptr @1, ptr %1, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !16
  br label %common.ret, !dbg !16

numeric_valid22:                                  ; preds = %worker.continue20
  %41 = zext nneg i32 %.094115139 to i64, !dbg !16
  %.not111 = icmp ugt i64 %39, %41, !dbg !16
  br i1 %.not111, label %numeric_valid24, label %numeric_error23, !dbg !16

numeric_error23:                                  ; preds = %numeric_valid22
  store ptr @2, ptr %1, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !16
  br label %common.ret, !dbg !16

numeric_valid24:                                  ; preds = %numeric_valid22
  %42 = call i64 @vd_list_version(ptr nonnull %11, i32 2, ptr nonnull %1), !dbg !15
  %43 = load ptr, ptr %1, align 8, !dbg !15
  %.not112 = icmp eq ptr %43, null, !dbg !15
  br i1 %.not112, label %numeric_valid26, label %numeric_error25, !dbg !15

numeric_error25:                                  ; preds = %numeric_valid24
  store ptr @3, ptr %1, align 8, !dbg !15
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !15
  br label %common.ret, !dbg !15

numeric_valid26:                                  ; preds = %numeric_valid24
  %44 = load ptr, ptr %3, align 8, !dbg !17
  store ptr %44, ptr %12, align 8, !dbg !17
  store ptr %44, ptr %6, align 8, !dbg !18
  %45 = add nsw i32 %.094115139, -1, !dbg !19
  store ptr %44, ptr %13, align 8, !dbg !18
  %46 = sext i32 %45 to i64, !dbg !18
  %47 = call i64 @vd_list_read_index(ptr nonnull %13, i32 2, i64 %46, i32 1, ptr nonnull %1), !dbg !18
  %48 = load ptr, ptr %1, align 8, !dbg !18
  %.not113 = icmp eq ptr %48, null, !dbg !18
  br i1 %.not113, label %numeric_valid28, label %numeric_error27, !dbg !18

numeric_error27:                                  ; preds = %numeric_valid26
  store ptr @4, ptr %1, align 8, !dbg !18
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !18
  br label %common.ret, !dbg !18

numeric_valid28:                                  ; preds = %numeric_valid26
  %49 = add i64 %47, 1, !dbg !20
  %50 = load ptr, ptr %5, align 8, !dbg !15
  store ptr %50, ptr %14, align 8, !dbg !15
  %51 = and i64 %49, 4294967295, !dbg !16
  call void @vd_list_write(ptr nonnull %14, i32 2, i64 %41, i64 %51, i64 %42, i32 1, ptr nonnull %1), !dbg !16
  %52 = load ptr, ptr %1, align 8, !dbg !16
  %.not114 = icmp eq ptr %52, null, !dbg !16
  br i1 %.not114, label %selected.valid, label %selected.failure, !dbg !16

selected.failure:                                 ; preds = %numeric_valid28
  %53 = getelementptr inbounds nuw i8, ptr %1, i64 8, !dbg !16
  %54 = load i32, ptr %53, align 8, !dbg !16
  %55 = icmp ult i32 %54, 32, !dbg !16
  br i1 %55, label %trace_caller, label %error_return, !dbg !16

selected.valid:                                   ; preds = %numeric_valid28
  call void @vd_gc_poll(), !dbg !16
  %56 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !16
  br i1 %56, label %worker.cancelled29, label %worker.continue30, !dbg !16

trace_caller:                                     ; preds = %selected.failure
  %57 = getelementptr inbounds nuw i8, ptr %1, i64 16, !dbg !16
  %58 = zext nneg i32 %54 to i64, !dbg !16
  %59 = getelementptr [8 x i8], ptr %57, i64 %58, !dbg !16
  store ptr @5, ptr %59, align 8, !dbg !16
  %60 = add nuw nsw i32 %54, 1, !dbg !16
  br label %error_return, !dbg !16

error_return:                                     ; preds = %selected.failure, %trace_caller
  %storemerge = phi i32 [ %60, %trace_caller ], [ 33, %selected.failure ], !dbg !16
  store i32 %storemerge, ptr %53, align 8, !dbg !16
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !16
  br label %common.ret, !dbg !16

worker.cancelled29:                               ; preds = %selected.valid
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !16
  br label %common.ret, !dbg !16

worker.continue30:                                ; preds = %selected.valid
  call void @vd_gc_poll(), !dbg !8
  %61 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !8
  br i1 %61, label %worker.cancelled33, label %block1, !dbg !8

worker.cancelled31:                               ; preds = %block4
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !7
  br label %common.ret, !dbg !7

worker.cancelled33:                               ; preds = %worker.continue30
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !8
  br label %common.ret, !dbg !8

worker.cancelled37:                               ; preds = %block7
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !7
  br label %common.ret, !dbg !7

worker.continue38:                                ; preds = %block7
  %.not107 = icmp eq i64 %indvars.iv, 8, !dbg !8
  call void @vd_gc_poll(), !dbg !8
  %62 = call i1 @vd_worker_checkpoint(ptr %1), !dbg !8
  br i1 %.not107, label %block10, label %block8, !dbg !8

worker.cancelled39:                               ; preds = %block8
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !8
  br label %common.ret, !dbg !8

worker.continue40:                                ; preds = %block8
  %63 = load ptr, ptr %2, align 8, !dbg !21
  store ptr %63, ptr %15, align 8, !dbg !21
  store ptr %63, ptr %7, align 8, !dbg !22
  store ptr %63, ptr %16, align 8, !dbg !22
  %64 = call i64 @vd_list_read_index(ptr nonnull %16, i32 2, i64 %indvars.iv, i32 1, ptr %1), !dbg !22
  %65 = load ptr, ptr %1, align 8, !dbg !22
  %.not108 = icmp eq ptr %65, null, !dbg !22
  br i1 %.not108, label %numeric_valid42, label %numeric_error41, !dbg !22

numeric_error41:                                  ; preds = %worker.continue40
  store ptr @6, ptr %1, align 8, !dbg !22
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !22
  br label %common.ret, !dbg !22

numeric_valid42:                                  ; preds = %worker.continue40
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !23
  %66 = and i64 %64, 4294967295, !dbg !24
  %.not109 = icmp eq i64 %indvars.iv.next, %66, !dbg !24
  call void @vd_gc_poll(), !dbg !9
  %67 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !9
  br i1 %.not109, label %block12, label %block11, !dbg !24

worker.cancelled43:                               ; preds = %worker.continue52
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !24
  br label %common.ret, !dbg !24

worker.continue44:                                ; preds = %worker.continue52
  call void @vd_gc_poll(), !dbg !10
  %68 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !10
  br i1 %68, label %worker.cancelled53, label %block7, !dbg !10

worker.cancelled49:                               ; preds = %block12
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !10
  br label %common.ret, !dbg !10

worker.continue50:                                ; preds = %block12
  call void @vd_gc_poll(), !dbg !10
  %69 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !10
  br i1 %69, label %worker.cancelled51, label %worker.continue52, !dbg !10

worker.cancelled51:                               ; preds = %worker.continue50
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !10
  br label %common.ret, !dbg !10

worker.continue52:                                ; preds = %worker.continue50
  call void @vd_gc_poll(), !dbg !24
  %70 = call i1 @vd_worker_checkpoint(ptr nonnull %1), !dbg !24
  br i1 %70, label %worker.cancelled43, label %worker.continue44, !dbg !24

worker.cancelled53:                               ; preds = %worker.continue44
  call void @vd_gc_frame_leave(ptr nonnull %19), !dbg !10
  br label %common.ret, !dbg !10
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #1

declare void @vd_gc_frame_enter(ptr, ptr, i64) local_unnamed_addr

declare void @vd_gc_poll() local_unnamed_addr

declare i1 @vd_worker_checkpoint(ptr) local_unnamed_addr

declare void @vd_gc_frame_leave(ptr) local_unnamed_addr

declare void @vd_list_new(ptr, i32, i64, ptr) local_unnamed_addr

declare void @vd_list_write(ptr, i32, i64, i64, i64, i32, ptr) local_unnamed_addr

declare i64 @vd_list_count(ptr, i32, ptr) local_unnamed_addr

declare i64 @vd_list_version(ptr, i32, ptr) local_unnamed_addr

declare i64 @vd_list_read_index(ptr, i32, i64, i32, ptr) local_unnamed_addr

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
  %19 = call i32 @vd_runtime_entry_metadata(ptr nonnull @__vd_2f616c6961732d646570656e64656e742e766f69643a3a656e747279__void, i32 %17, ptr nonnull %18, ptr nonnull @__vd_types__void, i64 0, ptr nonnull @__vd_witnesses__void, i64 0)
  %20 = load ptr, ptr %18, align 8
  %.not = icmp eq ptr %20, null
  br i1 %.not, label %success, label %runtime_error

common.ret:                                       ; preds = %exit, %success, %invalid_input
  %common.ret.op = phi i32 [ 2, %invalid_input ], [ 0, %success ], [ 1, %exit ]
  ret i32 %common.ret.op

invalid_input:                                    ; preds = %parse_input
  %21 = tail call i32 @puts(ptr nonnull dereferenceable(1) @7)
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
  %26 = call i32 (ptr, ...) @printf(ptr nonnull dereferenceable(1) @9, i32 %19)
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
  %30 = call i64 @fwrite(ptr nonnull @8, i64 42, i64 1, ptr %22) #5
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
!1 = !DIFile(filename: "alias-dependent.void", directory: "/home/shmellyorc/.local/state/void-log-performance/simd-aot-only-glgoup_7/fixtures")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "entry", linkageName: "__vd_2f616c6961732d646570656e64656e742e766f69643a3a656e747279__void", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "i32", size: 32, encoding: DW_ATE_signed)
!7 = !DILocation(line: 4, column: 5, scope: !3)
!8 = !DILocation(line: 6, column: 5, scope: !3)
!9 = !DILocation(line: 0, scope: !3)
!10 = !DILocation(line: 8, column: 20, scope: !3)
!11 = !DILocation(line: 2, column: 20, scope: !3)
!12 = !DILocation(line: 2, column: 9, scope: !3)
!13 = !DILocation(line: 3, column: 13, scope: !3)
!14 = !DILocation(line: 3, column: 9, scope: !3)
!15 = !DILocation(line: 5, column: 9, scope: !3)
!16 = !DILocation(line: 5, column: 11, scope: !3)
!17 = !DILocation(line: 5, column: 16, scope: !3)
!18 = !DILocation(line: 5, column: 17, scope: !3)
!19 = !DILocation(line: 5, column: 20, scope: !3)
!20 = !DILocation(line: 5, column: 25, scope: !3)
!21 = !DILocation(line: 7, column: 12, scope: !3)
!22 = !DILocation(line: 7, column: 13, scope: !3)
!23 = !DILocation(line: 7, column: 22, scope: !3)
!24 = !DILocation(line: 7, column: 17, scope: !3)
