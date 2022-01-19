@echo off
REM # Compile for the web using Emscripten.
REM # Assumes that emsdk is on you PATH.
REM # https://github.com/raysan5/raylib/wiki/Working-for-Web-(HTML5)

REM # Config
set @input=src/main.c src/game.c src/lib/raygui.c
set @output=bin/web/index.html

REM # Setup emsdk crap
call emsdk update
call emsdk install latest
call emsdk activate latest

REM # Compile and link
call emcc -o %@output% -Os -Wall -L./lib -lraylib -s USE_GLFW=3 -s TOTAL_MEMORY=134217728 --shell-file bin/web/shell.html %@input%

REM # Run
call emrun %@output%