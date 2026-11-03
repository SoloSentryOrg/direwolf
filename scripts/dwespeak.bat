echo off 

if "%~1"=="" exit /b 2
"C:\Program Files (x86)\eSpeak\command_line\espeak.exe" -v en-sc --stdin
exit /b %errorlevel%
