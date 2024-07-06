@echo off

taskkill /f /im httpd.exe
taskkill /f /t /im DarkHTTPServer.exe
taskkill /f /im perl.exe
taskkill /f /im httpd.exe
taskkill /f /t /im DarkHTTPServer.exe
exit /b 0
