"""Native Linux launcher safety tests plus a modeled macOS root-policy mismatch.

Only macOS root-selection guards are enabled in a disposable source copy.
Foundation lookup/manager completion are modeled; POSIX descriptors, lstat,
profile writes and race checks execute for real. No native macOS claim.
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
parser.add_argument('--source', type=Path, default=ROOT / 'apps/voidup/launcher.cpp')
parser.add_argument('--evidence', type=Path)
parser.add_argument('--reproduce-only', action='store_true')
args = parser.parse_args()
assert sys.platform == 'linux', 'Linux POSIX execution; macOS root selection is explicitly modeled'
receipts = []

TRACE = r'''
#include <iomanip>
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
    return ::fstatat(dir,name,info,flags);
}
#define lstat traced_lstat
#define fstatat traced_fstatat
'''

MANAGER = r'''
int main(int argc,char** argv) {
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
        # Instrument after all production includes; keep original lstat/fstatat
        # available inside the wrappers, preserving the actual syscall errno.
        # The wrappers need only the existing require helper, so place them
        # immediately after it, before any production syscall sites.
        helper = 'void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }\n'
        source = source.replace(helper, helper + TRACE.replace('#include <iomanip>\n', ''))
        source = '#include <iomanip>\n#define VOIDUP_BOOTSTRAP_ID "fixture"\n#define main bootstrap_main\n' + source + '\n#undef main\n' + MANAGER
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
        self.case = Path(self.temp.name)
        canonical = self.case / 'private/var/folders/qualification'
        canonical.mkdir(parents=True)
        (self.case / 'var').symlink_to(self.case / 'private/var', target_is_directory=True)
        self.home = self.case / "var/folders/qualification/first user 雪 ' $() # %"
        self.home.mkdir(mode=0o775)
        self.outside = self.case / 'outside'; self.outside.mkdir()
        self.trace = self.case / 'trace.jsonl'
        self.bundle = self.case / 'download'; (self.bundle / 'bin').mkdir(parents=True)
        self.initial = '# preserve developer settings\n'
        for name in ['.profile', '.bashrc', '.zshenv', '.bash_profile']:
            (self.home / name).write_text(self.initial); (self.home / name).chmod(0o664)

    def launch(self, *, platform='macos-modeled', action='', explicit=False, expected=0):
        binary = self.binaries[platform]
        for name in ['voidup-init', 'voidup']:
            shutil.copyfile(binary, self.bundle / 'bin' / name); (self.bundle / 'bin' / name).chmod(0o755)
        root = self.case / 'explicit root 雪' if explicit else self.home / ('Library/Application Support/voidup' if platform == 'macos-modeled' else 'data/voidup')
        manager_default = self.case / 'account-home/Library/Application Support/voidup' if platform == 'macos-modeled' else root
        environment = dict(os.environ, HOME=str(self.home), XDG_DATA_HOME=str(self.home / 'data'),
                           FIXTURE_ROOT=str(root), FIXTURE_ACTION=action, FIXTURE_OUTSIDE=str(self.outside),
                           FIXTURE_MANAGER_DEFAULT=str(manager_default), FIXTURE_TRACE=str(self.trace))
        operands = ['--ca-file', 'literal CA.pem']
        if explicit:operands += ['--root', str(root)]
        result = subprocess.run([str(self.bundle / 'bin/voidup-init'), *operands], env=environment,
                                capture_output=True, text=True, timeout=20)
        trace = [json.loads(line) for line in self.trace.read_text().splitlines()]
        receipts.append(dict(test=self.id().split('.')[-1], platform=platform, action=action, expected_root=str(root),
                             expected_exit=expected, exit=result.returncode, stdout=result.stdout, stderr=result.stderr, trace=trace))
        if args.evidence:
            args.evidence.mkdir(parents=True, exist_ok=True)
            (args.evidence / 'cases.json').write_text(json.dumps(receipts, indent=2, ensure_ascii=False)+'\n')
        self.assertEqual(result.returncode, expected, result.stderr)
        return root, result, trace

    def profiles_unchanged(self):
        for name in ['.profile', '.bashrc', '.zshenv', '.bash_profile']:
            self.assertEqual((self.home / name).read_text(), self.initial)

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
        self.assertEqual(trace[-1], {'event':'lstat', 'path':str(root), 'status':-1, 'errno':errno.ENOENT})
        self.profiles_unchanged()

    def test_linked_root_is_rejected(self):
        _, result, _ = self.launch(action='root-link', expected=1)
        self.assertIn('linked integration path', result.stderr); self.assertEqual(list(self.outside.iterdir()), [])
        self.profiles_unchanged()

    def test_wrong_root_type_is_rejected(self):
        _, result, _ = self.launch(action='root-file', expected=1)
        self.assertIn('not an ordinary object', result.stderr); self.profiles_unchanged()

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
