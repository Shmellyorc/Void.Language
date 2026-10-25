import ast
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import resource
import subprocess
import sys
import tempfile
from unittest.mock import patch
import yaml

ROOT = Path.cwd()
DEST = ROOT / 'docs/stage10/engineering/native-stack-ci1'
checks = []

def check(name, condition):
    assert condition, name
    checks.append(name)
    print('PASS:', name, flush=True)

def invoke(argv, **kwargs):
    result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    print(json.dumps({'argv': argv, 'exit_code': result.returncode}), flush=True)
    print(result.stdout.decode(errors='backslashreplace'), end='', flush=True)
    return result

helper = ROOT / 'tools/ci/stage10_native_stack.py'
launcher = ROOT / 'tools/ci/stage10_macos_stack.sh'
ast.parse(helper.read_text())
check('Python syntax', True)
check('Bash syntax', invoke(['/bin/bash', '-n', str(launcher)]).returncode == 0)
check('actionlint 1.7.7', invoke(['/tmp/void-actionlint-1.7.7/actionlint', '.github/workflows/stage10-native-stack.yml']).returncode == 0)
workflow = yaml.load((ROOT / '.github/workflows/stage10-native-stack.yml').read_text(), Loader=yaml.BaseLoader)
check('manual-only trigger, macOS-only default, all option', list(workflow['on']) == ['workflow_dispatch'] and workflow['on']['workflow_dispatch']['inputs']['platforms']['default'] == 'macos-only' and workflow['on']['workflow_dispatch']['inputs']['platforms']['options'] == ['macos-only', 'all'])
job = workflow['jobs']['stack']
matrix = job['strategy']['matrix']
expression = matrix['os']
import re
choices = re.findall(r"'([\[].*?[\]])'", expression)
for selected, expected in [('macos-only', 2), ('all', 4)]:
    systems = json.loads(choices[0 if selected == 'macos-only' else 1])
    entries = [(system, config) for system in systems for config in matrix['configuration']]
    check(selected + ' matrix', len(entries) == expected and set(matrix['configuration']) == {'Debug', 'Release'} and (selected != 'macos-only' or systems == ['macos-latest']))
check('360 minutes and always-upload preserved', job['timeout-minutes'] == '360' and job['steps'][-1]['if'] == 'always()')

