# G5 owns the closure and generation; invocation I/O exists only on its callback thread.
struct callback_registration:
    owner: native.callback_registration

private func _callback_read(which: u32) -> [u8]:
    var length = native.callback_input_size(which) as i64
    _check()
    var bytes: [u8] = []
    for index in 0..<length:
        bytes.push(0)
    var at: i64 = 0
    for at < length:
        var count = length - at
        if count > 65536:
            count = 65536
        native.callback_read(which, at as u64, bytes, at as u64, count as u64)
        _check()
        at += count
    return bytes

private func _callback_name() -> string:
    return conversion.unwrap(string.from_utf8(_callback_read(0)))

private func _callback_write(const bytes: [u8]):
    native.callback_output_size(bytes.count as u64)
    _check()
    var at: i64 = 0
    for at < bytes.count:
        var count = bytes.count - at
        if count > 65536:
            count = 65536
        native.callback_write(at as u64, bytes, at as u64, count as u64)
        _check()
        at += count

private func _callback_register(target: native.callback_target, handler: func(u32, i32) -> i32) -> callback_registration:
    native.callback_activate(target)
    _check()
    var acquired = native.callback_register(func(frames: u32, level: i32) -> i32:
        return handler(frames, level)
    )
    _check()
    _require(acquired != null)
    return callback_registration(owner = acquired as native.callback_registration)

func callback_close(value: callback_registration) -> i32:
    return native.callback_close(value.owner)

func callback_count(value: callback_registration) -> u64:
    var count = native.callback_count(value.owner)
    _check()
    return count

func callback_active(value: callback_registration) -> bool:
    var active = native.callback_active(value.owner)
    _check()
    return active

func trace_log(level: i32, message: string):
    var text = _string_buffer(message)
    defer native.buffer_close(text)
    native.trace_log(level, text)
    _check()

# Same supported types/hooks as the builtin formatter. Multiple values use ordinary
# VOID interpolation before this call. This is deliberately not C printf varargs.
func text_format<t>(value: t, spec: string = "") -> result<string, string>:
    return format(value, spec)

func set_trace_log_callback(handler: func(i32, string)) -> callback_registration:
    var target = native.callback_target_new(1) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        handler(level, _callback_name())
        return 1
    )

func set_load_file_data_callback(handler: func(string) -> result<[u8], string>) -> callback_registration:
    var target = native.callback_target_new(2) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        var outcome = handler(_callback_name())
        if outcome is Ok(var bytes):
            _callback_write(bytes)
            return 1
        return 2
    )

func set_save_file_data_callback(handler: func(string, [u8]) -> bool) -> callback_registration:
    var target = native.callback_target_new(3) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        if handler(_callback_name(), _callback_read(1)):
            return 1
        return 2
    )

func set_load_file_text_callback(handler: func(string) -> result<string, string>) -> callback_registration:
    var target = native.callback_target_new(4) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        var outcome = handler(_callback_name())
        if outcome is Ok(var text):
            var bytes = text.to_utf8()
            for byte in bytes:
                _require(byte != 0)
            _callback_write(bytes)
            return 1
        return 2
    )

func set_save_file_text_callback(handler: func(string, string) -> bool) -> callback_registration:
    var target = native.callback_target_new(5) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        if handler(_callback_name(), conversion.unwrap(string.from_utf8(_callback_read(1)))):
            return 1
        return 2
    )

# PCM generation uses the stream's native format. Processors always receive
# frames * 2 float32 samples, as specified by the upstream public declarations.
# Byte representations use the qualified x64 little-endian native format.
func set_audio_stream_callback(value: audio_stream, handler: func(u32, u32, u32) -> [u8]) -> callback_registration:
    var channels = value.channels
    var sample_size = value.sample_size
    var target = native.callback_target_stream(value.owner, 6) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        var bytes = handler(frames, channels, sample_size)
        _require(bytes.count as u64 == native.callback_input_size(1))
        _callback_write(bytes)
        return 1
    )

func attach_audio_stream_processor(value: audio_stream, handler: func([u8], u32) -> [u8]) -> callback_registration:
    var target = native.callback_target_stream(value.owner, 7) as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        var bytes = handler(_callback_read(1), frames)
        _require(bytes.count as u64 == native.callback_input_size(1))
        _callback_write(bytes)
        return 1
    )

func detach_audio_stream_processor(value: callback_registration) -> i32:
    _require(native.callback_kind(value.owner) == 7)
    return callback_close(value)

func attach_audio_mixed_processor(handler: func([u8], u32) -> [u8]) -> callback_registration:
    var target = native.callback_target_mixed() as native.callback_target
    defer native.callback_target_close(target)
    return _callback_register(target, func(frames: u32, level: i32) -> i32:
        var bytes = handler(_callback_read(1), frames)
        _require(bytes.count as u64 == native.callback_input_size(1))
        _callback_write(bytes)
        return 1
    )

func detach_audio_mixed_processor(value: callback_registration) -> i32:
    _require(native.callback_kind(value.owner) == 8)
    return callback_close(value)
