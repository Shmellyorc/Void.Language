from pathlib import Path
import hashlib, json, re, subprocess, time

share = Path(r'\\vboxsvr\VM\g4-conventional\diagnostic')
local = Path.home() / 'VoidS7-G4Conventional'
sdk = (Path.home() / 'VoidCI42377496702-Native/.toolchains/llvm-23.1.4/bin').resolve(strict=True)
for _ in range(120):
    if json.loads((share/'overlay.json').read_text())['status'] == 'restored':
        break
    time.sleep(2)
else:
    raise RuntimeError('Qualification overlay still active')
for profile in ('Release', 'Debug'):
    result = json.loads((share/(profile+'-result.json')).read_text())
    if result['status'] != 'PASS':
        continue
    log = (share/(profile+'-LastTest.log')).read_bytes().decode('utf-8', errors='replace')
    match = re.search(r'^([0-9a-f]{64})  (C:[^\r\n]+\\relocated optimized [^\r\n]+\\application\.exe)\r?$', log, re.M)
    assert match, profile
    program = Path(match[2])
    assert program.is_relative_to(local) and hashlib.sha256(program.read_bytes()).hexdigest()==match[1]
    native=program.parent/'native'
    output=share/(profile+'-native-images.txt')
    with output.open('wb') as stream:
        for name in ('dependency','provider','adapter'):
            path=native/f'void_g4_conventional_{name}.dll'
            header=subprocess.check_output([sdk/'llvm-readobj.exe','--file-headers','--coff-imports','--coff-exports',path])
            stream.write(header)
            if name != 'provider':
                continue
            text=header.decode('utf-8',errors='replace')
            base=int(re.search(r'ImageBase: (0x[0-9A-Fa-f]+)', text)[1],16)
            rva=int(re.search(r'Name: g4_conventional_sample\s+RVA: (0x[0-9A-Fa-f]+)',text)[1],16)
            address=base+rva
            stream.write(subprocess.check_output([sdk/'llvm-objdump.exe','-d',f'--start-address={address}',
                                                 f'--stop-address={address+70}',path]))
