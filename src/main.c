#define RAYGUI_IMPLEMENTATION
#include "lib/raygui.h"
#define RMEM_IMPLEMENTATION
#include "lib/rmem.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Run on a dedicated GPU if both a dedicated and integrated one are avaliable.
// See: https://stackoverflow.com/a/39047129
#if defined _MSC_VER
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

void BasicInit(void);
void GameInit(void);
void GameLoopOneIteration(void);

// If we are compiling for Windows in release mode, we don't want a command prompt 
// to pop up. To prevent that, our entry point needs to be WinMain instead of main.\
// See: https://stackoverflow.com/a/18709447
#if defined _MSC_VER && !defined _DEBUG
int __stdcall WinMain(void *instance, void *prevInstance, char *cmdLine, int showCmd)
#else
int main(void)
#endif
{
    BasicInit();
    GameInit();

    // On the web, the browser wants to control the main loop, so on that target
    // we move control over to it and it will call our main loop when it wants to.
    // See: https://emscripten.org/docs/porting/emscripten-runtime-environment.html#browser-main-loop
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(GameLoopOneIteration, 0, 1);
    #else
    while (!WindowShouldClose())
        GameLoopOneIteration();
    #endif
}