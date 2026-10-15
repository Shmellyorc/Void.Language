"""Explicit storage investigation only. No production calling convention change."""


def ir(target, layout, count, caller):
    value = f'[{count} x i32]'
    prefix = f'target triple = "{target}"\ntarget datalayout = "{layout}"\n'
    prefix += 'declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)\n'
    if not caller:
        return prefix + f'''declare void @touch(ptr, i64)
define void @roundtrip(ptr %result, ptr %snapshot, ptr %failure) "warn-stack-size"="1" {{
 %home = alloca {value}, align 4
 call void @llvm.memcpy.p0.p0.i64(ptr align 4 %home, ptr align 4 %snapshot, i64 {count * 4}, i1 false)
 call void @touch(ptr %home, i64 {count})
 call void @llvm.memcpy.p0.p0.i64(ptr align 4 %result, ptr align 4 %home, i64 {count * 4}, i1 false)
 ret void
}}
'''
    return prefix + f'''declare void @roundtrip(ptr, ptr, ptr)
declare void @fill(ptr, i64, i32)
declare i32 @check(ptr, i64, i32)
define i32 @entry(i32 %seed, ptr %failure) "warn-stack-size"="1" {{
 %input = alloca {value}, align 4
 %snapshot = alloca {value}, align 4
 %output = alloca {value}, align 4
 call void @fill(ptr %input, i64 {count}, i32 %seed)
 call void @llvm.memcpy.p0.p0.i64(ptr align 4 %snapshot, ptr align 4 %input, i64 {count * 4}, i1 false)
 call void @roundtrip(ptr %output, ptr %snapshot, ptr %failure)
 %checked = call i32 @check(ptr %output, i64 {count}, i32 %seed)
 ret i32 %checked
}}
'''


def semantics(target, layout, caller):
    """Later-argument owner writes/failure and callee failure, with a real channel.

    The private probe Failure matches the existing message/count/32-frame shape;
    it does not reuse runtime code or claim source-diagnostic/GC verification.
    """
    prefix = f'target triple = "{target}"\ntarget datalayout = "{layout}"\n'
    prefix += '%Failure = type {ptr, i32, [32 x ptr]}\n'
    prefix += 'declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)\n'
    if not caller:
        return prefix + '''@message = private constant [21 x i8] c"probe callee failure\\00"
declare void @touch(ptr, i64)
define void @checked_roundtrip(ptr %result, ptr %snapshot, i32 %later, i32 %mode, ptr %failure) "warn-stack-size"="1" {
 %home = alloca [16 x i32], align 4
 call void @llvm.memcpy.p0.p0.i64(ptr align 4 %home, ptr align 4 %snapshot, i64 64, i1 false)
 call void @touch(ptr %home, i64 16)
 %wrong_order = icmp ne i32 %later, 99
 %requested_failure = icmp eq i32 %mode, 1
 %bad = or i1 %wrong_order, %requested_failure
 br i1 %bad, label %fail, label %success
fail:
 store i32 -1, ptr %result
 %message_home = getelementptr %Failure, ptr %failure, i32 0, i32 0
 store ptr @message, ptr %message_home
 ret void
success:
 call void @llvm.memcpy.p0.p0.i64(ptr align 4 %result, ptr align 4 %home, i64 64, i1 false)
 ret void
}
'''
    return prefix + '''@trace = private constant [13 x i8] c"probe caller\\00"
declare void @checked_roundtrip(ptr, ptr, i32, i32, ptr)
declare void @fill(ptr, i64, i32)
declare i32 @later(ptr, i64, i32, ptr)
declare i32 @check(ptr, i64, i32)
declare i32 @plain(ptr, i64, i32)
declare void @record(i32)
define i32 @snapshot_case(i32 %seed, i32 %mode, ptr %failure) "warn-stack-size"="1" {
 %input = alloca [16 x i32], align 4
 %snapshot = alloca [16 x i32], align 4
 %output = alloca [16 x i32], align 4
 call void @fill(ptr %input, i64 16, i32 %seed)
 call void @llvm.memcpy.p0.p0.i64(ptr align 4 %snapshot, ptr align 4 %input, i64 64, i1 false)
 call void @record(i32 1)
 %next = call i32 @later(ptr %input, i64 16, i32 %mode, ptr %failure)
 %message_home = getelementptr %Failure, ptr %failure, i32 0, i32 0
 %first_message = load ptr, ptr %message_home
 %first_failed = icmp ne ptr %first_message, null
 br i1 %first_failed, label %propagate, label %invoke
invoke:
 call void @checked_roundtrip(ptr %output, ptr %snapshot, i32 %next, i32 %mode, ptr %failure)
 %second_message = load ptr, ptr %message_home
 %second_failed = icmp ne ptr %second_message, null
 br i1 %second_failed, label %propagate, label %consume
propagate:
 %count_home = getelementptr %Failure, ptr %failure, i32 0, i32 1
 %count = load i32, ptr %count_home
 %room = icmp ult i32 %count, 32
 br i1 %room, label %append, label %limit
append:
 %frame = getelementptr %Failure, ptr %failure, i32 0, i32 2, i32 %count
 store ptr @trace, ptr %frame
 %next_count = add i32 %count, 1
 store i32 %next_count, ptr %count_home
 ret i32 0
limit:
 store i32 33, ptr %count_home
 ret i32 0
consume:
 %result_ok = call i32 @check(ptr %output, i64 16, i32 %seed)
 %owner_ok = call i32 @plain(ptr %input, i64 16, i32 99)
 %snapshot_ok = call i32 @plain(ptr %snapshot, i64 16, i32 %seed)
 %a = or i32 %result_ok, %owner_ok
 %all = or i32 %a, %snapshot_ok
 ret i32 %all
}
'''
