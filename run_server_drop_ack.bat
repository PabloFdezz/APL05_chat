@echo off
REM Servidor con 50%% de perdida simulada de ACKs para probar reenvios.
bin\server.exe 40000 40001 50
pause
