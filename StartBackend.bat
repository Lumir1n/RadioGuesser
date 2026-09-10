@echo off
title RadioGuesser Backend
echo Starting RadioGuesser backend...
cd /d "c:\Users\Lumirin\Documents\Unreal Projects\RadioGuesser\Server\Radioguesser.Server"
set ASPNETCORE_ENVIRONMENT=Development
dotnet run --launch-profile http
pause
