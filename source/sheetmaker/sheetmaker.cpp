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
#include "bxx/array.h"
#include "bx/commandline.h"
#include "bx/os.h"
#include "bx/uint32_t.h"
#include "bx/readerwriter.h"
#include <dirent.h>

#include "termite/core.h"
#include "termite/gfx_defines.h"
#include "termite/resource_lib.h"
#include "termite/gfx_texture.h"
#include "termite/io_driver.h"
#include "termite/math_util.h"
#include "termite/gfx_driver.h"
#include "termite/vec_math.h"
#include "termite/gfx_vg.h"

#include "../include_common/sprite_format.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define T_IMGUI_API
#include "termite/plugin_api.h"

#include <conio.h>

#include "nvg/nanovg.h"

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

struct Sprite
{
    float tx0, ty0;
    float tx1, ty1;
    int textureItem;
};

struct SpriteAnimation
{
    char name[32];
    int fps;
    int frameStart;
    int frameEnd;
};

struct SpriteSheet
{
    char textureFilepath[128];
    int numSprites;
    int numAnims;
    Sprite* sprites;
    SpriteAnimation* anims;

    SpriteSheet()
    {
        strcpy(textureFilepath, "");
        numSprites = 0;
        numAnims = 0;
        sprites = nullptr;
        anims = nullptr;
    }
};

struct SheetProject
{
    SpriteSheet* sheet;
    int imageSize[2];
    uint8_t* pixels;
    Texture image;
    char rootPath[128];

    int selectedSprite;
    int selectedAnim;

    SheetProject()
    {
        sheet = nullptr;
        imageSize[0] = imageSize[1] = 512;
        pixels = nullptr;
        selectedSprite = -1;
        selectedAnim = -1;
        strcpy(rootPath, "");
    }
};

struct TextureItem
{
    char filepath[128];
    ResourceHandle handle;
};

struct TextureDatabase
{
    bx::Array<TextureItem> textures;
    ResourceLib* resLib;
    int loadedIdx;

    TextureDatabase()
    {
        resLib = nullptr;
        loadedIdx = 0;
    }
};
static SDL_Window* g_window = nullptr;
static InputData g_input;
static ImGuiApi_v0* g_gui = nullptr;
static NVGcontext* g_nvg = nullptr;
static SheetProject* g_project = nullptr;
static TextureDatabase* g_textureDb = nullptr;
static VectorGfxContext* g_vg = nullptr;

static bool fileIsValidTexture(const bx::Path& ext)
{
    if (ext.isEqualNoCase("tga") ||
        ext.isEqualNoCase("bmp") ||
        ext.isEqualNoCase("png") ||
        ext.isEqualNoCase("jpg") || 
        ext.isEqualNoCase("gif"))
    {
        return true;
    }

    return false;
}

static TextureDatabase* createTextureDatabase(const char* baseDir, const char* rootDir, ResourceLib* resLib)
{
    bx::AllocatorI* alloc = getHeapAlloc();
    bx::Path fullDir(baseDir);
    fullDir.join(rootDir);

    DIR* d = opendir(fullDir.cstr());
    if (!d)
        return nullptr;

    TextureDatabase* db = BX_NEW(alloc, TextureDatabase)();
    db->resLib = resLib;
    if (!db->textures.create(256, 512, alloc))
        return nullptr;
    
    dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        bx::Path filename(ent->d_name);
        
        if (ent->d_type == DT_REG && fileIsValidTexture(filename.getFileExt())) {
            bx::Path filepath(rootDir);
            filepath.joinUnix(filename.cstr());
            TextureItem* item = db->textures.push();
            bx::strlcpy(item->filepath, filepath.cstr(), sizeof(item->filepath));
            item->handle.reset();
        }        
    }
    closedir(d);

    return db;
}

static SheetProject* createProject(const char* rootPath)
{
    bx::AllocatorI* alloc = getHeapAlloc();
    SheetProject* project = BX_NEW(alloc, SheetProject);
    if (!project)
        return nullptr;
    project->sheet = BX_NEW(alloc, SpriteSheet);
    if (!project->sheet)
        return nullptr;
    bx::strlcpy(project->rootPath, rootPath, sizeof(project->rootPath));
    return project;
}

