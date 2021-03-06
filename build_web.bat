@echo off
setlocal EnableDelayedExpansion

rem # Compile for the web using Emscripten.
rem # Assumes that emsdk is on you PATH.
rem # https://github.com/raysan5/raylib/wiki/Working-for-Web-(HTML5)

rem # Setup emsdk
rem call emsdk update
rem call emsdk install latest
call emsdk activate latest

rem # Collect all .c files for compilation into the 'input' variable.
for /f %%a in ('forfiles /s /m *.c /c "cmd /c echo @relpath"') do set input=!input! "%%~a"

rem # Compile and link
call emcc -o bin/web/index.html -Wall -L./lib -lraylib_web -s USE_GLFW=3 -s TOTAL_MEMORY=16777216 --shell-file webshell.html --preload-file res %input%

rem # Make sure .data is smaller than 32MB
set /a size=0
for /f "usebackq" %%A in ('bin/web/index.data') do set /a size=%size%+%%~zA
if %size% geq 33554432 (
	echo WARNING!
	echo .data is larger than 32MB!
)

rem # Copy the favicon
copy /y "Pictomage.ico" "bin/web/favicon.ico"

pause
rem # Run with: $ emrun bin/web/index.html