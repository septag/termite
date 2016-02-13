#include "stengine/core.h"
#include "bxx/logger.h"
#include "bxx/path.h"

#include <SDL2/SDL.h>
#undef main

SDL_Window* gWindow = nullptr;

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static bool sdlPollEvents()
{
    SDL_Event e;
    SDL_PollEvent(&e);
    if (e.type == SDL_QUIT)
        return false;

    return true;
}

int main(int argc, char* argv[])
{
    bx::enableLogToFileHandle(stdout, stderr);

    if (SDL_Init(0) != 0) {
        BX_FATAL("SDL Init failed");
        return -1;
    }

    st::coreConfig conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    gWindow = SDL_CreateWindow("stTestSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                               WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!gWindow) {
        BX_FATAL("SDL window creation failed");
        st::coreShutdown();
        return -1;
    }
    conf.sdlWindow = gWindow;
    conf.gfxWidth = WINDOW_WIDTH;
    conf.gfxHeight = WINDOW_HEIGHT;

    if (st::coreInit(conf, nullptr)) {
        BX_FATAL(st::errGetString());
        BX_VERBOSE(st::errGetCallstack());
        st::coreShutdown();
        SDL_DestroyWindow(gWindow);
        SDL_Quit();
        return -1;
    }    
   
    while (sdlPollEvents()) {
        st::coreFrame();
    }

    st::coreShutdown();
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}

