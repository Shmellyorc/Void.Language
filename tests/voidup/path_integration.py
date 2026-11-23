"""Native Linux launcher safety tests plus a modeled macOS root-policy mismatch.

macOS root selection is enabled in a disposable copy with a fixture system root
and its owner standing in for uid 0. Darwin ACLs and manager completion remain
modeled; POSIX descriptors, profile writes and race checks execute on Linux.
"""
import argparse
import errno
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--compiler', type=Path, default=shutil.which('voidc'), help='Existing compiler for the real Runtime/storage check')
parser.add_argument('--source', type=Path, default=ROOT / 'apps/voidup/launcher.cpp')
parser.add_argument('--evidence', type=Path)
parser.add_argument('--reproduce-only', action='store_true')
args = parser.parse_args()
assert sys.platform == 'linux', 'Linux POSIX execution; macOS root selection is explicitly modeled'
receipts = []

TRACE = r'''
#include <iomanip>
#include <cstdarg>
int traced_lstat(const char* path, struct stat* info) {
    static unsigned root_checks=0;
    auto expected=std::getenv("FIXTURE_ROOT");
    auto action=std::getenv("FIXTURE_ACTION");
    if(expected && std::string(path)==expected && ++root_checks==3 && action) {
        if(std::string(action)=="replace-root" || std::string(action)=="remove-root") {
            fs::rename(path,std::string(path)+".retired");
            if(std::string(action)=="replace-root")fs::create_directory(path);
        }
    }
    auto status=::lstat(path,info);auto saved=errno;
    std::ofstream log(std::getenv("FIXTURE_TRACE"),std::ios::app);
    log<<"{\"event\":\"lstat\",\"path\":"<<std::quoted(path)
       <<",\"status\":"<<status<<",\"errno\":"<<(status<0?saved:0)<<"}\n";
    errno=saved;return status;
}
int traced_fstatat(int dir,const char* name,struct stat* info,int flags) {
    auto action=std::getenv("FIXTURE_ACTION");
    if(action && std::string(action)=="replace-profile" && std::string(name)==".profile") {
        require(!renameat(dir,name,dir,".profile.user-original"),"Fixture rename failed");
        int file=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL,0664);
        require(file>=0,"Fixture create failed");
        const std::string changed="user changed this profile\n";
        require(::write(file,changed.data(),changed.size())==ssize_t(changed.size()),"Fixture write failed");close(file);
    }
    auto status=::fstatat(dir,name,info,flags);auto saved=errno;
    if(!status && action && std::string(action)=="alias-owner" && std::string(name)=="var" &&
       fs::read_symlink("/proc/self/fd/"+std::to_string(dir))==fs::path(std::getenv("FIXTURE_SYSTEM_ROOT")))info->st_uid+=1;
    errno=saved;return status;
}
ssize_t traced_readlinkat(int dir,const char* name,char* bytes,size_t size) {
    static unsigned reads=0;auto action=std::getenv("FIXTURE_ACTION");
    auto system=fs::path(std::getenv("FIXTURE_SYSTEM_ROOT"));
    if(std::string(name)=="var" && fs::read_symlink("/proc/self/fd/"+std::to_string(dir))==system && ++reads==2 && action) {
        if(std::string(action)=="replace-alias") {
            auto target=fs::read_symlink(system/"var");fs::rename(system/"var",system/"var.retired");
            fs::create_directory_symlink(target,system/"var");
        }
        if(std::string(action)=="replace-ancestor") {
            auto home=fs::path(std::getenv("HOME"));fs::rename(home,home.string()+".retired");fs::create_directory(home);
        }
    }
    return ::readlinkat(dir,name,bytes,size);
}
#define lstat traced_lstat
#define fstatat traced_fstatat
#define readlinkat traced_readlinkat
'''