with tempfile.TemporaryDirectory(prefix='void-ci1-validation-') as tmp:
    tmp = Path(tmp)
    source = tmp / 'limits.c'
    source.write_text('#include <sys/resource.h>\n#include <stdio.h>\nint main(void) { struct rlimit r; if(getrlimit(RLIMIT_STACK,&r))return 2; printf("native soft=%llu hard=%llu\\n",(unsigned long long)r.rlim_cur,(unsigned long long)r.rlim_max); return !(r.rlim_cur==1048576 && r.rlim_max==1048576); }\n')
    native = tmp / 'limits'
    check('Linux native readback fixture compiled', invoke(['cc', str(source), '-o', str(native)]).returncode == 0)
    code = 'import resource,subprocess,sys; print("Python inherited",resource.getrlimit(resource.RLIMIT_STACK),flush=True); assert resource.getrlimit(resource.RLIMIT_STACK)==(1048576,1048576); sys.exit(subprocess.call([sys.argv[1]]))'
    check('real Linux shell -> Python -> native exec inherits both 1048576-byte limits', invoke(['/bin/bash', str(launcher), sys.executable, '-c', code, str(native)]).returncode == 0)
    def too_small():
        resource.setrlimit(resource.RLIMIT_STACK, (524288,524288))
    result = invoke(['/bin/bash', str(launcher), '/bin/echo', 'MUST-NOT-EXECUTE'], preexec_fn=too_small)
    check('unattainable limit fails before child execution', result.returncode != 0 and b'MUST-NOT-EXECUTE' not in result.stdout)
    check('launcher preserves child exit 17', invoke(['/bin/bash', str(launcher), sys.executable, '-c', 'raise SystemExit(17)']).returncode == 17)

    spec = importlib.util.spec_from_file_location('stack', helper)
    m = importlib.util.module_from_spec(spec)
    # Do not generate repository __pycache__ files.
    exec(compile(helper.read_bytes(), str(helper), 'exec'), m.__dict__)
    m.EVIDENCE = tmp / 'evidence'
    m.BUILD = tmp / 'build'
    m.BUILD.mkdir()
    m.CONFIGURATION = 'Debug'
    m.SYSTEM = 'Darwin'  # Logic simulation on Linux, never a native macOS result.
    for target in ('voidc','void_tests'):
        (m.BUILD / target).write_bytes(b'fixture')
    def macho_output(text):
        def fake_run(label, argv):
            m.EVIDENCE.mkdir(exist_ok=True)
            (m.EVIDENCE / (label + '.log')).write_text(text)
        return fake_run
    good = 'Load command 0\n       cmd LC_MAIN\n   cmdsize 24\n  entryoff 1234\n stacksize 0\n'
    for label, text, expected in [('default',good,True), ('override',good.replace('stacksize 0','stacksize 16777216'),False), ('missing','Load command 0\n cmd LC_SEGMENT_64\n',False), ('duplicate',good+good,False)]:
        passed = True
        try:
            with patch.object(m,'run',macho_output(text)):
                m.verify_macho()
        except RuntimeError:
            passed = False
        check('Mach-O fixture ' + label, passed == expected)
    with patch.object(m,'run',macho_output(good)):
        m.verify_macho()
    with patch.object(resource,'setrlimit',side_effect=AssertionError('must never change inside Python')):
        for observed, accepted in [((1048576,1048576),True),((1048576,8388608),False),((8388608,8388608),False)]:
            passed = True
            try:
                with patch.object(resource,'getrlimit',return_value=observed):
                    m.stack_boundary('frontend')
            except RuntimeError:
                passed = False
            check('readback only ' + str(observed), passed == accepted)
    calls = []
    with patch.object(m,'run',side_effect=lambda label,argv: calls.append((label,argv))):
        m.main('frontend')
        m.main('depth')
    check('Darwin parent routes both original phases through native shell', [c[0] for c in calls] == ['frontend-launcher','semantic-depth-launcher'] and all(c[1][:2] == ['/bin/bash','tools/ci/stage10_macos_stack.sh'] for c in calls))
    calls = []
    with patch.object(m,'run',side_effect=lambda label,argv: calls.append((label,argv))), patch.object(resource,'getrlimit',return_value=(1048576,1048576)):
        m.main('frontend-inherited')
        m.main('depth-inherited')
    check('unchanged original frontend/depth arguments after inherited gate', calls[0][1][:8] == ['ctest','--test-dir',str(m.BUILD),'-R','^frontend$','--no-tests=error','--output-on-failure','--output-junit'] and calls[1][1] == [sys.executable,'tests/integration/semantic_depth.py',str(m.BUILD/'voidc')])
    calls = []
    try:
        with patch.object(m,'run',side_effect=lambda *args: calls.append(args)), patch.object(resource,'getrlimit',return_value=(8388608,8388608)):
            m.main('frontend-inherited')
    except RuntimeError:
        pass
    check('wrong inherited limits block test scheduling', not calls)
    try:
        m.run('raw-failure',[sys.executable,'-c','import os; os.write(1,bytes([255,10])); raise SystemExit(17)'])
    except subprocess.CalledProcessError as error:
        check('raw command bytes and exit preserved', error.returncode == 17 and (m.EVIDENCE/'raw-failure.log').read_bytes() == bytes([255,10]) and json.loads((m.EVIDENCE/'raw-failure.json').read_text())['exit_code'] == 17)
    # An incomplete run may never acquire acceptance from successful format fixtures.
    with patch.object(m,'sources',return_value={}):
        m.save('source-before.json',{})
        try:
            m.finish()
        except RuntimeError:
            pass
        check('incomplete evidence fails honestly', not json.loads((m.EVIDENCE/'summary.json').read_text())['stack_gates_complete'])

baseline = 'b490579f84a702ec5d11da51a82934a498f88afa'
changed = subprocess.check_output(['git','diff','--name-only',baseline,'--'],text=True).splitlines()
check('tracked scope limited to targeted workflow/helper/handoff', set(changed) == {'.github/workflows/stage10-native-stack.yml','tools/ci/stage10_native_stack.py','docs/stage10/engineering/targeted-native-stack/HANDOFF.md'})
record = {'host':platform.platform(),'python':sys.version,'checks':checks,'passed':len(checks),'native_macos_execution':False,'hosted_dispatch':False,'scope':'Local harness logic, Linux inheritance and fixtures only; no native macOS/Windows acceptance'}
(DEST/'validation.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
