@echo off
rem Builds a 64-bit OpenLara.exe here with Visual Studio (Community 18), the OpenGL renderer, the static CRT
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > nul || exit /b 1
cd /d %~dp0
rc /nologo /fo OpenLara.res OpenLara.rc || exit /b 1
cl /nologo /O2 /Oi /GL /MT /EHs-c- /GR- /W1 /utf-8 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DWIN32 /DNDEBUG /I..\..\libs /I..\.. main.cpp ..\..\libs\stb_vorbis\stb_vorbis.c ..\..\libs\minimp3\minimp3.cpp ..\..\libs\tinf\tinflate.c OpenLara.res /FeOpenLara.exe /link /LTCG /SUBSYSTEM:WINDOWS opengl32.lib winmm.lib wsock32.lib gdi32.lib user32.lib kernel32.lib shell32.lib ole32.lib advapi32.lib || exit /b 1
echo BUILD OK
