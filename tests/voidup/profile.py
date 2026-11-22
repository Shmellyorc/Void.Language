"""Profile replacement preserves ownership, mode, named ACLs and absent ACLs."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk', type=Path, required=True)
p.add_argument('--source', type=Path, default=ROOT/'apps/voidup/launcher.cpp')
a = p.parse_args()
assert sys.platform == 'linux', 'Native Linux profile/ACL regression'
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
with tempfile.TemporaryDirectory(prefix='voidup-profile-') as directory:
    work = Path(directory); home = work/'home'; home.mkdir()
    harness = work/'probe.cpp'
    harness.write_text('#define VOIDUP_BOOTSTRAP_ID "test"\n#define main bootstrap_main\n#include '+
                       json.dumps(str(a.source.resolve()))+'\n#undef main\n'+
                       'int main(int argc,char** argv){try{integrate_path(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<e.what();return 1;}}\n')
    binary = work/'probe'
    subprocess.run([str(a.sdk/'bin/clang++'), '--no-default-config', '-std=c++17',
                    '--ld-path='+str(a.sdk/'bin/ld.lld'), str(harness), '-o', str(binary)], check=True)
    def acl(entries):
        return struct.pack('<I', 2)+b''.join(struct.pack('<HHI', *entry) for entry in entries)
    def state(path):
        info = path.stat()
        try: access = os.getxattr(path, 'system.posix_acl_access').hex()
        except OSError as error:
            if error.errno != 61: raise
            access = None
        return dict(uid=info.st_uid, gid=info.st_gid, mode=info.st_mode & 0o777, acl=access)
    profiles = [home/'.profile', home/'.bashrc']
    group = next((gid for gid in os.getgroups() if gid != os.getgid()), os.getgid())
    for profile in profiles:
        profile.write_text('# preserve developer settings\n')
        os.chown(profile, -1, group); profile.chmod(0o664)
    os.setxattr(profiles[1], 'system.posix_acl_access', acl([(1,6,0xffffffff),(4,6,0xffffffff),(8,4,65534),(16,6,0xffffffff),(32,4,0xffffffff)]))
    os.setxattr(home, 'system.posix_acl_default', acl([(1,7,0xffffffff),(4,5,0xffffffff),(8,5,65533),(16,5,0xffffffff),(32,5,0xffffffff)]))
    before = [state(profile) for profile in profiles]
    for _ in range(2):
        subprocess.run([str(binary), str(home/'bin')], env=dict(os.environ, HOME=str(home)), check=True, timeout=20, preexec_fn=limits)
        after = [state(profile) for profile in profiles]
        assert before == after, {'before': before, 'after': after}
        assert all(profile.read_text().startswith('# preserve developer settings\n') for profile in profiles)
    # Login/interactive startup may source several managed profiles. Repeated
    # sourcing must preserve unrelated entries and add exactly one tools path.
    for initial in ('/usr/bin:/bin', '', str(home/'bin')+':/usr/bin'):
        result=subprocess.run(['/bin/sh','-c','. "$HOME/.profile"; . "$HOME/.bashrc"; . "$HOME/.profile"; printf "%s" "$PATH"'],
                              env=dict(os.environ,HOME=str(home),PATH=initial),capture_output=True,text=True,check=True)
        parts=result.stdout.split(':')
        assert parts.count(str(home/'bin'))==1 and '' not in parts,parts
        assert [x for x in parts if x!=str(home/'bin')]==[x for x in initial.split(':') if x and x!=str(home/'bin')],parts
    # Migrate the exact G96 export while keeping surrounding user bytes/access.
    quoted="'"+str(home/'bin').replace("'", "'\"'\"'")+"'"
    profiles[0].write_text('# preserve developer settings\n\n# VOIDUP terminal tools\nexport PATH='+quoted+':"$PATH"\n# after managed export\n')
    subprocess.run([str(binary),str(home/'bin')],env=dict(os.environ,HOME=str(home)),check=True,timeout=20)
    contents=profiles[0].read_text()
    assert contents.count('# VOIDUP terminal tools')==1 and '# after managed export\n' in contents
print('PASS: profile owner/group/mode, named ACL and absent ACL preserved; idempotent retry')
print('Supplementary-group ownership exercised:', group != os.getgid())
