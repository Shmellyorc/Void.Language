"""Development source provenance: expand concrete sync/async I/O boundaries.
Shared source helpers own policies; this is not an execution interpreter.
Run after editing the body template below to replace generated source sections.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = '''
# BEGIN concrete $MODE file operations
private $ASYNCfunc _member$SUFFIX(value: string, bound: i64, authorized: bool) -> result<bool, failure>:
    var observation = $AWAITio.metadata$SUFFIX(value, false)
    if observation is Err(var cause):
        if (cause.kind is io.error_kind.not_found):
            return Ok(false)
        var receipt = _receipt()
        receipt.file_error(cause, failure_kind.file_io)
        return Err(receipt.problem())
    if not authorized:
        var receipt = _receipt()
        receipt.file_error(io.error(kind = io.error_kind.already_exists, operation = "validate_file", path = value, message = "Existing managed filename requires explicit adoption"), failure_kind.file_io)
        return Err(receipt.problem())
    if observation is Ok(var member):
        if not (member.kind is io.entry_kind.file) or member.size < 0 or member.size > bound:
            var receipt = _receipt()
            receipt.file_error(_layout_error(value, "Managed member must be a regular file within its byte bound"), failure_kind.file_io)
            return Err(receipt.problem())
        if member.size > 0:
            with _temporary_file() as temporary:
                var acquisition = $AWAITio.open$SUFFIX(value, io.open_mode.read)
                if acquisition is Err(var cause):
                    var receipt = _receipt()
                    receipt.file_error(cause, failure_kind.file_io)
                    return Err(receipt.problem())
                if acquisition is Ok(var resource):
                    temporary.resource = resource
                    var receipt = _receipt()
                    var seek = $AWAITresource.seek$SUFFIX(-1, io.seek_origin.end)
                    if seek is Err(var cause):
                        receipt.file_error(cause, failure_kind.file_io)
                    else:
                        var tail = $AWAITresource.read_exact$SUFFIX(1)
                        if tail is Err(var cause):
                            receipt.file_error(cause, failure_kind.file_io)
                        if tail is Ok(var bytes):
                            if bytes[0] != (10 as u8):
                                receipt.file_error(_layout_error(value, "Managed nonempty file must end with LF; explicit repair is required"), failure_kind.file_io)
                    var close = $AWAITresource.close$SUFFIX()
                    if close is Err(var cause):
                        receipt.file_error(cause, failure_kind.cleanup_io, true)
                        if not (cause.kind is io.error_kind.busy):
                            temporary.resource = null
                    else:
                        temporary.resource = null
                    if receipt.operation != "":
                        return Err(receipt.problem())
    return Ok(true)

private $ASYNCfunc (self logger) _preflight$SUFFIX(options: config, namespace: string) -> result<bool, failure>:
    var count = options.backups
    if namespace == self._namespace and self._owned_count > count:
        count = self._owned_count
    var present = false
    for index in 0..count:
        var retained = namespace == self._namespace and index <= self._owned_count
        var authorized = retained or (options.existing is existing_files.adopt)
        var found = $AWAIT_member$SUFFIX(_slot(namespace, index), options.rotation_bytes, authorized)?
        if index == 0:
            present = found
    return Ok(present)

private $ASYNCfunc (self logger) _acquire$SUFFIX(options: config, namespace: string, present: bool) -> result<bool, failure>:
    self._namespace = namespace
    self._owned_count = options.backups
    var mode = io.open_mode.create_new
    if present:
        mode = io.open_mode.append
    var acquisition = $AWAITio.open$SUFFIX(namespace, mode)
    if acquisition is Err(var cause):
        var receipt = _receipt()
        receipt.file_error(cause, failure_kind.file_io)
        return Err(receipt.problem())
    if acquisition is Ok(var resource):
        self._file = resource
        var length = $AWAITresource.length$SUFFIX()
        if length is Err(var cause):
            var receipt = _receipt()
            receipt.file_error(cause, failure_kind.file_io)
            return Err(receipt.problem())
        if length is Ok(var size):
            self._length = size
    return Ok(true)

private $ASYNCfunc (self logger) _retire$SUFFIX(receipt: _receipt, kind: failure_kind):
    if self._file is Ok(var resource):
        if $AWAITresource.flush$SUFFIX() is Err(var cause):
            receipt.file_error(cause, kind)
        var close = $AWAITresource.close$SUFFIX()
        if close is Err(var cause):
            receipt.file_error(cause, kind, true)
            if not (cause.kind is io.error_kind.busy):
                self._file = null
        else:
            self._file = null

private $ASYNCfunc (self logger) _rotate$SUFFIX(receipt: _receipt) -> bool:
    $AWAITself._retire$SUFFIX(receipt, failure_kind.rotation_io)
    if receipt.file_cause != null or receipt.cleanup_cause != null:
        return false
    var count = self._options.backups
    var oldest = _slot(self._namespace, count)
    if $AWAITio.remove_file$SUFFIX(oldest, true) is Err(var cause):
        receipt.file_error(cause, failure_kind.rotation_io)
        return false
    if count > 0:
        # Missing sources are permitted gaps; every destination remains no-replace.
        for index in (count - 1)..0:
            var source = _slot(self._namespace, index)
            var target = _slot(self._namespace, index + 1)
            var move = $AWAITio.move_file$SUFFIX(source, target, false)
            if move is Err(var cause):
                if not (cause.kind is io.error_kind.not_found):
                    receipt.file_error(cause, failure_kind.rotation_io)
                    return false
    var acquisition = $AWAITself._acquire$SUFFIX(self._options, self._namespace, false)
    if acquisition is Err(var cause):
        if cause.file_cause is Ok(var original):
            receipt.file_error(original, failure_kind.rotation_io)
        return false
    return true

private $ASYNCfunc (self logger) _deliver$SUFFIX(bytes: [u8], guard: _guard) -> result<bool, failure>:
    var receipt = _receipt()
    guard.uncertain = true
    self._console_record(bytes, receipt)
    if self._options.file_path != "":
        receipt.file_attempted = true
        var ready = true
        if bytes.count > self._options.rotation_bytes - self._length:
            ready = $AWAITself._rotate$SUFFIX(receipt)
        if ready:
            if self._file is Ok(var resource):
                var outcome = $AWAITresource.write_bytes$SUFFIX(bytes)
                if outcome is Err(var cause):
                    receipt.file_bytes = cause.bytes_transferred
                    receipt.file_error(cause, failure_kind.file_io)
                if outcome is Ok(var count):
                    receipt.file_bytes = count
                    receipt.file_complete = true
                    self._length += count
            else:
                receipt.file_error(io.error(kind = io.error_kind.closed, operation = "write_bytes", path = self._namespace, message = "No owned active file"), failure_kind.file_io)
    if receipt.operation != "":
        self._fault = true
        return Err(receipt.problem())
    guard.uncertain = false
    return Ok(true)

## Acquire an independent managed logger; creates only an explicitly configured file.
## @param options Copied settings; default is info with UTC and stderr only.
## @return Owned logger, or typed validation/acquisition failure.
$ASYNCfunc open$SUFFIX(options: config = config()) -> result<logger, failure>:
    var validated = _validate(options)?
    var instance = logger(options)
    with instance._reserve("open")? as guard:
        instance._enter(guard)
        guard.release_file = true
        if options.file_path != "":
            var resolved = path.absolute(options.file_path)
            if resolved is Err(var cause):
                var receipt = _receipt()
                receipt.file_error(cause, failure_kind.file_io)
                return Err(receipt.problem())
            if resolved is Ok(var namespace):
                var present = $AWAITinstance._preflight$SUFFIX(options, namespace)?
                guard.uncertain = true
                var acquired = $AWAITinstance._acquire$SUFFIX(options, namespace, present)?
        guard.release_file = false
        guard.uncertain = false
        return Ok(instance)

## Replace copied settings and explicitly recover a faulted instance.
## Existing owned names retain authority. Filesystem changes are not transactional.
## @param options Complete replacement settings; validation precedes mutation.
## @return True after acquisition and publication, or typed primary/cleanup causes.
$ASYNCfunc (self logger) configure$SUFFIX(options: config) -> result<bool, failure>:
    var validated = _validate(options)?
    with self._reserve("configure")? as guard:
        self._enter(guard)
        var namespace = ""
        if options.file_path != "":
            var resolved = path.absolute(options.file_path)
            if resolved is Err(var cause):
                var receipt = _receipt()
                receipt.file_error(cause, failure_kind.file_io)
                return Err(receipt.problem())
            if resolved is Ok(var value):
                namespace = value
        var same_policy = (namespace == self._namespace and options.backups == self._options.backups and
            options.rotation_bytes == self._options.rotation_bytes and ((options.existing is existing_files.adopt) == (self._options.existing is existing_files.adopt)))
        if not self._fault and same_policy:
            self._options = options
            return Ok(true)
        # Preflight the entire replacement/retained set before closing or deleting anything.
        var present = false
        if namespace != "":
            present = $AWAITself._preflight$SUFFIX(options, namespace)?
        guard.uncertain = true
        guard.release_file = true
        var receipt = _receipt()
        self._console_flush(receipt)
        $AWAITself._retire$SUFFIX(receipt, failure_kind.cleanup_io)
        if receipt.operation != "":
            self._fault = true
            guard.release_file = false
            return Err(receipt.problem())
        if namespace == self._namespace and namespace != "" and options.backups < self._owned_count:
            for index in (options.backups + 1)..self._owned_count:
                if $AWAITio.remove_file$SUFFIX(_slot(namespace, index), true) is Err(var cause):
                    receipt.file_error(cause, failure_kind.file_io)
                    return Err(receipt.problem())
        if namespace != "":
            var acquired = $AWAITself._acquire$SUFFIX(options, namespace, present)?
        self._options = options
        self._fault = false
        guard.release_file = false
        guard.uncertain = false
        return Ok(true)

## Emit an eager message; file operations use this call's explicit execution mode.
## @param severity Record severity; off is an invalid record level.
## @param message Already evaluated text, escaped into one bounded UTF-8 line.
## @return True for complete delivery, false when filtered, or typed failure/progress.
$ASYNCfunc (self logger) write$SUFFIX(severity: level, message: string) -> result<bool, failure>:
    if not self._admit(severity)?:
        return Ok(false)
    with self._reserve("write")? as guard:
        self._enter(guard)
        var bytes = _prepare(self._options, severity, message)?
        return $AWAITself._deliver$SUFFIX(bytes, guard)

## Drain console output and synchronize the owned file.
## @return True when both sinks acknowledge flushing, or original typed failures.
$ASYNCfunc (self logger) flush$SUFFIX() -> result<bool, failure>:
    with self._reserve("flush")? as guard:
        self._enter(guard)
        if self._fault:
            return Err(_reason(failure_kind.faulted, "flush", "Explicit recovery configuration is required"))
        var receipt = _receipt()
        guard.uncertain = true
        self._console_flush(receipt)
        if self._file is Ok(var resource):
            if $AWAITresource.flush$SUFFIX() is Err(var cause):
                receipt.file_error(cause, failure_kind.cleanup_io)
        if receipt.operation != "":
            return Err(receipt.problem())
        guard.uncertain = false
        return Ok(true)

## Flush and close owned resources, including after a recoverable flush failure.
## Busy preserves ownership; a native close failure retires its handle. Repeated close succeeds.
## @return True once closed, or original primary and cleanup failures.
$ASYNCfunc (self logger) close$SUFFIX() -> result<bool, failure>:
    with self._reserve("close", true)? as guard:
        self._enter(guard)
        if self._closed and self._file == null:
            return Ok(true)
        var receipt = _receipt()
        guard.uncertain = true
        guard.release_file = true
        self._console_flush(receipt)
        $AWAITself._retire$SUFFIX(receipt, failure_kind.cleanup_io)
        guard.release_file = false
        if self._file == null:
            self._closed = true
        if receipt.operation != "":
            return Err(receipt.problem())
        guard.uncertain = false
        return Ok(true)
# END concrete $MODE file operations
'''
path = ROOT / 'stdlib/log.void'
text = path.read_text().split('# BEGIN concrete sync file operations')[0]
for asynchronous in (False, True):
    substitutions = {'$MODE': 'async' if asynchronous else 'sync', '$ASYNC': 'async ' if asynchronous else '', '$SUFFIX': '_async' if asynchronous else '', '$AWAIT': 'await ' if asynchronous else ''}
    section = TEMPLATE
    for key, value in substitutions.items():
        section = section.replace(key, value)
    text += section
path.write_text(text)
with path.open('a') as output:
    output.write('''
## Build an admitted message exactly once, with no native sink lock held.
## @param severity Record severity; filtering occurs before invoking the builder.
## @param make_message Temporary synchronous builder; never saved in the logger.
## @return True on complete delivery, false when filtered, or typed failure.
func (self logger) write_lazy(severity: level, make_message: func() -> string) -> result<bool, failure>:
    if not self._admit(severity)?:
        return Ok(false)
    with self._reserve("write_lazy")? as guard:
        self._enter(guard)
        var message = make_message()
        var bytes = _prepare(self._options, severity, message)?
        return self._deliver(bytes, guard)

## Terminal synchronous deterministic cleanup. GC does not invoke this method.
## Inspect explicit close for typed diagnostics; cleanup escalates through convert.unwrap.
func (self logger) deinit():
    self._terminal = true
    convert.unwrap(self.close())

## Filter the application Runtime's default logger without reserving output.
## @param severity Record severity; off returns false.
## @return Whether the configured threshold admits this severity.
func enabled(severity: level) -> bool:
    return _default.enabled(severity)

## Build a default-logger message only after admission.
## @param severity Record severity.
## @param make_message Temporary synchronous builder; zero calls when filtered.
## @return Complete delivery, filtering, or typed failure.
func write_lazy(severity: level, make_message: func() -> string) -> result<bool, failure>:
    return _default.write_lazy(severity, make_message)
''')
    for severity in ('trace', 'debug', 'info', 'warn', 'error'):
        for receiver in (True, False):
            declaration = '(self logger) ' if receiver else ''
            target = 'self' if receiver else '_default'
            output.write(f'''
## Emit an eager {severity} record using the canonical checked writer.
## @param message Already evaluated text; ordinary formatting occurs in the caller.
## @return True for delivery, false for filtering, or typed failure/progress.
func {declaration}{severity}(message: string) -> result<bool, failure>:
    return {target}.write(level.{severity}, message)
''')
    for asynchronous in (False, True):
        prefix, suffix, awaiting = ('async ', '_async', 'await ') if asynchronous else ('', '', '')
        for name, params, args, docs in (
            ('write', 'severity: level, message: string', 'severity, message', '## @param severity Record severity; off is invalid.\n## @param message Already evaluated text.\n'),
            ('flush', '', '', ''),
            ('shutdown', '', '', ''),
        ):
            method = 'close' if name == 'shutdown' else name
            output.write(f'''
## Checked {name} on the application Runtime's default logger.
{docs}## @return Complete success, filtering for writes, or typed failure/progress.
{prefix}func {name}{suffix}({params}) -> result<bool, failure>:
    return {awaiting}_default.{method}{suffix}({args})
''')
        output.write(f'''
## Explicitly configure or replace a shut-down application default.
## @param options Complete copied settings; file ownership requires an exclusive trusted namespace.
## @return True after publication, or typed validation/lifecycle/acquisition failure.
{prefix}func configure{suffix}(options: config) -> result<bool, failure>:
    if _default._is_closed():
        var validated = _validate(options)?
        if _default._is_busy():
            return Err(_reason(failure_kind.busy, "configure", "Logger is busy"))
        var closed = {awaiting}_default.close{suffix}()?
        var replacement = {awaiting}open{suffix}(options)?
        _default = replacement
        return Ok(true)
    return {awaiting}_default.configure{suffix}(options)
''')
