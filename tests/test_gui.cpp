#include <cstddef>
#ifdef _WIN32
#  define HAVE_STDINT_H 1
#endif
#include <SDL.h>
#undef main
#include <SDL_syswm.h>
#undef None

#include "bxx/logger.h"
#include "bxx/path.h"

#include "termite/core.h"
#include "termite/gfx_defines.h"

#define T_IMGUI_API
#include "termite/plugin_api.h"

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
static ImGuiApi_v0* g_gui = nullptr;

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

	static bool opened = true;
	g_gui->begin("test", &opened, 0);
	g_gui->button("test", ImVec2(100, 0));
	static float color[3] = {1.0f, 0, 0};
	g_gui->colorEdit3("Color", color);
	//g_gui->colorButton(ImVec4(color[0], color[1], color[2], 1.0f), true, true);
	g_gui->end();
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

	g_window = SDL_CreateWindow("Termite: TestGui", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
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
	g_gui = (ImGuiApi_v0*)getEngineApi(uint16_t(ApiId::ImGui), 0);

	while (sdlPollEvents()) {
		termite::doFrame();
	}

	termite::shutdown();
	SDL_DestroyWindow(g_window);
	SDL_Quit();

	return 0;
}

