call windres Pictomage.rc -O coff -o bin/windows/Pictomage.res
call gcc -O2 -L./lib src/*.c bin/windows/Pictomage.res -lraylib_windows_x64 -lwinmm -lgdi32 -o bin/Windows/Pictomage.exe