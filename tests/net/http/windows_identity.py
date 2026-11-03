"""Operator-owned Windows test identities. No production credential authority."""
import hashlib
import json
import secrets
import subprocess
from pathlib import Path


class WindowsIdentities:
    def __init__(self, helper, directory, openssl, evidence):
        self.helper, self.directory, self.openssl = map(Path, (helper, directory, openssl))
        self.destination = Path(evidence) / 'windows-credentials.json'
        self.objects, self.records = [], []
        self.before = self.after = None

    def save(self, **extra):
        self.destination.write_text(json.dumps({
            'helper_sha256': hashlib.sha256(self.helper.read_bytes()).hexdigest(),
            'inventory_before': self.before, 'inventory_after': self.after,
            'operations': self.records, **extra}, indent=2) + '\n')

    def call(self, action, *args):
        result = subprocess.run([str(self.helper), action, *map(str, args)],
                                capture_output=True, text=True, timeout=30)
        rows = [json.loads(line) for line in result.stdout.splitlines()]
        self.records.append({'action': action, 'exit': result.returncode, 'records': rows})
        self.save()
        if result.returncode:
            raise RuntimeError('Windows test credential ' + action + ' failed; public receipt retained')
        return rows

    def inventory(self):
        return next(row for row in self.call('inventory') if 'keys_sha256' in row)

    def __enter__(self):
        # Restrict the disposable CA/server private files before generation too.
        protect_directory(self.directory)
        self.before = self.inventory()
        return self

    def tool(self, *args):
        result = subprocess.run([str(self.openssl), *map(str, args)], cwd=self.directory,
                                capture_output=True, timeout=30)
        self.records.append({'action': 'certificate-tool-' + args[0], 'exit': result.returncode,
                             'diagnostic': result.stderr.decode(errors='replace')})
        self.save()
        if result.returncode:
            raise RuntimeError('Windows public certificate fixture failed; diagnostic retained')

    def provision(self, label, algorithm, public_only=False):
        nonce = secrets.token_hex(32).upper()
        item = {'label': label, 'nonce': nonce, 'public': self.directory / (label + '.public'),
                'csr': self.directory / (label + '.csr.der'), 'certificate': '-', 'public_only': public_only}
        self.objects.append(item)
        self.call('create', nonce, algorithm, item['public'], item['csr'])
        (self.directory / (label + '.ext')).write_text(
            'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n'
            'extendedKeyUsage=clientAuth\n')
        self.tool('x509', '-req', '-inform', 'DER', '-in', item['csr'], '-CA', 'cert.pem', '-CAkey', 'key.pem',
                  '-days', '1',
                  '-extfile', label + '.ext', '-out', label + '.pem')
        self.tool('x509', '-in', label + '.pem', '-outform', 'DER', '-out', label + '.der')
        item['certificate'] = self.directory / (label + '.der')
        self.call('install-public-only' if public_only else 'install', nonce, item['public'], item['certificate'])
        item['before'] = self.inspect(item)
        der = item['certificate'].read_bytes()
        item['sha256'] = hashlib.sha256(der).hexdigest()
        item['reference'] = 'CurrentUser\\VOID-HTTP-TEST-STORE-' + nonce + '\\' + hashlib.sha1(der).hexdigest().upper()
        return item

    def inspect(self, item):
        extra = ['public-only'] if item['public_only'] else []
        return next(row for row in self.call('inspect', item['nonce'], item['public'], item['certificate'], *extra)
                    if 'public_sha256' in row)

    def __exit__(self, kind, error, traceback):
        failures = []
        for item in reversed(self.objects):
            if not item['public'].exists():
                continue
            try:
                if 'before' in item and self.inspect(item) != item['before']:
                    failures.append('Borrowed credential changed: ' + item['label'])
                extra = ['public-only'] if item['public_only'] else []
                self.call('cleanup', item['nonce'], item['public'], item['certificate'], *extra)
            except BaseException as failure:
                failures.append(str(failure))
        try:
            self.after = self.inventory()
            if self.before != self.after:
                failures.append('Windows credential inventory changed')
        except BaseException as failure:
            failures.append(str(failure))
        self.save(cleanup_complete=not failures, cleanup_errors=failures,
                  test_error=kind.__name__ if kind else None)
        if failures:
            raise RuntimeError('; '.join(failures)) from error
        return False


def protect_directory(directory):
    # Replace the DACL, including any explicit pre-existing ACEs. grant:r only
    # replaces the named principals and can leave other explicit write grants.
    import os
    environment = os.environ.copy()
    environment['VOID_HTTP_PRIVATE_DIRECTORY'] = str(Path(directory).resolve())
    script = r'''$ErrorActionPreference='Stop'
$security=[System.Security.AccessControl.DirectorySecurity]::new()
$security.SetAccessRuleProtection($true,$false)
$user=[System.Security.Principal.WindowsIdentity]::GetCurrent().User
$security.SetOwner($user)
$inherit=[System.Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit'
foreach($sid in @($user,[System.Security.Principal.SecurityIdentifier]::new('S-1-5-18'))){
  $rule=[System.Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl',$inherit,'None','Allow')
  $security.AddAccessRule($rule)
}
[System.IO.Directory]::SetAccessControl($env:VOID_HTTP_PRIVATE_DIRECTORY,$security)
'''
    subprocess.run(['powershell', '-NoProfile', '-Command', script], env=environment,
                   check=True, stdout=subprocess.DEVNULL)
