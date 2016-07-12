#include <cstddef>
#ifdef _WIN32
#  define HAVE_STDINT_H 1
#endif
#include <SDL.h>
#undef main
#include <SDL_syswm.h>
#undef None

#include "termite/core.h"
#include "bxx/logger.h"
#include "bxx/path.h"

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

#include <conio.h>

using namespace termite;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

struct InputData
{
    int mouseButtons[3];
    float mouseWheel;
    bool keysDown[512];
    bool keyShift;
    bool keyCtrl;
    bool keyAlt;

    InputData()
    {
        mouseButtons[0] = mouseButtons[1] = mouseButtons[2] = 0;
        mouseWheel = 0.0f;
        keyShift = keyCtrl = keyAlt = false;
        memset(keysDown, 0x00, sizeof(keysDown));
    }
};

static SDL_Window* g_window = nullptr;
static InputData g_input;
static VectorGfxContext* g_vg = nullptr;
static DebugDrawContext* g_debug = nullptr;
static Camera g_cam;
static ResourceHandle g_model;
static ProgramHandle g_modelProg;
static UniformHandle g_modelColor;

static bool sdlPollEvents()
{
    SDL_Event e;
    SDL_PollEvent(&e);

    switch (e.type) {
    case SDL_QUIT:
        return false;
    case SDL_MOUSEWHEEL:
    {
        if (e.wheel.y > 0)
            g_input.mouseWheel = 1.0f;
        else if (e.wheel.y < 0)
            g_input.mouseWheel = -1.0f;
        break;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
        if (e.button.button == SDL_BUTTON_LEFT) g_input.mouseButtons[0] = 1;
        if (e.button.button == SDL_BUTTON_RIGHT) g_input.mouseButtons[1] = 1;
        if (e.button.button == SDL_BUTTON_MIDDLE) g_input.mouseButtons[2] = 1;
        break;
    }

    case SDL_TEXTINPUT:
        inputSendChars(e.text.text);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        int key = e.key.keysym.sym & ~SDLK_SCANCODE_MASK;
        g_input.keysDown[key] = e.type == SDL_KEYDOWN;
        g_input.keyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        g_input.keyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        g_input.keyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
        inputSendKeys(g_input.keysDown, g_input.keyShift, g_input.keyAlt, g_input.keyCtrl);
        break;
    }

    default:
        break;
    }

    return true;
}

static void update(float dt)
{
    int mx, my;
    uint32_t mouseMask = SDL_GetMouseState(&mx, &my);

    float mousePos[2] = { 0.0f, 0.0f };
    if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MOUSE_FOCUS) {
        mousePos[0] = (float)mx;
        mousePos[1] = (float)my;
    } else {
        mousePos[0] = -1.0f;
        mousePos[1] = -1.0f;
    }

    g_input.mouseButtons[0] = (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;
    g_input.mouseButtons[1] = (mouseMask & SDL_BUTTON(SDL_BUTTON_RIGHT)) ? 1 : 0;
    g_input.mouseButtons[2] = (mouseMask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) ? 1 : 0;

    inputSendMouse(mousePos, g_input.mouseButtons, g_input.mouseWheel);

    g_input.mouseButtons[0] = g_input.mouseButtons[1] = g_input.mouseButtons[2] = 0;
    g_input.mouseWheel = 0.0f;

    vgBegin(g_vg, WINDOW_WIDTH, WINDOW_HEIGHT);
    
	// Camera look/movement
    const float moveSpeed = 5.0f;
    const float lookSpeed = 3.0f;

    int numKeys;
    const uint8_t* keys = SDL_GetKeyboardState(&numKeys);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
        camStrafe(&g_cam, -1.0f*moveSpeed*dt);
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
        camStrafe(&g_cam, moveSpeed*dt);
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
        camForward(&g_cam, moveSpeed*dt);
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
        camForward(&g_cam, -1.0f*moveSpeed*dt);
    SDL_GetRelativeMouseState(&mx, &my);
    if (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        camPitchYaw(&g_cam, -float(my)*lookSpeed*dt, -float(mx)*lookSpeed*dt);
    }
    vgTextf(g_vg, 10.0f, 10.0f, "pitch=%.4f", g_cam.pitch);
    vgEnd(g_vg);

    ddBegin(g_debug, WINDOW_WIDTH, WINDOW_HEIGHT, &g_cam, g_vg);
    ddColor(g_debug, vec4f(0, 0.5f, 0, 1.0f));
    ddSnapGridXZ(g_debug, 1.0f, 5.0f, 50.0f);
    ddColor(g_debug, vec4f(1.0f, 0, 0, 1.0f));
    ddBoundingBox(g_debug, aabbf(-1.0f, -0.5f, -0.5f, 0.5f, 1.5f, 2.5f), true);
    ddBoundingSphere(g_debug, spheref(0, 0, 5.0f, 1.5f), true);
    ddEnd(g_debug);

#if 0
    const vec4_t colors[] = {
        vec4f(1.0f, 1.0f, 1.0f, 1.0f),
        vec4f(1.0f, 0, 0, 1.0f),
        vec4f(0, 0, 1.0f, 1.0f)
    };

    gfxModel* model = dsGetObjPtr<gfxModel*>(nullptr, g_model);
    if (model) {
        gfxDriverI* driver = coreGetGfxDriver();
        mtx4x4_t viewMtx = camViewMtx(&g_cam);
        mtx4x4_t projMtx = camProjMtx(&g_cam, float(WINDOW_WIDTH) / float(WINDOW_HEIGHT));

        driver->touch(6);
        driver->setViewRect(6, 0, 0, gfxBackbufferRatio::Equal);
        driver->setViewTransform(6, &viewMtx, &projMtx);

        for (int i = 0; i < model->numNodes; i++) {
            const gfxModel::Node& node = model->nodes[i];
            const gfxModel::Mesh& mesh = model->meshes[node.mesh];
            const gfxModel::Geometry& geo = model->geos[mesh.geo];

            mtx4x4_t localmtx = node.localMtx;
            if (node.parent != -1) {
                const gfxModel::Node& parentNode = model->nodes[node.parent];
                localmtx = localmtx * parentNode.localMtx;
            }

            for (int c = 0; c < mesh.numSubmeshes; c++) {
                const gfxModel::Submesh& submesh = mesh.submeshes[c];
                driver->setTransform(&localmtx, 1);
                driver->setUniform(g_modelColor, &colors[submesh.mtl % 3]);
                driver->setVertexBuffer(mdlGetVertexBuffer(model, mesh.geo));
                driver->setIndexBuffer(mdlGetIndexBuffer(model, mesh.geo), submesh.startIndex, submesh.numIndices);

                driver->submit(6, g_modelProg);
            }
        }
    }
#endif
}

