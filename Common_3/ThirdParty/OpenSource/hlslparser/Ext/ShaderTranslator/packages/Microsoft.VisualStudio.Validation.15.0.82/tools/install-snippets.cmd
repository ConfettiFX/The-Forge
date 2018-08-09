@ECHO OFF 

setlocal ENABLEEXTENSIONS
set KEY_NAME="HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders"
set VALUE_NAME=Personal

FOR /F "usebackq skip=2 tokens=1-3" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO (
    set ValueName=%%A
    set ValueType=%%B
    set ValueValue=%%C
)

if defined ValueName (
    SET Docs=%ValueValue%
) else (
    @echo Could not determine location of "My Documents" folder.
)

FOR %%V IN (2010 2012 2013) DO (
    IF EXIST "%DOCS%\Visual Studio %%V" (
        ECHO Installing snippets for Visual Studio %%V...
        ROBOCOPY /NJH /NJS /NDL /NFL "%~dp02013" "%DOCS%\Visual Studio %%V\Code Snippets\Visual C#\My Code Snippets" *.snippet
    )
)

FOR %%V IN (2015) DO (
    IF EXIST "%DOCS%\Visual Studio %%V" (
        ECHO Installing snippets for Visual Studio %%V...
        ROBOCOPY /NJH /NJS /NDL /NFL "%~dp02015" "%DOCS%\Visual Studio %%V\Code Snippets\Visual C#\My Code Snippets" *.snippet
    )
)
