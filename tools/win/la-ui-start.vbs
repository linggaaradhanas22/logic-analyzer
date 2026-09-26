' Starts the unified Logic Analyzer UI server without a console window and
' opens the workspace page. Double-clicking again while it runs just reopens
' the page. Optional first argument: "nobrowser" (used by smoke tests).
'
' Run: wscript.exe tools\win\la-ui-start.vbs

Option Explicit

Dim shell, fso, root, launcher, pythonw, url, quiet, i
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

root = fso.GetParentFolderName(fso.GetParentFolderName( _
       fso.GetParentFolderName(WScript.ScriptFullName)))
launcher = root & "\.venv\Scripts\la-ui.exe"
pythonw = root & "\.venv\Scripts\pythonw.exe"
url = "http://127.0.0.1:8080/"

quiet = False
If WScript.Arguments.Count > 0 Then
    If LCase(WScript.Arguments(0)) = "nobrowser" Then quiet = True
End If

If Listening() Then
    If Not quiet Then OpenUrl()
    WScript.Quit 0
End If

If fso.FileExists(launcher) Then
    shell.CurrentDirectory = root
    shell.Run Quote(launcher), 0, False
ElseIf fso.FileExists(pythonw) Then
    shell.CurrentDirectory = root
    shell.Run Quote(pythonw) & " -c " & _
              Quote("import sys; from host.la_cli.commands import ui_main; sys.exit(ui_main())"), _
              0, False
Else
    MsgBox "The project .venv is missing." & vbCrLf & vbCrLf & _
           "Run tools\win\setup.ps1 -InstallPackages or follow" & vbCrLf & _
           "docs\windows-dev.md, then try again.", _
           vbCritical, "Logic Analyzer UI"
    WScript.Quit 1
End If

For i = 1 To 50
    If Listening() Then Exit For
    WScript.Sleep 200
Next

If Not Listening() Then
    MsgBox "The UI server did not start on port 8080." & vbCrLf & vbCrLf & _
           "Start it once in PowerShell (.venv\Scripts\la-ui.exe) to read the" & vbCrLf & _
           "error message, then try this shortcut again.", _
           vbExclamation, "Logic Analyzer UI"
    WScript.Quit 1
End If

If Not quiet Then OpenUrl()

Sub OpenUrl()
    shell.Run url, 1, False
End Sub

Function Listening()
    Dim exec, line
    Listening = False
    On Error Resume Next
    Set exec = shell.Exec("%comspec% /c netstat -ano -p tcp")
    If Err.Number <> 0 Then
        On Error GoTo 0
        Exit Function
    End If
    On Error GoTo 0
    Do While Not exec.StdOut.AtEndOfStream
        line = exec.StdOut.ReadLine
        If InStr(line, ":8080 ") > 0 And InStr(line, "LISTENING") > 0 Then
            Listening = True
            Exit Do
        End If
    Loop
End Function

Function Quote(value)
    Quote = Chr(34) & value & Chr(34)
End Function