MANAGER = r'''
int main(int argc,char** argv) {
#ifdef FIXTURE_MACOS_ROOT
    if(argc==2 && std::string(argv[1])=="--select-root")try {
        macos_root_selection selected(default_root());std::cout<<selected.path.string();return 0;
    } catch(const std::system_error& e){std::cerr<<e.code().value()<<" "<<e.what();return 1;}
      catch(const std::exception& e){std::cerr<<e.what();return 1;}
#endif
    if(fs::path(argv[0]).filename()=="voidup") {
        fs::path root=std::getenv("FIXTURE_MANAGER_DEFAULT");
        for(int i=1;i+1<argc;++i)if(std::string(argv[i])=="--root")root=argv[i+1];
        {
            std::ofstream log(std::getenv("FIXTURE_TRACE"),std::ios::app);
            log<<"{\"event\":\"manager\",\"root\":"<<std::quoted(root.string())<<",\"args\":[";
            for(int i=1;i<argc;++i){if(i>1)log<<',';log<<std::quoted(argv[i]);}log<<"]}\n";
        }
        auto action=std::string(std::getenv("FIXTURE_ACTION"));
        if(action=="manager-failure")return 37;
        if(action!="missing-root") {
            fs::create_directories(root.parent_path());
            if(action=="root-file")std::ofstream(root)<<"root is a file";
            else if(action=="root-link")fs::create_directory_symlink(std::getenv("FIXTURE_OUTSIDE"),root);
            else {
                fs::create_directories(root);std::ofstream(root/"writer.lock")<<"fixture lock";
                if(action=="bin-link")fs::create_directory_symlink(std::getenv("FIXTURE_OUTSIDE"),root/"bin");
                if(action=="bin-file")std::ofstream(root/"bin")<<"bin is a file";
            }
        }
        if(action=="manager-replace-ancestor") {
            auto home=fs::path(std::getenv("HOME"));fs::rename(home,home.string()+".retired");fs::create_directory(home);
        }
        if(action=="manager-link-new-ancestor") {
            auto home=fs::path(std::getenv("HOME"));fs::rename(home/"Library",home/"Library.retired");
            fs::create_directory_symlink(home/"Library.retired",home/"Library");
        }
        std::cout<<"VOID 1.10.0 is ready.\n";return 0;
    }
    return run_main(argc,argv);
}
'''


