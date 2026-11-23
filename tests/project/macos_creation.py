"""Focused Linux execution of actual creation plans with a modeled macOS prefix.

Uses an existing Makefiles build's compile/link recipes and static libraries.
Only a disposable project_creation.cpp copy enables the Apple selector, replaces
its filesystem root, and substitutes the fixture owner for UID 0. Darwin ACLs
are excluded; ordinary Parent traversal and publication execute unchanged.
No compiler installation or qualification-fixture changes are performed.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--source', type=Path, default=ROOT/'src/frontend/project_creation.cpp')
args = parser.parse_args()
assert sys.platform == 'linux', 'Linux model only; native Apple coverage is in creation.cpp'
args.build=args.build.resolve();args.evidence=args.evidence.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
receipts=[]

PROBE = r'''
#include "void/project_creation.hpp"
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace vd::project;
std::string action;
fs::path system_root, destination;
bool injected=false;
extern "C" int __real_fstatat(int,const char*,struct stat*,int);
extern "C" int __wrap_fstatat(int parent,const char* name,struct stat* info,int flags) {
    auto status=__real_fstatat(parent,name,info,flags);
    if(!status && action=="alias-owner" && std::string(name)=="var" && S_ISLNK(info->st_mode))info->st_uid+=1;
    return status;
}
extern "C" ssize_t __real_readlinkat(int,const char*,char*,size_t);
extern "C" ssize_t __wrap_readlinkat(int parent,const char* name,char* bytes,size_t size) {
    static unsigned count=0;
    if(std::string(name)=="var" && ++count==2) {
        if(action=="replace-alias") {
            auto target=fs::read_symlink(system_root/"var");
            fs::rename(system_root/"var",system_root/"old-var");fs::create_directory_symlink(target,system_root/"var");
        }
        if(action=="replace-system-directory") {
            fs::rename(system_root/"private",system_root/"old-private");
            fs::create_directories(system_root/"private/var");
        }
    }
    return __real_readlinkat(parent,name,bytes,size);
}
extern "C" int __real_openat(int,const char*,int,...);
extern "C" int __wrap_openat(int parent,const char* name,int flags,...) {
    mode_t mode=0;if(flags&O_CREAT){va_list values;va_start(values,flags);mode=va_arg(values,int);va_end(values);}
    if(!injected && action=="replace-user-ancestor" && std::string(name)=="workspace") {
        injected=true;fs::rename(system_root/"private/var/workspace",system_root/"private/var/old-workspace");
        fs::create_directory_symlink(system_root/"outside",system_root/"private/var/workspace");
    }
    return __real_openat(parent,name,flags,mode);
}
std::error_code entropy(void* bytes,std::size_t size) {
    std::memset(bytes,7,size);
    if(!injected && action=="replace-parent-after-admission") {
        injected=true;auto parent=system_root/"private/var/workspace";
        fs::rename(parent,parent.parent_path()/"retired-workspace");fs::create_directory(parent);
    }
    return {};
}
void checkpoint(const fs::path& stage) {
    if(action=="rollback")throw std::runtime_error("injected verification failure");
    if(action=="foreign")std::ofstream(stage/"foreign")<<"keep foreign bytes";
    if(action=="tamper")std::ofstream(stage/"src/app.void",std::ios::app)<<"// changed";
    if(action=="late-collision") {fs::create_directory(destination);std::ofstream(destination/"external")<<"keep winner";}
    if(action=="replace-stage") {
        fs::rename(stage,stage.string()+".retired");fs::create_directory(stage);
        std::ofstream(stage/"external")<<"keep replacement";
    }
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    system_root=std::getenv("VOID_CREATION_SYSTEM_ROOT");action=argv[1];
    try {
        CreationOptions options;options.new_directory=std::string(argv[2])!="init";
        options.in_place=std::string(argv[2])=="in-place";
        auto plan=plan_creation(argv[3],options,entropy);destination=plan.root;
        if(action=="alias-after-plan") {
            fs::rename(system_root/"var",system_root/"old-var");
            fs::create_directory_symlink(system_root/"outside",system_root/"var");
        }
        publish_creation(plan,checkpoint);
        std::cout<<plan.root.string()<<'\n';return 0;
    } catch(const std::system_error& error) {std::cerr<<"errno="<<error.code().value()<<" "<<error.what()<<'\n';return 1;}
      catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
'''

def run(command, **kwargs):
    result=subprocess.run(list(map(str,command)),capture_output=True,text=True,timeout=120,**kwargs)
    receipts.append(dict(command=list(map(str,command)),exit=result.returncode,stdout=result.stdout,stderr=result.stderr))
    (args.evidence/'receipts.json').write_text(json.dumps(receipts,indent=2)+'\n')
    return result

class Creation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work=args.evidence/'compiled';cls.work.mkdir(exist_ok=True)
        commands=json.loads((args.build/'compile_commands.json').read_text())
        creation=next(x for x in commands if x['file'].endswith('/src/frontend/project_creation.cpp'))
        unit=next(x for x in commands if x['file'].endswith('/tests/project/creation.cpp'))
        recipe=shlex.split((args.build/'CMakeFiles/void_project_creation.dir/link.txt').read_text())
        cls.cpp=shlex.split(creation['command'])[0]
        def compile(command,source,output):
            values=shlex.split(command['command']);values[values.index('-c')+1]=str(source)
            values[values.index('-o')+1]=str(output)
            values+=['-I'+str(ROOT/'src/frontend')]
            result=run(values,cwd=args.build);assert result.returncode==0,result.stderr
        def link(object,output,extra=(),wrapped=False):
            values=[];skip=False
            for value in recipe:
                if skip:skip=False;continue
                if value=='-Xlinker':skip=True;continue
                if value.endswith('tests/project/creation.cpp.o'):values.extend([str(object),*map(str,extra)]);continue
                values.append(value)
            values[values.index('-o')+1]=str(output)
            if wrapped:values+=['-Wl,--wrap=openat,--wrap=readlinkat,--wrap=fstatat']
            result=run(values,cwd=args.build);assert result.returncode==0,result.stderr
        native=cls.work/'project-native.o';compile(creation,args.source.resolve(),native)
        unit_object=cls.work/'creation-test.o';compile(unit,ROOT/'tests/project/creation.cpp',unit_object)
        cls.existing=cls.work/'creation-existing';link(unit_object,cls.existing,[native])
        source=args.source.read_text()
        source=source.replace('#ifdef __APPLE__\n// Select only', '#if defined(__APPLE__) || defined(FIXTURE_MACOS_CREATION)\n// Select only')
        source=source.replace('#ifdef __APPLE__\n    MacosCreationTarget selected(root);','#if defined(__APPLE__) || defined(FIXTURE_MACOS_CREATION)\n    MacosCreationTarget selected(root);')
        source=source.replace('#ifdef __APPLE__\n    selected.verify();','#if defined(__APPLE__) || defined(FIXTURE_MACOS_CREATION)\n    selected.verify();')
        source=source.replace('const fs::path system_root="/";','const fs::path system_root=std::getenv("VOID_CREATION_SYSTEM_ROOT");')
        source=source.replace('static uid_t system_owner() {return 0;}','static uid_t system_owner() {return geteuid();}')
        copy=cls.work/'project-modeled.cpp';copy.write_text('#include <cstdlib>\n#define FIXTURE_MACOS_CREATION\n'+source)
        modeled=cls.work/'project-modeled.o';compile(creation,copy,modeled)
        probe=cls.work/'probe.cpp';probe.write_text(PROBE)
        probe_object=cls.work/'probe.o';compile(unit,probe,probe_object)
        cls.probe=cls.work/'probe';link(probe_object,cls.probe,[modeled],True)
        # Relink the actual CLI with this one modeled translation unit; retain
        # the configured main/backend/runtime objects and dependency recipe.
        cli=shlex.split((args.build/'CMakeFiles/voidc.dir/link.txt').read_text())
        values=[];skip=False
        for value in cli:
            if skip:skip=False;continue
            if value=='-Xlinker':skip=True;continue
            if value=='libvoid_core.a':values.append(str(modeled))
            values.append(value)
        cls.compiler=cls.work/'voidc-modeled';values[values.index('-o')+1]=str(cls.compiler)
        result=run(values,cwd=args.build);assert result.returncode==0,result.stderr

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='creation-system-alias-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.root.chmod(0o755)
        self.physical=self.root/'private/var/workspace';self.physical.mkdir(parents=True)
        (self.root/'private').chmod(0o755);(self.root/'private/var').chmod(0o755)
        (self.root/'var').symlink_to('private/var',target_is_directory=True)
        self.raw=self.root/'var/workspace';self.outside=self.root/'outside';self.outside.mkdir()
        self.env=dict(os.environ,VOID_CREATION_SYSTEM_ROOT=str(self.root))

    def create(self,target=None,*,action='',operation='new',success=True):
        result=run([self.probe,action,operation,target or self.raw/'hello'],env=self.env)
        self.assertEqual(result.returncode,0 if success else 1,result.stderr)
        return result

    def test_existing_creation_publication_and_rollback_suite(self):
        result=run([self.existing]);self.assertEqual(result.returncode,0,result.stderr)

    def test_actual_cli_new_and_init(self):
        for operation in ['new','init']:
            target=self.raw/operation
            if operation=='init':target.mkdir()
            result=run([self.compiler,operation,target],env=self.env)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertTrue((target/'void.project.toml').is_file())
            if operation=='new':self.assertTrue((target/'src/app.void').is_file())

    def test_relative_alias_publication_and_collision(self):
        self.create();self.assertTrue((self.physical/'hello/src/app.void').is_file())
        self.create(success=False);self.create(self.raw/'HELLO',success=False)

    def test_absolute_system_alias(self):
        (self.root/'var').unlink();(self.root/'var').symlink_to(self.root/'private/var',target_is_directory=True)
        self.create()

    def test_physical_path(self):
        result=self.create(self.physical/'hello');self.assertEqual(result.stdout.strip(),str(self.physical/'hello'))

    def test_init_and_in_place(self):
        for mode in ['init','in-place']:
            target=self.raw/mode;target.mkdir();self.create(target,operation=mode)
            self.assertTrue((target/'void.project.toml').is_file())

    def test_missing_ancestor(self):
        self.create(self.raw/'missing/hello',success=False);self.assertFalse((self.physical/'missing').exists())

    def test_non_directory_ancestor(self):
        (self.physical/'file').write_text('keep');result=self.create(self.raw/'file/hello',success=False)
        self.assertIn('errno=20',result.stderr);self.assertEqual((self.physical/'file').read_text(),'keep')

    def test_unexpected_alias_target(self):
        (self.root/'var').unlink();(self.root/'var').symlink_to(self.outside,target_is_directory=True)
        self.create(success=False);self.assertEqual(list(self.outside.iterdir()),[])

    def test_alias_owner(self):self.create(action='alias-owner',success=False)

    def test_writable_system_directories(self):
        for directory in [self.root,self.root/'private',self.root/'private/var']:
            directory.chmod(0o775);self.create(success=False);directory.chmod(0o755)

    def test_alias_replacement(self):self.create(action='replace-alias',success=False)

    def test_system_directory_replacement(self):self.create(action='replace-system-directory',success=False)

    def test_alias_change_after_plan_cannot_redirect_publication(self):
        self.create(action='alias-after-plan')
        self.assertTrue((self.physical/'hello/void.project.toml').is_file())
        self.assertEqual(list(self.outside.iterdir()),[])

    def test_linked_physical_system_directory(self):
        (self.root/'private').rename(self.root/'saved-private')
        (self.root/'private').symlink_to(self.root/'saved-private',target_is_directory=True)
        self.create(success=False)

    def test_user_symlink(self):
        (self.physical/'link').symlink_to(self.outside,target_is_directory=True)
        self.create(self.raw/'link/hello',success=False);self.assertEqual(list(self.outside.iterdir()),[])

    def test_user_ancestor_replacement(self):
        self.create(action='replace-user-ancestor',success=False);self.assertEqual(list(self.outside.iterdir()),[])

    def test_parent_replacement_after_admission(self):
        result=self.create(action='replace-parent-after-admission',success=False)
        self.assertIn('changed after review',result.stderr)
        self.assertEqual(list(self.physical.iterdir()),[])
        self.assertEqual(list((self.physical.parent/'retired-workspace').iterdir()),[])

    def test_wrong_case_ancestor(self):self.create(self.root/'var/WORKSPACE/hello',success=False)

    def test_system_alias_case_collision(self):
        (self.root/'VAR').mkdir();self.create(success=False)

    def test_parent_reference_rejected(self):
        self.create(self.raw/'..'/'escape',success=False)
        self.assertFalse((self.physical.parent/'escape').exists())

    def test_literal_prefix_boundary(self):
        (self.root/'various').symlink_to(self.physical,target_is_directory=True)
        self.create(self.root/'various/hello',success=False)

    def test_missing_and_non_directory_system_target(self):
        (self.root/'private/var').rename(self.root/'saved-var')
        missing=self.create(success=False)
        # Existing Parent checks exact directory entries before openat. An
        # absent component retains that existing admission diagnostic.
        self.assertIn('wrong case or a case collision',missing.stderr)
        self.assertFalse((self.root/'private/var').exists())
        (self.root/'private/var').write_text('keep target')
        wrong_type=self.create(success=False);self.assertIn('errno=20',wrong_type.stderr)
        self.assertEqual((self.root/'private/var').read_text(),'keep target')

    def test_late_collision_preserves_winner(self):
        self.create(action='late-collision',success=False)
        self.assertEqual((self.physical/'hello/external').read_text(),'keep winner')
        self.assertEqual([x.name for x in self.physical.iterdir()],['hello'])

    def test_verification_failure_rolls_back(self):
        self.create(action='rollback',success=False);self.assertEqual(list(self.physical.iterdir()),[])

    def test_foreign_content_is_retained(self):
        self.create(action='foreign',success=False)
        stages=list(self.physical.iterdir());self.assertEqual(len(stages),1)
        self.assertEqual((stages[0]/'foreign').read_text(),'keep foreign bytes')
        self.assertEqual([x.name for x in stages[0].iterdir()],['foreign'])

    def test_modified_starter_not_published(self):
        self.create(action='tamper',success=False);self.assertEqual(list(self.physical.iterdir()),[])

    def test_replaced_stage_not_removed(self):
        self.create(action='replace-stage',success=False)
        self.assertFalse((self.physical/'hello').exists())
        external=[p/'external' for p in self.physical.iterdir() if (p/'external').exists()]
        self.assertEqual(len(external),1);self.assertEqual(external[0].read_text(),'keep replacement')

if __name__=='__main__':unittest.main(argv=[sys.argv[0]],verbosity=2)
