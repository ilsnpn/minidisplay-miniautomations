# Arrete le pont (necessaire avant de televerser un firmware : il occupe le port COM).
$procs = Get-CimInstance Win32_Process -Filter "Name='powershell.exe' OR Name='pwsh.exe'" |
    Where-Object { $_.CommandLine -like '*MiniDisplayBridge.ps1*' }
foreach ($p in $procs) { Stop-Process -Id $p.ProcessId -Force; Write-Host "Pont arrete (PID $($p.ProcessId))" }
if (-not $procs) { Write-Host 'Aucun pont en cours.' }
