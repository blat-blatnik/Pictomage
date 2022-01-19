#include "lib/raylib.h"

#ifdef __EMSCRIPTEN__
#   include <emscripten/emscripten.h>
#endif

void game_init(void);
void game_loop_one_iteration(void);

int main(void)
{
    game_init();

    #ifndef __EMSCRIPTEN__
    while (!WindowShouldClose())
        game_loop_one_iteration();
    #else
    emscripten_set_main_loop(game_loop_one_iteration, 0, 1);
    #endif
}