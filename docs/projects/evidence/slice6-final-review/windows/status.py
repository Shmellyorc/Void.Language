from pathlib import Path
import json, shutil, subprocess
home=Path.home()
share=Path(r'\\vboxsvr\VM\project-slice6-final-review')
cmd=("Get-CimInstance Win32_Process | Where-Object { "
     "$_.Name -in 'python.exe','cmake.exe','ninja.exe','ctest.exe' -and "
     "($_.CommandLine -like '*project-slice6-final-review*native.py*' -or $_.CommandLine -like '*VoidS6-Storage*') } "
     "| Select-Object ProcessId,Name | ConvertTo-Json -Compress")
proc=subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-Command',cmd],capture_output=True,text=True,check=True)
running=json.loads(proc.stdout) if proc.stdout.strip() else []
backup=home/'VoidS6-Storage-backup'
state={'active_native_processes':running,'backup_exists':backup.exists(),
       'backup_files':sum(p.is_file() for p in backup.rglob('*')) if backup.exists() else 0,
       'source_packet_exists':(home/'VoidS6-Storage-source').exists(),'free_bytes':shutil.disk_usage(home).free}
(share/'launch-status.json').write_text(json.dumps(state,indent=2)+'\n')
