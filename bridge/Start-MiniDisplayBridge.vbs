' =====================================================================
'  Lanceur silencieux de MiniDisplay Bridge (aucune console).
' =====================================================================
Option Explicit

Dim sh, fso, baseDir, ps1, psExe, cmd

Set sh  = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

baseDir = fso.GetParentFolderName(WScript.ScriptFullName)
ps1     = fso.BuildPath(baseDir, "MiniDisplayBridge.ps1")

If Not fso.FileExists(ps1) Then
    MsgBox "Fichier introuvable :" & vbCrLf & ps1, vbCritical, "MiniDisplay Bridge"
    WScript.Quit 1
End If

psExe = fso.BuildPath(sh.ExpandEnvironmentStrings("%SystemRoot%"), "System32\WindowsPowerShell\v1.0\powershell.exe")
If Not fso.FileExists(psExe) Then psExe = "powershell.exe"

cmd = """" & psExe & """ -NoProfile -NonInteractive -ExecutionPolicy Bypass " & _
      "-WindowStyle Hidden -File """ & ps1 & """"

sh.Run cmd, 0, False
