"""Failure evidence only; never choose an install path or decide acceptance."""
import errno
import json
import os
from pathlib import Path
import stat

MARKER = 'VOIDUP_DIRECTORY_DIAGNOSTIC '
HOME_LEAF = "first user 雪 ' $() # %"


def metadata(path, *, fd=None):
    try:
        value = os.stat(path, dir_fd=fd, follow_symlinks=False)
        kind = 'symlink' if stat.S_ISLNK(value.st_mode) else 'directory' if stat.S_ISDIR(value.st_mode) else 'file' if stat.S_ISREG(value.st_mode) else 'other'
        return dict(errno=0, device=value.st_dev, inode=value.st_ino, uid=value.st_uid, mode=value.st_mode, type=kind)
    except OSError as error:
        return dict(errno=error.errno)


def system_var():
    result = metadata('/var')
    if result.get('type') == 'symlink':
        try:
            target = os.readlink('/var')
            result['link_target'] = target if target == '/private/var' else '<redacted non-system target>'
        except OSError as error:
            result['readlink_errno'] = error.errno
    return result


def begin(home):
    return dict(home=str(home), system_var_before=system_var())


def admitted_capture(requested, home):
    # Evidence privacy scope only; this never admits a Runtime directory.
    return (isinstance(requested, str) and len(requested) <= 4096 and
            Path(home).name == HOME_LEAF and Path(home).parent.name.startswith('voidup-first-run-') and
            requested.startswith('/'))


def physical(path, home):
    try:
        value = os.path.realpath(path, strict=True)
        bases = [home] + (['/private' + home] if home.startswith('/var/') else [])
        return dict(errno=0, path=value if any(value == b or value.startswith(b + '/') for b in bases) else '<outside fixture scope>')
    except OSError as error:
        return dict(errno=error.errno)


def inspect(requested):
    """Read-only replay from the captured request, not an original syscall trace."""
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | os.O_NOFOLLOW
    events = []
    current = os.open('/', flags)
    prefix = ''
    try:
        for name in requested[1:].split('/'):
            prefix += '/' + name
            event = dict(path=prefix, component=name, before=metadata(name, fd=current))
            if event['before'].get('type') == 'symlink':
                try:
                    target = os.readlink(name, dir_fd=current)
                    event['link_target'] = target if prefix == '/var' and target == '/private/var' else '<redacted non-system target>'
                except OSError as error:
                    event['readlink_errno'] = error.errno
            try:
                child = os.open(name, flags, dir_fd=current)
            except OSError as error:
                event.update(operation='openat', errno=error.errno, after=metadata(name, fd=current))
                events.append(event)
                break
            event.update(operation='openat', errno=0)
            events.append(event)
            os.close(current)
            current = child
    finally:
        os.close(current)
    return events


def classify(record, baseline, after):
    before, current = record['before'], record['after']
    keys = ('errno', 'device', 'inode', 'type', 'uid', 'mode')
    identity = lambda item: tuple(item.get(key) for key in keys)
    if identity(before) != identity(current):
        return 'change_observed_across_failed_syscall'
    if before.get('errno') == errno.ENOENT:
        return 'missing_component_observed'
    if before.get('type') == 'file':
        return 'non_directory_component_observed'
    if before.get('type') == 'symlink':
        if record['component'] == 'var' and record['physical_parent'] == '/' and any(
                identity(value) != identity(before) for value in (baseline, after)):
            return 'var_metadata_changed_between_fixture_observations'
        if (record['component'] == 'var' and record['physical_parent'] == '/' and
                record.get('link_target') == '/private/var' and before.get('uid') == 0 and
                all(identity(value) == identity(before) and value.get('link_target') == '/private/var'
                    for value in (baseline, after))):
            return 'standard_var_alias_observed_without_identity_change'
        return 'non_system_symlink_observed'
    return 'unclassified_native_failure'


def retain(stderr, context, evidence):
    """Best effort: preserve the caller's original result and assertion."""
    destination = Path(evidence) / 'native-directory-failure.json'
    report = dict(schema=1, status='runtime_capture_absent', captures=[])
    try:
        lines = [line[len(MARKER):] for line in stderr.decode(errors='replace').splitlines() if line.startswith(MARKER)]
        for line in lines[:16]:
            record = json.loads(line)
            if record.get('schema') != 1 or not admitted_capture(record.get('requested'), context['home']):
                report['status'] = 'capture_rejected_by_privacy_scope'
                continue
            entry = dict(runtime=record)
            report['captures'].append(entry)
            report['status'] = 'actual_runtime_failure_captured'
            after = system_var()
            replayable = record['requested'].startswith(context['home'] + '/') and all(
                part not in ('', '.', '..') for part in record['requested'][1:].split('/'))
            entry.update(dict(classification=classify(record, context['system_var_before'], after),
                system_var_before=context['system_var_before'], system_var_after=after,
                requested_physical=physical(record['requested'], context['home']) if replayable else dict(status='outside_fixture_replay_scope'),
                home_physical=physical(context['home'], context['home']),
                replay=inspect(record['requested']) if replayable else [],
                limitation='Metadata and read-only replay are separate observations; absence of an observed change is not proof against every race.'))
        destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    except Exception as error:
        try:
            report['collection_error'] = dict(error_type=type(error).__name__, errno=getattr(error, 'errno', None))
            if not report['captures']:report['status'] = 'diagnostic_collection_failed'
            destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
        except Exception:
            pass
