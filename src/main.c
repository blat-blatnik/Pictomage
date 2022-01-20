#include "basic.h"

#ifdef __EMSCRIPTEN__
#   include <emscripten/emscripten.h>
#endif

void GameInit(void);
void GameLoopOneIteration(void);

int main(void)
{
    GameInit();

    #ifndef __EMSCRIPTEN__
    while (!WindowShouldClose())
        GameLoopOneIteration();
    #else
    emscripten_set_main_loop(GameLoopOneIteration, 0, 1);
    #endif
}