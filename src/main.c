#ifdef __EMSCRIPTEN__
#   include <emscripten/emscripten.h>
#endif

#define RAYGUI_IMPLEMENTATION
#include "lib/raygui.h"
#define RMEM_IMPLEMENTATION
#include "lib/rmem.h"
#define PHYSAC_IMPLEMENTATION
#include "lib/physac.h"

void BasicInit(void);
void GameInit(void);
void GameLoopOneIteration(void);

int main(void)
{
    BasicInit();
    GameInit();

    #ifndef __EMSCRIPTEN__
    while (!WindowShouldClose())
        GameLoopOneIteration();
    #else
    emscripten_set_main_loop(GameLoopOneIteration, 0, 1);
    #endif
}