static void destroyProject(SheetProject* project)
{
    bx::AllocatorI* alloc = getHeapAlloc();
    if (project->sheet) {
        SpriteSheet* sheet = project->sheet;
        if (sheet->sprites) {
            BX_FREE(alloc, sheet->sprites);
        }
        if (sheet->anims) {
            BX_FREE(alloc, sheet->anims);
        }
        BX_DELETE(alloc, sheet);
    }

    if (project->pixels)
        BX_FREE(alloc, project->pixels);
    
    BX_DELETE(alloc, project);
}

static void loadTexturesIterative(TextureDatabase* db)
{
    ResourceLib* resLib = db->resLib;

    int index = db->loadedIdx;
    if (index == db->textures.getCount())
        return;

    TextureItem* item = db->textures.itemPtr(index);
    if (!item->handle.isValid()) {
        LoadTextureParams params;
        params.flags |= TextureFlag::MipPoint;

        item->handle = loadResource(resLib, "image", item->filepath, &params);
        db->loadedIdx++;
    }
}

static void destroyTextureDatabase(TextureDatabase* db)
{
    bx::AllocatorI* alloc = getHeapAlloc();

    // Release all textures
    ResourceLib* resLib = db->resLib;
    if (resLib) {
        for (int i = 0; i < db->textures.getCount(); i++) {
            TextureItem* item = db->textures.itemPtr(i);
            if (item->handle.isValid()) {
                unloadResource(resLib, item->handle);
            }
        }
    }
    db->textures.destroy();
    BX_DELETE(alloc, db);
}

