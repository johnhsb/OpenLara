@echo off
rem Builds a 64-bit OpenLara.exe here with Visual Studio (Community 18), the OpenGL renderer, the static CRT
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul || exit /b 1
cd /d %~dp0
rc /nologo /fo OpenLara.res OpenLara.rc || exit /b 1
cl /nologo /O2 /Oi /GL /MT /Zi /EHs-c- /GR- /W1 /utf-8 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DWIN32 /DNDEBUG /I..\..\libs /I..\.. main.cpp ..\..\libs\stb_vorbis\stb_vorbis.c ..\..\libs\minimp3\minimp3.cpp ..\..\libs\tinf\tinflate.c OpenLara.res /FeOpenLara.exe /link /LTCG /DEBUG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS opengl32.lib winmm.lib wsock32.lib gdi32.lib user32.lib kernel32.lib shell32.lib ole32.lib advapi32.lib || exit /b 1
rem signs it with the certificate "OpenLara Local Build" of the current user when it is there (a PC that trusts it runs it with no warning)
signtool sign /q /n "OpenLara Local Build" /s My /fd SHA256 OpenLara.exe > nul 2>&1 && echo SIGNED || echo NOT SIGNED
echo BUILD OK
