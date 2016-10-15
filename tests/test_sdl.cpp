#include <cstddef>
#ifdef _WIN32
#  define HAVE_STDINT_H 1
#endif
#include <SDL.h>
#include <SDL_syswm.h>

#include "bxx/logger.h"
#include "bxx/path.h"

#include "termite/core.h"
#include "termite/gfx_defines.h"
#include "termite/gfx_font.h"
#include "termite/gfx_vg.h"
#include "termite/resource_lib.h"
#include "termite/gfx_texture.h"
#include "termite/gfx_debugdraw.h"
#include "termite/camera.h"
#include "termite/gfx_model.h"
#include "termite/io_driver.h"
#include "termite/gfx_utils.h"
#include "termite/sdl_utils.h"

#include "imgui/imgui.h"

#include <conio.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

static SDL_Window* g_window = nullptr;
static termite::VectorGfxContext* g_vg = nullptr;
static termite::DebugDrawContext* g_debug = nullptr;
static termite::Camera g_cam;
static termite::vec2int_t g_displaySize = termite::vec2i(WINDOW_WIDTH, WINDOW_HEIGHT);

static void update(float dt)
{
    int mx, my;
    uint32_t mouseMask = SDL_GetMouseState(&mx, &my);

    // Camera look/movement
    const float moveSpeed = BX_ENABLED(BX_PLATFORM_ANDROID) ? 1.0f : 5.0f;
    const float lookSpeed = BX_ENABLED(BX_PLATFORM_ANDROID) ? 0.1f : 3.0f;

    int numKeys;
    const uint8_t* keys = SDL_GetKeyboardState(&numKeys);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
        termite::camStrafe(&g_cam, -1.0f*moveSpeed*dt);
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
        termite::camStrafe(&g_cam, moveSpeed*dt);
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
        termite::camForward(&g_cam, moveSpeed*dt);
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
        termite::camForward(&g_cam, -1.0f*moveSpeed*dt);
    SDL_GetRelativeMouseState(&mx, &my);
    if (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        termite::camPitchYaw(&g_cam, -float(my)*lookSpeed*dt, -float(mx)*lookSpeed*dt);
    }

    // Clear Scene
    termite::getGfxDriver()->touch(0);
    termite::getGfxDriver()->setViewClear(0, termite::GfxClearFlag::Color | termite::GfxClearFlag::Depth, 0x303030ff, 1.0f, 0);
    termite::getGfxDriver()->setViewRectRatio(0, 0, 0, termite::BackbufferRatio::Equal);
    
    float aspect = float(g_displaySize.x) / float(g_displaySize.y);
    termite::mtx4x4_t viewMtx = g_cam.getViewMtx();
    termite::mtx4x4_t projMtx = g_cam.getProjMtx(aspect);
    termite::vgBegin(g_vg, float(g_displaySize.x), float(g_displaySize.y));
    termite::vgTextf(g_vg, 10.0f, 10.0f, "Ft = %f", termite::getFrameTime()*1000.0);
    termite::vgEnd(g_vg);

    termite::ddBegin(g_debug, float(g_displaySize.x), float(g_displaySize.y), viewMtx, projMtx, g_vg);
    termite::ddColor(g_debug, termite::vec4f(0, 0.5f, 0, 1.0f));
    termite::ddSnapGridXZ(g_debug, g_cam, 1.0f, 5.0f, 50.0f);
    termite::ddColor(g_debug, termite::vec4f(1.0f, 0, 0, 1.0f));
    termite::ddBoundingBox(g_debug, termite::aabbf(-1.0f, -0.5f, -0.5f, 0.5f, 1.5f, 2.5f), true);
    termite::ddBoundingSphere(g_debug, termite::spheref(0, 0, 5.0f, 1.5f), true);
    termite::ddEnd(g_debug);
}

int main(int argc, char* argv[])
{
    bx::enableLogToFileHandle(stdout, stderr);
    bx::setLogTag("Termite");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        BX_FATAL("SDL Init failed");
        return -1;
    }

    termite::Config conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

#if BX_PLATFORM_ANDROID
    SDL_DisplayMode disp;
    if (SDL_GetCurrentDisplayMode(0, &disp) == 0) {
        g_displaySize.x = disp.w;
        g_displaySize.y = disp.h;
    }    
#endif

    g_window = SDL_CreateWindow("TestSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                g_displaySize.x, g_displaySize.y, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        BX_FATAL("SDL window creation failed");
        termite::shutdown();
        return -1;
    }

    conf.gfxWidth = g_displaySize.x;
    conf.gfxHeight = g_displaySize.y;

    termite::sdlMapImGuiKeys(&conf);

    termite::GfxPlatformData platformData;
    termite::sdlGetNativeWindowHandle(g_window, &platformData.nwh);
    
    if (termite::initialize(conf, update, &platformData) ||
        T_FAILED(termite::registerFont("fonts/fixedsys.fnt", "fixedsys")))
    {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }

    g_vg = termite::createVectorGfxContext(101);
    g_debug = termite::createDebugDrawContext(100);
    termite::camInit(&g_cam);
    termite::camLookAt(&g_cam, termite::vec3f(0, 1.0f, -12.0f), termite::vec3f(0, 0, 0));

    // reset graphics driver
    termite::getGfxDriver()->reset(g_displaySize.x, g_displaySize.y, 0);
    
    SDL_Event ev;
    while (true) {
        if (SDL_PollEvent(&ev)) {
            termite::sdlHandleEvent(ev);
            if (ev.type == SDL_QUIT)
                break;
            if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    BX_VERBOSE("Resume");
                    termite::sdlGetNativeWindowHandle(g_window, &platformData.nwh);
                    termite::getGfxDriver()->setPlatformData(platformData);
                    termite::getGfxDriver()->reset(g_displaySize.x, g_displaySize.y, 0);
                } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    BX_VERBOSE("Pause");
                }
            }
        }
        termite::doFrame();
    }

    termite::destroyDebugDrawContext(g_debug);
    termite::destroyVectorGfxContext(g_vg);
    termite::shutdown();
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}

