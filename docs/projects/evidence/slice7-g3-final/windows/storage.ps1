$ErrorActionPreference = 'Stop'
$os = Get-CimInstance Win32_OperatingSystem
$disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
$builds = Get-CimInstance Win32_Process | Where-Object { $_.Name -in 'cmake.exe','ninja.exe','clang-cl.exe','ctest.exe' } | Select-Object Name,ProcessId
[ordered]@{ captured_utc = [DateTime]::UtcNow.ToString('o'); os = $os.Caption; build = $os.BuildNumber; total_bytes = $disk.Size; free_bytes = $disk.FreeSpace; active_builds = @($builds) } | ConvertTo-Json -Depth 3 | Set-Content -Encoding ascii -LiteralPath '\\vboxsvr\VM\project-slice7-g3-final\storage.json'
