"""Minimal explicitly reviewed SQLite owner, reusing G3/G4 and the D4 catalog."""
from pathlib import Path
from hashlib import sha256
import json,os,shutil,subprocess
HEADER_SHA='919e7f2e8ed1d8f56ac17b412b8971c76aa5d1a879752cc6058f75e7d5910e1d'
SOURCE_SHA='b1dd5d74ec7f29055a6684fa06fb3c2f6821c87dd38f9a458dfd2e8a1db28189'
WRAPPER=r'''
#include "imported.h"
#include <stdlib.h>
#include <stdio.h>
struct G25Database { sqlite3* db; };
G25Database* g25_sql_open(void) {
    G25Database* owner=(G25Database*)calloc(1,sizeof(*owner));
    if (!owner) return NULL;
    if (sqlite3_open(":memory:",&owner->db)!=SQLITE_OK) { sqlite3_close(owner->db);free(owner);return NULL; }
    return owner;
}
int g25_sql_query(const G25Database* owner) {
    sqlite3_stmt* statement=NULL;int result=-1;
    if (sqlite3_exec(owner->db,"CREATE TABLE sample(n INTEGER); INSERT INTO sample VALUES(20),(22);",NULL,NULL,NULL)!=SQLITE_OK) return -2;
    if (sqlite3_prepare_v2(owner->db,"SELECT sum(n) FROM sample",-1,&statement,NULL)==SQLITE_OK && sqlite3_step(statement)==SQLITE_ROW)
        result=sqlite3_column_int(statement,0);
    sqlite3_finalize(statement);return result;
}
int g25_sql_close(G25Database* owner) {
    int status=sqlite3_close_v2(owner->db);free(owner);
    puts("G25 SQLite owner released");return status;
}
'''
def qualify(base,sqlite,compiler,clang,flags,env,run,generate,execute):
 assert sha256((sqlite/'sqlite3.h').read_bytes()).hexdigest()==HEADER_SHA
 assert sha256((sqlite/'sqlite3.c').read_bytes()).hexdigest()==SOURCE_SHA
 project=base/'sqlite';run(compiler,'new',project);out=project/'src/sqlite'
 coverage=generate(sqlite/'sqlite3.h',out,'org.voidlang.g25.sqlite','/src/sqlite/native.void')
 assert coverage['generated_functions']>=10,coverage['counts']
 for name in ['sqlite3_open','sqlite3_exec','sqlite3_close']:
  assert any(row['name']==name and row['category']=='reviewed_adapter_required' and 'pointer' in row['reason'] for row in coverage['declarations']),name
 # This handwritten fixture is the ownership decision. The importer did not guess it.
 descriptor=out/'provider.descriptor';text=descriptor.read_text();at=min(text.index('bind '),text.find('record ') if 'record ' in text else len(text))
 text=text[:at]+'resource database G25Database pointer null open_db close_db owner-calls shutdown-any consume-always\n'+text[at:]
 text+='bind open_db g25_sql_open owner:database\nbind query g25_sql_query c_int borrow:database\nbind close_db g25_sql_close c_int close:database\n'
 # Bind the reviewed adapter bytes into the existing checked descriptor identity.
 marker=text.index('thunk ')+6;text=text[:marker]+sha256((text+WRAPPER).encode()).hexdigest()+text[marker+64:];descriptor.write_bytes(text.encode())
 with (out/'imported.h').open('a') as f:f.write('\ntypedef struct G25Database G25Database;\nVDI_EXPORT G25Database* g25_sql_open(void);\nVDI_EXPORT int g25_sql_query(const G25Database*);\nVDI_EXPORT int g25_sql_close(G25Database*);\n')
 (out/'reviewed.c').write_text(WRAPPER);(out/'native.void').unlink()
 run(compiler,'native-bind',descriptor,'imported.h','-o',out/'native.void')
 run(compiler,'native-bind',descriptor,'imported.h','-o',out/'checked.c')
 windows=os.name=='nt';catalog=base/'approved-sqlite';catalog.mkdir();library='sqlite_provider.dll' if windows else 'libsqlite_provider.so'
 obj=base/('sqlite3.obj' if windows else 'sqlite3.o')
 compile_flags=[f for f in flags if f not in ('-shared','-fuse-ld=lld')]
 run(clang,'-std=c11',*compile_flags,'-DSQLITE_THREADSAFE=1','-DSQLITE_OMIT_LOAD_EXTENSION=1','-c',sqlite/'sqlite3.c','-o',obj)
 run(clang,'-std=c11',*flags,'-I'+str(out),out/'imports.c',out/'reviewed.c',out/'checked.c',obj,'-o',catalog/library)
 shutil.copyfile(descriptor,catalog/'provider.descriptor');(catalog/'SQLITE-NOTICE.txt').write_text('SQLite 3.53.4, public domain. https://www.sqlite.org/copyright.html\n')
 def identity(name):
  p=catalog/name;return f'{p.stat().st_size}:{sha256(p.read_bytes()).hexdigest()}'
 link_input='\"sqlite_provider.lib\"=\"'+identity('sqlite_provider.lib')+'\"' if windows else ''
 target='x86_64-pc-windows-msvc' if windows else 'x86_64-unknown-linux-gnu'
 (catalog/'void.native.toml').write_text(f'''format=1
[native]
id="org.voidlang.g25.sqlite"
version="3.53.4"
abi=3
target="{target}"
revision="g25-reviewed-sqlite-3530400"
descriptor="provider.descriptor"
descriptor-identity="{identity('provider.descriptor')}"
library="{library}"
adapter="{library}"
[native.runtime]
"{library}"="{identity(library)}"
[native.link]
{link_input}
[native.notices]
"SQLITE-NOTICE.txt"="{identity('SQLITE-NOTICE.txt')}"
''')
 manifest=project/'void.project.toml';manifest.write_text(manifest.read_text()+'''\n[native-requirements.sqlite]
id="org.voidlang.g25.sqlite"
version="=3.53.4"
abi=3
''')
 env['VOID_NATIVE_CATALOG']=str(catalog)
 main=next((project/'src').glob('*.void'));main.write_text('''use "./sqlite/api.void" as sql
use "./sqlite/native.void" as owned
func query_database() -> i32:
    var database = owned.open_db() as owned.database
    defer owned.close_db(database)
    return owned.query(database)
func main() -> i32:
    if sql.sqlite3_libversion_number() != 3053004:
        return 2
    var result = query_database()
    if result != 42:
        return 3
    if sql.sqlite3_shutdown() != 0:
        return 4
    print("G25 SQLITE:", result)
    return 0
''')
 run(compiler,'resolve','--project',project);run(compiler,'resolve','--locked','--project',project);run(compiler,'deps','explain','--project',project,'--json')
 execute(project,base/'sqlite-exec','G25 SQLITE: 42')
 original_main=main.read_text()
 main.write_text('''use "./sqlite/api.void" as sql
use "./sqlite/native.void" as owned
func main() -> i32:
    var database = owned.open_db() as owned.database
    defer owned.close_db(database)
    var values = [1, 2]
    print(values[sql.sqlite3_libversion_number()])
    return 0
''')
 run(compiler,'check','--project',project)
 for profile in ['debug','release']:
  run(compiler,'run','--project',project,'--allow-native','--profile',profile,status=1,text='G25 SQLite owner released')
  image=base/('sqlite-failure-'+profile)/('app.exe' if windows else 'app');run(compiler,'build','--project',project,'--allow-native','--profile',profile,'-o',image);run(image,status=1,text='G25 SQLite owner released')
 main.write_text(original_main)
 if os.environ.get('VOID_SQLITE_IMPORT_EVIDENCE'):
  evidence=Path(os.environ['VOID_SQLITE_IMPORT_EVIDENCE']);evidence.mkdir();shutil.copytree(project,evidence/'project');shutil.copytree(catalog,evidence/'catalog')
 print('G25 pinned SQLite 3.53.4 import, reviewed open/execute/result/close, D4 locked catalog, normal/O2 JIT/AOT and checked-failure owner cleanup passed',flush=True)
