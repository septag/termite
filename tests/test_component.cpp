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
#include "termite/component_system.h"
#include "termite/gfx_debugdraw.h"
#include "termite/gfx_vg.h"
#include "termite/camera.h"

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

struct TestComponent
{
	char name[32];
	vec3_t pos;

	static bool createInstance(Entity ent, ComponentHandle handle)
	{
		TestComponent* data = getComponentData<TestComponent>(handle);

		// Set random position
		float x = getRandomFloatUniform(-20.0f, 20.0f);
		float z = getRandomFloatUniform(0.0f, 20.0f);

		data->pos = vec3f(x, 0.0f, z);

		static int nameIdx = 1;
		bx::snprintf(data->name, sizeof(data->name), "Entity #%d", nameIdx++);

		return true;
	}

	static void destroyInstance(Entity ent, ComponentHandle handle)
	{
		TestComponent* data = getComponentData<TestComponent>(handle);
		BX_VERBOSE("Entity: %s destroyed", data->name);
	}

	static void updateAll()
	{
	}

	static void preUpdateAll()
	{
	}

	static void postUpdateAll()
	{
	}

	static void registerSelf()
	{
		static ComponentCallbacks callbacks;
		callbacks.createInstance = createInstance;
		callbacks.destroyInstance = destroyInstance;
		callbacks.postUpdateAll = postUpdateAll;
		callbacks.preUpdateAll = preUpdateAll;
		callbacks.updateAll = updateAll;

		Handle = registerComponentType("Test", 0, &callbacks, ComponentFlag::None, sizeof(TestComponent), 100, 100);
	}

	static ComponentTypeHandle Handle;
};

ComponentTypeHandle TestComponent::Handle;

static SDL_Window* g_window = nullptr;
static InputData g_input;
static ImGuiApi_v0* g_gui = nullptr;
static EntityManager* g_emgr = nullptr;
static DebugDrawContext* g_ddraw = nullptr;
static VectorGfxContext* g_vg = nullptr;
static Camera g_cam;

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

	if (!g_gui->isMouseHoveringAnyWindow()) {
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
	}

	// 
	ddBegin(g_ddraw, float(WINDOW_WIDTH), float(WINDOW_HEIGHT), &g_cam, g_vg);
	ddSnapGridXZ(g_ddraw, 1.0f, 5.0f, 50.0f);

	// show components
	mtx4x4_t xform = mtx4x4Ident();
	
	ComponentHandle components[256];
	uint16_t numComponents = getAllComponents(TestComponent::Handle, components, 256);
	char** names = (char**)alloca(numComponents*sizeof(char*));

	for (uint16_t i = 0; i < numComponents; i++) {
		TestComponent* data = getComponentData<TestComponent>(components[i]);
		vec3_t pos = data->pos;
		float scale = 0.05f;
		vec3_t minpt = pos - vec3f(scale, scale, scale);
		vec3_t maxpt = pos + vec3f(scale, scale, scale);
		ddBoundingBox(g_ddraw, aabbv(minpt, maxpt));
		ddText(g_ddraw, pos, data->name);

		names[i] = (char*)alloca(strlen(data->name) + 1);
		strcpy(names[i], data->name);
	}

	ddEnd(g_ddraw);

	// Gui
	static bool opened = true;
	static float color[3] = { 1.0f, 0, 0 };

	g_gui->begin("ComponentTest", &opened, 0);
	if (g_gui->button("CreateEntity", ImVec2(100, 0))) {
		Entity ent = createEntity(g_emgr);
		createComponent(ent, TestComponent::Handle);
	}

	static int current = 0;
	g_gui->listBox("Entities", &current, (const char**)names, numComponents, -1);

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
	g_emgr = createEntityManager(getHeapAlloc());
	g_ddraw = createDebugDrawContext(1);
	g_vg = createVectorGfxContext(2);
	camInit(&g_cam);
	camLookAt(&g_cam, vec3f(0, 1.0f, -12.0f), vec3f(0, 0, 0));
	TestComponent::registerSelf();

	while (sdlPollEvents()) {
		termite::doFrame();
	}

	destroyDebugDrawContext(g_ddraw);
	destroyVectorGfxContext(g_vg);
	destroyEntityManager(g_emgr);
	termite::shutdown();
	SDL_DestroyWindow(g_window);
	SDL_Quit();

	return 0;
}

