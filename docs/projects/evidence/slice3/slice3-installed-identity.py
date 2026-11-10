from pathlib import Path
import subprocess,hashlib,json,re
r=Path.cwd();b=r/'build/llvm23-release/voidc';i=r/'build/slice3-installed/compiler/bin/voidc';rows=[];parts=[];data=[]
for p in [b,i]:
 argv=[str(p),'--version'];version=subprocess.check_output(argv,text=True);dynamic=subprocess.check_output(['readelf','-d',p],text=True)
 sections={m[1]:(int(m[2],16),int(m[3],16)) for m in re.finditer(r'^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+[0-9a-f]+\s+([0-9a-f]+)\s+([0-9a-f]+)',subprocess.check_output(['readelf','-SW',p],text=True),re.M)}
 content=p.read_bytes();data.append(content);parts.append(sections);rows.append({'path':str(p),'sha256':hashlib.sha256(content).hexdigest(),'bytes':len(content),'version_argv':argv,'version':version,'runpath':[line.strip() for line in dynamic.splitlines() if 'RUNPATH' in line or 'RPATH' in line],'code_sha256':{n:hashlib.sha256(content[sections[n][0]:sum(sections[n])]).hexdigest() for n in ['.text','.rodata']}})
assert parts[0]==parts[1] and len(data[0])==len(data[1]);assert rows[0]['version']==rows[1]['version'] and rows[0]['runpath']!=rows[1]['runpath'];assert rows[0]['code_sha256']==rows[1]['code_sha256']
start=0
for offset,size in sorted(parts[0][n] for n in ['.dynamic','.dynstr']):
 assert data[0][start:offset]==data[1][start:offset];start=offset+size
assert data[0][start:]==data[1][start:]
(r/'build/slice3-installed-identity.json').write_text(json.dumps({'binaries':rows,'whole_file_equality':rows[0]['sha256']==rows[1]['sha256'],'same_build_identity':True,'same_code_sections':True,'all_changes_confined_to':['.dynamic','.dynstr']},indent=2)+'\n');print('Installed compiler identity PASS; only ELF dynamic metadata differs')