class Integration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = tempfile.TemporaryDirectory(prefix='voidup-path-integration-')
        cls.addClassCleanup(cls.scratch.cleanup)
        cls.work = Path(cls.scratch.name).resolve()
        source = args.source.resolve().read_text()
        # Model only macOS default-root selection and the surrounding bootstrap
        # root handoff guards. Image lookup, ACLs and syscalls remain Linux.
        source = source.replace('#elif defined(__APPLE__)\n    return home()/',
                                '#elif defined(__APPLE__) || defined(FIXTURE_MACOS_ROOT)\n    return home()/')
        start = source.index('auto root=default_root();')
        end = source.index('auto status=run(manager,args);', start)
        source = source[:start] + source[start:end].replace('#ifdef __APPLE__',
            '#if defined(__APPLE__) || defined(FIXTURE_MACOS_ROOT)') + source[end:]
        source = source.replace('#ifdef __APPLE__\n    if(selected_root)selected_root->verify();',
            '#if defined(__APPLE__) || defined(FIXTURE_MACOS_ROOT)\n    if(selected_root)selected_root->verify();')
        source = source.replace('#ifdef __APPLE__\n    if(selected_root)selected_root->complete();',
            '#if defined(__APPLE__) || defined(FIXTURE_MACOS_ROOT)\n    if(selected_root)selected_root->complete();')
        source = source.replace('#if defined(__APPLE__)\n// Only the default',
            '#if defined(__APPLE__) || defined(FIXTURE_MACOS_ROOT)\n// Only the default')
        source = source.replace('const fs::path system_root = "/";',
            'const fs::path system_root = std::getenv("FIXTURE_SYSTEM_ROOT");')
        source = source.replace('static uid_t system_uid() { return 0; }',
            'static uid_t system_uid() { return geteuid(); }')
        # Instrument after all production includes; keep original lstat/fstatat
        # available inside the wrappers, preserving the actual syscall errno.
        # The wrappers need only the existing require helper, so place them
        # immediately after it, before any production syscall sites.
        helper = 'void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }\n'
        source = source.replace(helper, helper + TRACE.replace('#include <iomanip>\n', '').replace('#include <cstdarg>\n', ''))
        source = '#include <iomanip>\n#include <cstdarg>\n#define VOIDUP_BOOTSTRAP_ID "fixture"\n#define main bootstrap_main\n' + source + '\n#undef main\n' + MANAGER
        cpp = cls.work / 'probe.cpp'; cpp.write_text(source)
        cls.binaries = {}
        for platform in ['linux', 'macos-modeled']:
            binary = cls.work / platform
            subprocess.run([str(args.sdk / 'bin/clang++'), '--no-default-config', '-std=c++17',
                            '--ld-path=' + str(args.sdk / 'bin/ld.lld'),
                            *(['-DFIXTURE_MACOS_ROOT'] if platform == 'macos-modeled' else []),
                            str(cpp), '-o', str(binary)], check=True, timeout=90)
            cls.binaries[platform] = binary

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='case-', dir=self.work)
        self.addCleanup(self.temp.cleanup)
        self.case = Path(self.temp.name); self.case.chmod(0o755)
        canonical = self.case / 'private/var/folders/qualification'
        canonical.mkdir(parents=True)
        (self.case/'private').chmod(0o755); (self.case/'private/var').chmod(0o755)
        (self.case / 'var').symlink_to(self.case / 'private/var', target_is_directory=True)
        self.home = self.case / "var/folders/qualification/first user 雪 ' $() # %"
        self.home.mkdir(mode=0o775)
        self.physical_home = self.home.resolve()
        self.outside = self.case / 'outside'; self.outside.mkdir()
        self.trace = self.case / 'trace.jsonl'
        self.bundle = self.case / 'download'; (self.bundle / 'bin').mkdir(parents=True)
        self.initial = '# preserve developer settings\n'
        for name in ['.profile', '.bashrc', '.zshenv', '.bash_profile']:
            (self.home / name).write_text(self.initial); (self.home / name).chmod(0o664)

    def launch(self, *, platform='macos-modeled', action='', explicit=False, explicit_path=None, expected=0):
        binary = self.binaries[platform]
        for name in ['voidup-init', 'voidup']:
            shutil.copyfile(binary, self.bundle / 'bin' / name); (self.bundle / 'bin' / name).chmod(0o755)
        root = self.case / 'explicit root 雪' if explicit else (self.home.resolve()/'Library/Application Support/voidup' if platform=='macos-modeled' else self.home/'data/voidup')
        if explicit_path is not None:root=explicit_path;explicit=True
        manager_default = self.case / 'account-home/Library/Application Support/voidup' if platform == 'macos-modeled' else root
        environment = dict(os.environ, HOME=str(self.home), XDG_DATA_HOME=str(self.home / 'data'),
                           FIXTURE_ROOT=str(root), FIXTURE_ACTION=action, FIXTURE_OUTSIDE=str(self.outside), FIXTURE_SYSTEM_ROOT=str(self.case),
                           FIXTURE_MANAGER_DEFAULT=str(manager_default), FIXTURE_TRACE=str(self.trace))
        operands = ['--ca-file', 'literal CA.pem']
        if explicit:operands += ['--root', str(root)]
        result = subprocess.run([str(self.bundle / 'bin/voidup-init'), *operands], env=environment,
                                capture_output=True, text=True, timeout=20)
        trace = [json.loads(line) for line in self.trace.read_text().splitlines()] if self.trace.exists() else []
        receipts.append(dict(test=self.id().split('.')[-1], platform=platform, action=action, expected_root=str(root),
                             expected_exit=expected, exit=result.returncode, stdout=result.stdout, stderr=result.stderr, trace=trace))
        if args.evidence:
            args.evidence.mkdir(parents=True, exist_ok=True)
            (args.evidence / 'cases.json').write_text(json.dumps(receipts, indent=2, ensure_ascii=False)+'\n')
        self.assertEqual(result.returncode, expected, result.stderr)
        return root, result, trace

    def profiles_unchanged(self, location=None):
        for name in ['.profile', '.bashrc', '.zshenv', '.bash_profile']:
            self.assertEqual(((location or self.home) / name).read_text(), self.initial)

    def select(self):
        result = subprocess.run([self.binaries['macos-modeled'], '--select-root'], capture_output=True, text=True,
            env=dict(os.environ, HOME=str(self.home), FIXTURE_SYSTEM_ROOT=str(self.case), FIXTURE_ACTION=''), timeout=20)
        receipts.append(dict(test=self.id().split('.')[-1], stage='selection-only', exit=result.returncode,
                             stdout=result.stdout, stderr=result.stderr))
        return result

    def test_relative_system_alias_uses_physical_root(self):
        (self.case/'var').unlink(); (self.case/'var').symlink_to('private/var', target_is_directory=True)
        root, _, trace = self.launch()
        self.assertEqual(root, self.physical_home/'Library/Application Support/voidup')
        self.assertEqual(trace[0]['root'], str(root))

    def test_physical_default_home_is_unchanged(self):
        self.home = self.physical_home
        root, _, trace = self.launch()
        self.assertEqual(trace[0]['root'], str(self.home/'Library/Application Support/voidup'))

    def test_missing_suffix_is_selected_without_creation(self):
        result = self.select(); self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, str(self.physical_home/'Library/Application Support/voidup'))
        self.assertFalse((self.physical_home/'Library').exists())

    def test_unexpected_alias_target_is_rejected_before_manager(self):
        (self.case/'var').unlink(); (self.case/'var').symlink_to(self.outside, target_is_directory=True)
        _, result, trace = self.launch(expected=1)
        self.assertIn('Unexpected macOS system alias target', result.stderr); self.assertEqual(trace, [])
        self.profiles_unchanged(self.physical_home); self.assertEqual(list(self.outside.iterdir()), [])

    def test_user_controlled_alias_is_rejected_before_manager(self):
        _, result, trace = self.launch(action='alias-owner', expected=1)
        self.assertIn('user-controlled', result.stderr); self.assertEqual(trace, []); self.profiles_unchanged()

    def test_writable_system_ancestors_are_rejected(self):
        for directory in [self.case, self.case/'private', self.case/'private/var']:
            with self.subTest(directory=directory.name):
                directory.chmod(0o775)
                _, result, trace = self.launch(expected=1)
                self.assertIn('Unsafe macOS system directory', result.stderr); self.assertEqual(trace, [])
                directory.chmod(0o755)
        self.profiles_unchanged()

    def test_linked_physical_system_ancestor_is_rejected(self):
        (self.case/'private').rename(self.case/'private.retired')
        (self.case/'private').symlink_to(self.case/'private.retired', target_is_directory=True)
        _, result, trace = self.launch(expected=1)
        self.assertIn('Cannot open default root ancestor', result.stderr); self.assertEqual(trace, [])
        self.profiles_unchanged(); self.assertEqual(list(self.outside.iterdir()), [])

    def test_unexpected_user_symlink_ancestor_is_rejected(self):
        (self.physical_home/'Library').symlink_to(self.outside, target_is_directory=True)
        _, _, trace = self.launch(expected=1); self.assertEqual(trace, [])
        self.profiles_unchanged(); self.assertEqual(list(self.outside.iterdir()), [])

    def test_missing_system_target_and_non_directory_are_distinct(self):
        target = self.case/'private/var'; target.rename(self.case/'var.retired')
        result = self.select(); self.assertEqual(result.returncode, 1); self.assertTrue(result.stderr.startswith('2 '), result.stderr)
        target.write_text('not a directory')
        result = self.select(); self.assertEqual(result.returncode, 1); self.assertTrue(result.stderr.startswith('20 '), result.stderr)
        self.assertEqual(target.read_text(), 'not a directory')

    def test_replaced_alias_is_rejected_before_manager(self):
        _, result, trace = self.launch(action='replace-alias', expected=1)
        self.assertIn('system alias changed', result.stderr); self.assertEqual(trace, []); self.profiles_unchanged()

    def test_replaced_ancestor_is_rejected_before_manager(self):
        _, result, trace = self.launch(action='replace-ancestor', expected=1)
        self.assertIn('ancestor changed', result.stderr); self.assertEqual(trace, [])
        self.profiles_unchanged(Path(str(self.physical_home)+'.retired'))
        self.assertFalse((self.physical_home/'Library').exists())

    def test_replaced_ancestor_after_manager_prevents_integration(self):
        _, result, trace = self.launch(action='manager-replace-ancestor', expected=1)
        self.assertIn('ancestor changed', result.stderr); self.assertEqual(len(trace), 1)
        self.profiles_unchanged(Path(str(self.physical_home)+'.retired'))
        self.assertEqual(list(self.physical_home.iterdir()), [])

    def test_linked_new_ancestor_after_manager_prevents_integration(self):
        _, result, trace = self.launch(action='manager-link-new-ancestor', expected=1)
        self.assertIn('Cannot open default root ancestor', result.stderr); self.assertEqual(len(trace), 1)
        self.profiles_unchanged(); self.assertFalse((self.physical_home/'Library/Application Support/voidup/bin').exists())

    def test_retry_after_rejected_alias_preserves_profiles(self):
        self.launch(action='replace-alias', expected=1); self.profiles_unchanged()
        root, _, _ = self.launch()
        self.assertTrue((root/'bin/voidup').is_file())
        self.assertEqual((self.home/'.profile').read_text().count('# VOIDUP terminal tools'), 1)

    def test_explicit_alias_root_is_forwarded_literally(self):
        raw = self.home/'Library/Application Support/voidup'
        _, _, trace = self.launch(explicit_path=raw)
        self.assertEqual(trace[0]['args'], ['--ca-file', 'literal CA.pem', '--root', str(raw)])

    def test_selected_root_reaches_real_voidup_storage_and_runtime(self):
        sources=self.case/'source';sources.mkdir()
        for path in (ROOT/'apps/voidup').glob('*.void'):shutil.copyfile(path,sources/path.name)
        entry=sources/'probe.void'
        def invoke(root):
            entry.write_text('use "./storage.void" as storage\nuse json as codec\n'
                'func main() -> i32:\n'
                '    var outcome = storage.up_root('+json.dumps(str(root),ensure_ascii=False)+')\n'
                '    if outcome is Err(var failure):\n'
                '        if codec.stringify(failure) is Ok(var rendered):\n'
                '            print(rendered)\n'
                '        return 1\n'
                '    print("ROOT_READY")\n'
                '    return 0\n')
            self.assertIsNotNone(args.compiler, 'Pass --compiler for the actual Runtime check')
            result=subprocess.run([str(args.compiler),'run','--no-cache',str(entry)],
                                  capture_output=True,text=True,timeout=60)
            receipts.append(dict(test=self.id().split('.')[-1],stage='actual VOIDUP storage/installed Runtime',
                requested=str(root),exit=result.returncode,stdout=result.stdout,stderr=result.stderr))
            return result
        raw=self.home/'Library/Application Support/voidup'
        rejected=invoke(raw);self.assertEqual(rejected.returncode,1,rejected.stderr)
        error=json.loads(rejected.stdout);self.assertEqual(error['operation'],'io.create_directory_handle')
        self.assertEqual(error['native_code'],errno.ENOTDIR)
        selected=self.select();self.assertEqual(selected.returncode,0,selected.stderr)
        root=Path(selected.stdout);ready=invoke(root);self.assertEqual(ready.returncode,0,ready.stderr)
        self.assertIn('ROOT_READY',ready.stdout)
        before={name:(root/name).read_bytes() for name in ['control.json','voidup-root.json']}
        retried=invoke(root);self.assertEqual(retried.returncode,0,retried.stderr)
        self.assertEqual(before,{name:(root/name).read_bytes() for name in before})
        self.profiles_unchanged()

    def test_default_macos_root_handoff(self):
        root, result, trace = self.launch()
        self.assertEqual(trace[0]['root'], str(root))
        self.assertEqual(trace[0]['args'], ['--root', str(root), '--ca-file', 'literal CA.pem'])
        self.assertFalse((self.case / 'account-home').exists())
        self.assertEqual(trace[1], {'event':'lstat', 'path':str(root), 'status':0, 'errno':0})
        for name in ['voidc', 'voidls', 'voidup']:
            self.assertTrue((root / 'bin' / name).is_file())
        self.launch()  # Repeated setup and repeated shell startup must be idempotent.
        for name in ['.profile', '.bashrc', '.zshenv', '.bash_profile']:
            profile = self.home / name
            self.assertTrue(profile.read_text().startswith(self.initial))
            self.assertEqual(profile.stat().st_mode & 0o777, 0o664)
            self.assertEqual(profile.read_text().count('# VOIDUP terminal tools'), 1)
        terminal = subprocess.run(['/bin/sh', '-c', '. "$HOME/.profile"; . "$HOME/.bashrc"; . "$HOME/.zshenv"; printf "%s" "$PATH"'],
                                  env=dict(os.environ, HOME=str(self.home), PATH='/usr/bin:/bin'), capture_output=True, text=True, check=True)
        self.assertEqual(terminal.stdout.split(':').count(str(root / 'bin')), 1)

    def test_explicit_root_is_preserved_without_duplicate_option(self):
        root, _, trace = self.launch(explicit=True)
        self.assertEqual(trace[0]['args'], ['--ca-file', 'literal CA.pem', '--root', str(root)])

    def test_linux_arguments_and_default_root_unchanged(self):
        _, _, trace = self.launch(platform='linux')
        self.assertEqual(trace[0]['args'], ['--ca-file', 'literal CA.pem'])

    def test_manager_failure_precedes_integration(self):
        _, _, trace = self.launch(action='manager-failure', expected=37)
        self.assertEqual(len(trace), 1); self.profiles_unchanged()

    def test_missing_completed_root_is_rejected(self):
        root, _, trace = self.launch(action='missing-root', expected=1)
        self.assertEqual(len(trace), 1); self.assertFalse(root.exists())
        self.profiles_unchanged()

    def test_linked_root_is_rejected(self):
        _, result, _ = self.launch(action='root-link', expected=1)
        self.assertIn('Cannot open default root ancestor', result.stderr); self.assertEqual(list(self.outside.iterdir()), [])
        self.profiles_unchanged()

    def test_wrong_root_type_is_rejected(self):
        _, result, _ = self.launch(action='root-file', expected=1)
        self.assertIn('Cannot open default root ancestor', result.stderr); self.profiles_unchanged()

    def test_linked_integration_ancestor_is_rejected(self):
        self.launch(action='bin-link', expected=1)
        self.assertEqual(list(self.outside.iterdir()), []); self.profiles_unchanged()

    def test_wrong_integration_ancestor_type_is_rejected(self):
        self.launch(action='bin-file', expected=1); self.profiles_unchanged()

    def test_missing_home_is_rejected(self):
        shutil.rmtree(self.home)
        _, _, trace = self.launch(explicit=True, expected=1)
        self.assertEqual(trace[-1], {'event':'lstat', 'path':str(self.home), 'status':-1, 'errno':errno.ENOENT})
        self.assertFalse(self.home.exists())

    def test_linked_home_is_rejected(self):
        shutil.rmtree(self.home); self.home.symlink_to(self.outside, target_is_directory=True)
        _, result, _ = self.launch(explicit=True, expected=1)
        self.assertIn('linked integration path', result.stderr); self.assertEqual(list(self.outside.iterdir()), [])

    def test_wrong_home_type_is_rejected(self):
        shutil.rmtree(self.home); self.home.write_text('ordinary file')
        self.launch(explicit=True, expected=1); self.assertEqual(self.home.read_text(), 'ordinary file')

    def test_linked_profile_is_rejected(self):
        sentinel = self.outside / 'sentinel'; sentinel.write_text('preserve outside bytes')
        (self.home / '.profile').unlink(); (self.home / '.profile').symlink_to(sentinel)
        self.launch(expected=1); self.assertEqual(sentinel.read_text(), 'preserve outside bytes')

    def test_profile_directory_is_rejected(self):
        (self.home / '.profile').unlink(); (self.home / '.profile').mkdir()
        self.launch(expected=1); self.assertTrue((self.home / '.profile').is_dir())

    def test_profile_fifo_is_rejected_without_blocking(self):
        (self.home / '.profile').unlink(); os.mkfifo(self.home / '.profile')
        self.launch(expected=1)

    def test_replaced_root_is_rejected_before_profile_writes(self):
        _, result, _ = self.launch(action='replace-root', expected=1)
        self.assertIn('Integration root changed', result.stderr); self.profiles_unchanged()

    def test_removed_root_is_rejected_before_profile_writes(self):
        _, result, _ = self.launch(action='remove-root', expected=1)
        self.assertIn('Integration root changed', result.stderr); self.profiles_unchanged()

    def test_profile_change_is_not_overwritten(self):
        _, result, _ = self.launch(action='replace-profile', expected=1)
        self.assertIn('Shell profile changed', result.stderr)
        self.assertEqual((self.home / '.profile').read_text(), 'user changed this profile\n')
        self.assertEqual((self.home / '.profile.user-original').read_text(), self.initial)


if __name__ == '__main__':
    suite = unittest.TestSuite([Integration('test_default_macos_root_handoff')]) if args.reproduce_only else unittest.defaultTestLoader.loadTestsFromTestCase(Integration)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())