static bool showMessageBox(const char* name, const char* fmt, ...)
{
    if (g_gui->beginPopupModal(name, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        va_list args;
        va_start(args, fmt);
        g_gui->textV(fmt, args);
        va_end(args);

        if (g_gui->button("Ok", ImVec2(150.0f, 0)))
            g_gui->closeCurrentPopup();

        g_gui->endPopup();
        return true;
    }
    return false;
}

static bool saveSpriteSheet(const char* filepath, const SpriteSheet* sheetInfo, const uint8_t* pixels, int width, int height)
{
    if (!pixels)
        return false;
    assert(pixels);

    IoDriverDual* io = getIoDriver();
    bx::AllocatorI* alloc = getHeapAlloc();

    // Save Image
    bx::Path filepathp(filepath);
    bx::Path filename = filepathp.getFilename();
    bx::Path filedir = filepathp.getDirectory();
    filename += ".tga";
    filepathp = io->blocking->getUri();
    filepathp.join(filedir.cstr()).join(filename.cstr());
    if (!stbi_write_tga(filepathp.cstr(), width, height, 4, pixels)) {
        return false;
    }
        
    // Save Descriptor file
    bx::MemoryBlock mem(alloc);
    bx::MemoryWriter writer(&mem);
    bx::Error err;

    tsHeader header;
    header.sign = TSPRITE_SIGN;
    header.version = TSPRITE_VERSION;
    header.numAnims = sheetInfo->numAnims;
    header.numSprites = sheetInfo->numSprites;

    filepathp = filedir;
    filepathp.joinUnix(filename.cstr());
    bx::strlcpy(header.textureFilepath, filepathp.cstr(), sizeof(header.textureFilepath));
    writer.write(&header, sizeof(header), &err);

    // Write sprites
    tsSprite* sprites = (tsSprite*)alloca(sizeof(tsSprite)*header.numSprites);
    assert(sprites);
    for (int i = 0; i < header.numSprites; i++) {
        const Sprite& sprite = sheetInfo->sprites[i];
        tsSprite& tsprite = sprites[i];

        tsprite.tx0 = sprite.tx0;
        tsprite.ty0 = sprite.ty0;
        tsprite.tx1 = sprite.tx1;
        tsprite.ty1 = sprite.ty1;
    }
    writer.write(sprites, sizeof(tsSprite)*header.numSprites, &err);

    // Write Anims
    tsAnimation* anims = (tsAnimation*)alloca(sizeof(tsAnimation)*header.numAnims);
    assert(anims);
    for (int i = 0; i < header.numAnims; i++) {
        const SpriteAnimation& anim = sheetInfo->anims[i];
        tsAnimation& tanim = anims[i];

        bx::strlcpy(tanim.name, anim.name, sizeof(tanim.name));
        tanim.startFrame = anim.frameStart;
        tanim.endFrame = anim.frameEnd;
        tanim.fps = anim.fps;        
    }
    writer.write(&anims, sizeof(tsAnimation)*header.numAnims, &err);

    // Write to file
    MemoryBlock* block = refMemoryBlockPtr(mem.more(), (uint32_t)writer.seek());
    bool r = io->blocking->write(filepath, block) > 0 ? true : false;
    releaseMemoryBlock(block);

    return r;
}

static void generateSpriteSheet(Sprite* sprites, int numSprites, int width, int height)
{
    bx::AllocatorI* alloc = getHeapAlloc();

    if (numSprites == 0)
        return;
    
    stbrp_context ctx;
    stbrp_node* nodes = (stbrp_node*)alloca(sizeof(stbrp_node)*width);
    stbrp_rect* rects = (stbrp_rect*)alloca(sizeof(stbrp_rect)*numSprites);
    assert(nodes);
    assert(rects);
    memset(rects, 0x00, sizeof(stbrp_rect)*numSprites);

    int numNodes = width;
    stbrp_init_target(&ctx, width, height, nodes, numNodes);
    
    for (int i = 0; i < numSprites; i++) {
        const Sprite& sprite = sprites[i];
        const TextureItem& tex = g_textureDb->textures[sprite.textureItem];
        Texture* t = (Texture*)getResourceObj(g_textureDb->resLib, tex.handle);

        rects[i].id = i;
        rects[i].w = t->info.width;
        rects[i].h = t->info.height;
    }

    stbrp_pack_rects(&ctx, rects, numSprites);

    g_project->pixels = (uint8_t*)BX_REALLOC(alloc, g_project->pixels, width*height*sizeof(uint32_t));
    if (!g_project->pixels) {
        g_gui->openPopup("msg");
        return;
    }
    uint8_t* destPixels = g_project->pixels;
    memset(destPixels, 0x00, width*height * sizeof(uint32_t));
        
    for (int i = 0; i < numSprites; i++) {
        if (!rects[i].was_packed) {
            g_gui->openPopup("msg");
            break;
        }

        const Sprite& sprite = sprites[i];
        const TextureItem& tex = g_textureDb->textures[sprite.textureItem];
        Texture* t = (Texture*)getResourceObj(g_textureDb->resLib, tex.handle);

        sprites[i].tx0 = float(rects[i].x) / float(width);
        sprites[i].ty0 = float(rects[i].y) / float(height);
        sprites[i].tx1 = sprites[i].tx0 + float(t->info.width)/float(width);
        sprites[i].ty1 = sprites[i].ty0 + float(t->info.height) / float(height);
        
        // Load image data
        IoDriverDual* io = getIoDriver();
        MemoryBlock* imageData = io->blocking->read(tex.filepath);
        if (imageData) {
            int srcWidth, srcHeight, comp;
            stbi_uc* srcPixels = stbi_load_from_memory(imageData->data, imageData->size, &srcWidth, &srcHeight, &comp, 4);
            releaseMemoryBlock(imageData);
            if (srcPixels) {
                blitRawPixels(destPixels, rects[i].x, rects[i].y, width, height, srcPixels, 0, 0,
                              t->info.width, t->info.height, sizeof(uint32_t));
                stbi_image_free(srcPixels);
            }   
        }
    }    

    if (showMessageBox("msg", "Increase the image size. Not all sprites fit the target image"))
        return;

    // create texture for preview
    if (g_project->image.handle.isValid()) {
        getGfxDriver()->destroyTexture(g_project->image.handle);
    }
    g_project->image.handle = getGfxDriver()->createTexture2D(width, height, 1, TextureFormat::RGBA8,
                                                       TextureFlag::MinPoint | TextureFlag::MagPoint,
                                                       getGfxDriver()->makeRef(g_project->pixels, width*height * sizeof(uint32_t), nullptr, nullptr));
    g_project->image.info.width = width;
    g_project->image.info.height = height;
}

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

static int showTexturesPopup(const char* popupName)
{
    int selectedIdx = -1;
    if (g_gui->beginPopupModal(popupName, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        if (g_gui->button("Close", ImVec2(0, 0)))
            g_gui->closeCurrentPopup();

        g_gui->beginChild("Gallery", ImVec2(300.0f, 300.0f), true, ImGuiWindowFlags_AlwaysAutoResize);
        const int numColumns = 4;
        g_gui->columns(numColumns, nullptr, false);

        int textureIdx = 0;
        for (int i = 0, c = g_textureDb->textures.getCount(); i < c; i += numColumns) {
            for (int k = 0; k < numColumns; k++) {
                if (textureIdx >= c)
                    break;

                const TextureItem& tex = g_textureDb->textures[textureIdx];
                if (tex.handle.isValid()) {
                    TextureHandle* handle = &((Texture*)getResourceObj(g_textureDb->resLib, tex.handle))->handle;
                    if (g_gui->imageButton((ImTextureID)handle, ImVec2(64, 64), ImVec2(0, 0),
                                           ImVec2(1.0f, 1.0f), 1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f))) 
                    {
                        selectedIdx = textureIdx;
                        g_gui->closeCurrentPopup();
                    }

                    if (g_gui->isItemHovered())
                        g_gui->setTooltip("%s", tex.filepath);
                }

                g_gui->nextColumn();
                textureIdx++;
            }
        }

        g_gui->endChild();
        g_gui->endPopup();
    }

    return selectedIdx;
}

static bool showEditPopup(const char* name, const char* caption, char* value, size_t valueSize)
{
    static char text[256];
    bool r = false;
    if (g_gui->beginPopupModal(name, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        g_gui->inputText(caption, text, sizeof(text), 0, nullptr, nullptr);
        if (g_gui->button("Ok", ImVec2(100.0f, 0))) {
            bx::strlcpy(value, text, valueSize);
            g_gui->closeCurrentPopup();
            r = true;
        }
        g_gui->sameLine(0, -1.0f);
        if (g_gui->button("Cancel", ImVec2(100.0f, 0))) {
            g_gui->closeCurrentPopup();
        }
        g_gui->endPopup();
    }
    return r;
}

static void renderGui(float dt)
{
    bx::AllocatorI* alloc = getHeapAlloc();
    SpriteSheet* sheet = g_project->sheet;

    static bool opened = true;
    g_gui->begin("SheetMaker", &opened, 0);

    if (g_gui->button("New Sheet", ImVec2(150.0f, 0))) {
    }

    if (g_gui->button("Open", ImVec2(150.0f, 0))) {
    }

    if (g_gui->button("Save", ImVec2(150.0f, 0))) {
    }

    if (g_gui->button("Export", ImVec2(150.0f, 0))) {
        g_gui->openPopup("Export Spritesheet");
    }
    char sheetName[128];
    if (showEditPopup("Export Spritesheet", "Name", sheetName, sizeof(sheetName))) {
        bx::Path filepath("spritesheets");
        filepath.joinUnix(sheetName);
        filepath += ".tsheet";
        saveSpriteSheet(filepath.cstr(), sheet, g_project->pixels, g_project->imageSize[0], g_project->imageSize[1]);
    }

    g_gui->inputInt2("Size", g_project->imageSize, 0);

    if (g_gui->collapsingHeader("Sprites", nullptr, true, true)) {
        if (g_gui->button("Add", ImVec2(100.0f, 0))) {
            g_gui->openPopup("Add Texture");
        }

        int textureIdx = showTexturesPopup("Add Texture");
        if (textureIdx != -1) {
            // Add to list
            sheet->sprites = (Sprite*)BX_REALLOC(alloc, sheet->sprites, (sheet->numSprites + 1) * sizeof(Sprite));
            assert(sheet->sprites);
            Sprite& sprite = sheet->sprites[sheet->numSprites];
            sprite.textureItem = textureIdx;
            sprite.tx0 = sprite.ty0 = sprite.tx1 = sprite.ty1 = -1.0f;
            sheet->numSprites++;
        }

        g_gui->sameLine(0, -1.0f);
        if (g_gui->button("Remove", ImVec2(100.0f, 0)) && g_project->selectedSprite != -1) {
            int index = g_project->selectedSprite;
            int nextIndex = index + 1 < sheet->numSprites ? (index + 1) : index;
            if (index != nextIndex)
                memmove(&sheet->sprites[index], &sheet->sprites[nextIndex], (sheet->numSprites - nextIndex) * sizeof(Sprite));
            sheet->numSprites--;
            g_project->selectedSprite = sheet->numSprites ? index : -1;
        }

        // Sprite List
        if (sheet->numSprites) {
            char** names = (char**)alloca(sizeof(char*)*sheet->numSprites);
            for (int i = 0; i < sheet->numSprites; i++) {
                const Sprite& sprite = sheet->sprites[i];
                const TextureItem& tex = g_textureDb->textures[sprite.textureItem];

                names[i] = (char*)alloca(sizeof(char) * 128);
                bx::strlcpy(names[i], tex.filepath, 128);
            }
            g_gui->listBox("Sprites", &g_project->selectedSprite, (const char**)names, sheet->numSprites, -1);
            if (g_gui->isItemHovered() && g_project->selectedSprite != -1) {
                const Sprite& sprite = sheet->sprites[g_project->selectedSprite];
                const TextureItem& texItem = g_textureDb->textures[sprite.textureItem];
                Texture* tex = (Texture*)getResourceObj(g_textureDb->resLib, texItem.handle);
                TextureHandle* handle = &tex->handle;

                g_gui->beginTooltip();
                g_gui->image((ImTextureID)handle, ImVec2(128.0f, 128.0f), ImVec2(0, 0), ImVec2(1.0f, 1.0f),
                             ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0, 0, 0, 0));
                g_gui->text("%dx%d Index=%d", tex->info.width, tex->info.height, g_project->selectedSprite);
                g_gui->endTooltip();
            }

            if (g_gui->button("Generate Sheet", ImVec2(150.0f, 0))) {
                generateSpriteSheet(sheet->sprites, sheet->numSprites, g_project->imageSize[0], g_project->imageSize[1]);
            }
        }
    }

    if (g_gui->collapsingHeader("Animation Clips", nullptr, true, true)) {
        if (g_gui->button("Add Clip", ImVec2(150.0f, 0))) {
            g_gui->openPopup("Add Clip");
        }

        char name[32];
        name[0] = 0;
        if (showEditPopup("Add Clip", "Name", name, sizeof(name))) {
            sheet->anims = (SpriteAnimation*)BX_REALLOC(alloc, sheet->anims, (sheet->numAnims + 1)*sizeof(SpriteAnimation));
            assert(sheet->anims);
            SpriteAnimation& anim = sheet->anims[sheet->numAnims];
            sheet->numAnims++;
            memset(&anim, 0x00, sizeof(anim));
            anim.fps = 30;
            bx::strlcpy(anim.name, name, sizeof(anim.name));
        }

        g_gui->sameLine(0, -1);
        if (g_gui->button("Remove Clip", ImVec2(150.0f, 0)) && g_project->selectedAnim != -1) {
            int index = g_project->selectedSprite;
            int nextIndex = index + 1 < sheet->numSprites ? (index + 1) : index;
            if (index != nextIndex)
                memmove(&sheet->sprites[index], &sheet->sprites[nextIndex], (sheet->numSprites - nextIndex) * sizeof(Sprite));
            sheet->numSprites--;
            g_project->selectedAnim = sheet->numAnims ? index : -1;
        }

        // Animation list
        if (sheet->numAnims) {
            char** names = (char**)alloca(sizeof(char*)*sheet->numAnims);
            for (int i = 0; i < sheet->numAnims; i++) {
                const SpriteAnimation& anim = sheet->anims[i];
                names[i] = (char*)alloca(sizeof(char) * 32);
                bx::strlcpy(names[i], anim.name, 32);
            }

            g_gui->listBox("Clips", &g_project->selectedAnim, (const char**)names, sheet->numAnims, -1);
        }

        if (g_project->selectedAnim != -1 && sheet->numSprites) {
            SpriteAnimation& anim = sheet->anims[g_project->selectedAnim];
            g_gui->sliderInt("Fps", &anim.fps, 1, 60, "%.0f");
            g_gui->sliderInt("Start", &anim.frameStart, 0, sheet->numSprites - 1, "%.0f");
            anim.frameEnd = bx::uint32_max(anim.frameEnd, anim.frameStart);
            g_gui->sliderInt("End", &anim.frameEnd, anim.frameStart, sheet->numSprites, "%.0f");
            
            if (g_project->image.handle.isValid() && anim.frameEnd - anim.frameStart > 0) {
                static float animTime = 0;

                // Play sprite animation
                float availWidth = g_gui->getContentRegionAvailWidth();
                float ratio = float(g_project->image.info.width) / float(g_project->image.info.height);

                int frameCount = anim.frameEnd - anim.frameStart;
                float totalTime = float(frameCount) / float(anim.fps);
                animTime = bx::fwrap(animTime + dt, totalTime);
                float animFrameTime = 1.0f / float(anim.fps);

                int frameIdx = bx::uint32_clamp(int(animTime / animFrameTime), 0, frameCount - 1);
                const Sprite& sprite = sheet->sprites[frameIdx];

                g_gui->image((ImTextureID)&g_project->image.handle, ImVec2(availWidth, availWidth/ratio), 
                             ImVec2(sprite.tx0, sprite.ty0), ImVec2(sprite.tx1, sprite.ty1), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                             ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
        }
    }

    g_gui->end();
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

    static float loadTextureElapsed = 0;
    if (loadTextureElapsed >= 0.1f) {
        loadTexturesIterative(g_textureDb);
        loadTextureElapsed = 0;
    }
    loadTextureElapsed += dt;

    renderGui(dt);

    float imageWidth = float(g_project->imageSize[0]);
    float imageHeight = float(g_project->imageSize[1]);
    float viewWidth = float(WINDOW_WIDTH);
    float viewHeight = float(WINDOW_HEIGHT);
    const float minBorderX = 100.0f;
    const float minBorderY = 50.0f;
    float viewWidthBorder = viewWidth - minBorderX*2.0f;
    float viewHeightBorder = viewHeight - minBorderY*2.0f;
    float ratio = imageWidth / imageHeight;
    if (imageHeight > viewHeightBorder) {
        imageHeight = viewHeightBorder;
        imageWidth = ratio*imageHeight;
    }
    if (imageWidth > viewWidthBorder) {
        imageWidth = viewWidthBorder;
        imageHeight = imageWidth / ratio;
    }
    float imageX = (viewWidth - imageWidth)*0.5f;
    float imageY = (viewHeight - imageHeight)*0.5f;
    
    vgBegin(g_vg, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (g_project->image.handle.isValid()) {
        vgImageRect(g_vg, rectfwh(imageX, imageY, imageWidth, imageHeight), &g_project->image);
    }
    vgEnd(g_vg);
    
    nvgBeginFrame(g_nvg, WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f);

    // Image Border
    nvgBeginPath(g_nvg);
    nvgStrokeWidth(g_nvg, 1.0f);
    nvgStrokeColor(g_nvg, nvgRGB(128, 128, 128));
    nvgRect(g_nvg, imageX - 1.0f, imageY - 1.0f, imageWidth + 2.0f, imageHeight + 2.0f);
    nvgStroke(g_nvg);

    // Sprite Border
    if (g_project->selectedSprite != -1) {
        const Sprite& sprite = g_project->sheet->sprites[g_project->selectedSprite];
        if (sprite.tx0 >= 0.0f && sprite.ty0 >= 0.0f) {
            float x = imageWidth*sprite.tx0;
            float y = imageHeight*sprite.ty0;
            float w = (sprite.tx1 - sprite.tx0)*imageWidth;
            float h = (sprite.ty1 - sprite.ty0)*imageHeight;

            nvgBeginPath(g_nvg);
            nvgStrokeWidth(g_nvg, 1.0f);
            nvgStrokeColor(g_nvg, nvgRGB(0, 128, 0));
            nvgRect(g_nvg, imageX + x, imageY + y, w, h);
            nvgStroke(g_nvg);
        }
    }

    nvgEndFrame(g_nvg);
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
    bx::CommandLine cmd(argc, argv);
    const char* projectRoot = cmd.findOption('p', "project");

    bx::enableLogToFileHandle(stdout, stderr);

    if (SDL_Init(0) != 0) {
        BX_FATAL("SDL Init failed");
        return -1;
    }

    termite::Config conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    g_window = SDL_CreateWindow("SheetMaker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!g_window) {
        BX_FATAL("SDL window creation failed");
        termite::shutdown();
        return -1;
    }
    GfxPlatformData* platformData = getSDLWindowData(g_window);


    conf.gfxWidth = WINDOW_WIDTH;
    conf.gfxHeight = WINDOW_HEIGHT;
    conf.enableJobDispatcher = false;
    conf.gfxDriverFlags = uint32_t(GfxResetFlag::VSync);
    strcpy(conf.uiIniFilename, "sheetmaker.ini");
    conf.pageSize = 64;

    bx::Path dataUri(projectRoot);
    dataUri.normalizeSelf();
    dataUri.join("assets");
    bx::strlcpy(conf.dataUri, dataUri.cstr(), sizeof(conf.dataUri));

    // Set Keymap
    conf.keymap[ImGuiKey_Tab] = SDLK_TAB;
    conf.keymap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    conf.keymap[ImGuiKey_RightArrow] = SDL_SCANCODE_DOWN;
    conf.keymap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    conf.keymap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    conf.keymap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    conf.keymap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    conf.keymap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    conf.keymap[ImGuiKey_End] = SDL_SCANCODE_END;
    conf.keymap[ImGuiKey_Delete] = SDLK_DELETE;
    conf.keymap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    conf.keymap[ImGuiKey_Enter] = SDLK_RETURN;
    conf.keymap[ImGuiKey_Escape] = SDLK_ESCAPE;
    conf.keymap[ImGuiKey_A] = SDLK_a;
    conf.keymap[ImGuiKey_C] = SDLK_c;
    conf.keymap[ImGuiKey_V] = SDLK_v;
    conf.keymap[ImGuiKey_X] = SDLK_x;
    conf.keymap[ImGuiKey_Y] = SDLK_y;
    conf.keymap[ImGuiKey_Z] = SDLK_z;

    if (termite::initialize(conf, update, platformData)) {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }

    bx::AllocatorI* alloc = getHeapAlloc();

    IoDriverDual* io = getIoDriver();
    ResourceLib* resLib = getDefaultResourceLib();

    g_gui = (ImGuiApi_v0*)getEngineApi(uint16_t(ApiId::ImGui), 0);    
    g_nvg = nvgCreate(1, 254, getGfxDriver(), (GfxApi_v0*)getEngineApi(uint16_t(ApiId::Gfx), 0), alloc);
    g_textureDb = createTextureDatabase(io->async->getUri(), "source/sprites", resLib);
    g_vg = createVectorGfxContext(1);
    
    if (!g_textureDb) {
        BX_FATAL("Could not load texture database");
        termite::shutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }
    g_project = createProject(dataUri.cstr());

    while (sdlPollEvents()) {
        termite::doFrame();
    }

    destroyVectorGfxContext(g_vg);
    destroyProject(g_project);
    destroyTextureDatabase(g_textureDb);
    nvgDelete(g_nvg);
    termite::shutdown();
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}

