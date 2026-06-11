@echo off
setlocal

if not exist bin mkdir bin

echo Compilando servidor con MSVC...
cl /EHsc /std:c++17 /I common common\protocol.cpp server\server.cpp /Fe:bin\server.exe ws2_32.lib
if errorlevel 1 goto error

echo Compilando cliente con MSVC...
cl /EHsc /std:c++17 /I common common\protocol.cpp client\client.cpp /Fe:bin\client.exe ws2_32.lib
if errorlevel 1 goto error

echo.
echo Compilacion completada correctamente.
echo Nota: este .bat debe ejecutarse desde "Developer Command Prompt for Visual Studio".
goto end

:error
echo.
echo Hubo errores de compilacion.

:end
endlocal
