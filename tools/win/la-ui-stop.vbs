' Stops the unified Logic Analyzer UI server started by la-ui-start.vbs: the
' Python process listening on port 8080 is terminated. Other programs holding
' port 8080 are reported, never killed. Optional first argument: "quiet"
' (prints instead of a dialog, used by smoke tests).
'
' Run: wscript.exe tools\win\la-ui-stop.vbs

Option Explicit

Dim shell, procId, procName, quiet, exec
Set shell = CreateObject("WScript.Shell")

quiet = False
If WScript.Arguments.Count > 0 Then
    If LCase(WScript.Arguments(0)) = "quiet" Then quiet = True
End If

procId = ListenerPid()
If procId = "" Then
    Say "The Logic Analyzer UI server is not running.", vbInformation
    WScript.Quit 0
End If

procName = ProcessName(procId)
If InStr(LCase(procName), "python") = 0 Then
    Say "Port 8080 is used by another program (" & procName & "); not stopping it.", vbExclamation
    WScript.Quit 1
End If

Set exec = shell.Exec("%comspec% /c taskkill /PID " & procId & " /T /F >nul 2>&1")
Do While exec.Status = 0
    WScript.Sleep 100
Loop

If Listening() Then
    Say "The Logic Analyzer UI server did not stop; try Task Manager.", vbExclamation
    WScript.Quit 1
End If

Say "The Logic Analyzer UI server has been stopped.", vbInformation
WScript.Quit 0

Function ListenerPid()
    Dim exec, line
    ListenerPid = ""
    Set exec = shell.Exec("%comspec% /c netstat -ano -p tcp")
    Do While Not exec.StdOut.AtEndOfStream
        line = exec.StdOut.ReadLine
        If InStr(line, ":8080 ") > 0 And InStr(line, "LISTENING") > 0 Then
            line = Trim(line)
            ListenerPid = Mid(line, InStrRev(line, " ") + 1)
            Exit Do
        End If
    Loop
End Function

Function ProcessName(procId)
    Dim exec, text, name
    ProcessName = "unknown"
    Set exec = shell.Exec("%comspec% /c tasklist /FI " & Quote("PID eq " & procId) & " /FO CSV /NH")
    text = exec.StdOut.ReadAll
    If Len(Trim(text)) > 0 Then
        name = Split(text, ",")(0)
        ProcessName = Replace(name, Chr(34), "")
    End If
End Function

Function Listening()
    Dim exec, line
    Listening = False
    Set exec = shell.Exec("%comspec% /c netstat -ano -p tcp")
    Do While Not exec.StdOut.AtEndOfStream
        line = exec.StdOut.ReadLine
        If InStr(line, ":8080 ") > 0 And InStr(line, "LISTENING") > 0 Then
            Listening = True
            Exit Do
        End If
    Loop
End Function

Sub Say(message, style)
    If quiet Then
        WScript.Echo message
    Else
        MsgBox message, style, "Logic Analyzer UI"
    End If
End Sub

Function Quote(value)
    Quote = Chr(34) & value & Chr(34)
End Function