static termite::GfxPlatformData* getSDLWindowData(SDL_Window* window)
{
    static termite::GfxPlatformData platform;
    memset(&platform, 0x00, sizeof(platform));

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi)) {
        BX_WARN("Could not fetch SDL window handle");
        return nullptr;
    }

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    platform.ndt = wmi.info.x11.display;
    platform.nwh = (void*)(uintptr_t)wmi.info.x11.window;
#elif BX_PLATFORM_OSX
    platform.ndt = NULL;
    platform.nwh = wmi.info.cocoa.window;
#elif BX_PLATFORM_WINDOWS
    platform.ndt = NULL;
    platform.nwh = wmi.info.win.window;
#endif // BX_PLATFORM_

    return &platform;
}

int main(int argc, char* argv[])
{
    bx::enableLogToFileHandle(stdout, stderr);

    if (SDL_Init(0) != 0) {
        BX_FATAL("SDL Init failed");
        return -1;
    }

    termite::Config conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    g_window = SDL_CreateWindow("stTestSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                               WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!g_window) {
        BX_FATAL("SDL window creation failed");
        termite::shutdown();
        return -1;
    }

    conf.gfxWidth = WINDOW_WIDTH;
    conf.gfxHeight = WINDOW_HEIGHT;

    // Set Keymap
    conf.keymap[int(GuiKeyMap::Tab)] = SDLK_TAB;
    conf.keymap[int(GuiKeyMap::LeftArrow)] = SDL_SCANCODE_LEFT;
    conf.keymap[int(GuiKeyMap::DownArrow)] = SDL_SCANCODE_DOWN;
    conf.keymap[int(GuiKeyMap::UpArrow)] = SDL_SCANCODE_UP;
    conf.keymap[int(GuiKeyMap::DownArrow)] = SDL_SCANCODE_DOWN;
    conf.keymap[int(GuiKeyMap::PageUp)] = SDL_SCANCODE_PAGEUP;
    conf.keymap[int(GuiKeyMap::PageDown)] = SDL_SCANCODE_PAGEDOWN;
    conf.keymap[int(GuiKeyMap::Home)] = SDL_SCANCODE_HOME;
    conf.keymap[int(GuiKeyMap::End)] = SDL_SCANCODE_END;
    conf.keymap[int(GuiKeyMap::Delete)] = SDLK_DELETE;
    conf.keymap[int(GuiKeyMap::Backspace)] = SDLK_BACKSPACE;
    conf.keymap[int(GuiKeyMap::Enter)] = SDLK_RETURN;
    conf.keymap[int(GuiKeyMap::Escape)] = SDLK_ESCAPE;
    conf.keymap[int(GuiKeyMap::A)] = SDLK_a;
    conf.keymap[int(GuiKeyMap::C)] = SDLK_c;
    conf.keymap[int(GuiKeyMap::V)] = SDLK_v;
    conf.keymap[int(GuiKeyMap::X)] = SDLK_x;
    conf.keymap[int(GuiKeyMap::Y)] = SDLK_y;
    conf.keymap[int(GuiKeyMap::Z)] = SDLK_z;

    if (termite::initialize(conf, update, getSDLWindowData(g_window))) {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }

    g_vg = createVectorGfxContext(101);
    g_debug = createDebugDrawContext(100);
    camInit(&g_cam);
    camLookAt(&g_cam, vec3f(0, 1.0f, -12.0f), vec3f(0, 0, 0));

    LoadModelParams modelParams;
    g_model = loadResource(nullptr, "model", "models/torus.t3d", &modelParams);
    assert(g_model.isValid());

    GfxDriverApi* driver = getGfxDriver();
    g_modelProg = loadShaderProgram(driver, getIoDriver()->blocking, "shaders/test_model.vso", "shaders/test_model.fso");
    assert(g_modelProg.isValid());
    g_modelColor = driver->createUniform("u_color", UniformType::Vec4, 1);
        
    while (sdlPollEvents()) {
        termite::doFrame();
    }

    driver->destroyUniform(g_modelColor);
    driver->destroyProgram(g_modelProg);
    unloadResource(nullptr, g_model);

    destroyDebugDrawContext(g_debug);
    destroyVectorGfxContext(g_vg);
    termite::shutdown();
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}

