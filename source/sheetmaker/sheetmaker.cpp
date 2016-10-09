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
#include "bx/hash.h"
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
#include "termite/sdl_utils.h"

#include "../include_common/sprite_format.h"
#include "../include_common/folder_png.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define T_IMGUI_API
#include "termite/plugin_api.h"

#include <conio.h>
#include <algorithm>

#include "nvg/nanovg.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

struct Sprite
{
    float tx0, ty0;
    float tx1, ty1;
    int textureItem;
    int tag;
};

struct SpriteAnimation
{
    char name[32];
    int fps;
    int frameStart;
    int frameEnd;
};

struct FrameTag
{
    char name[32];
};

struct SpriteSheet
{
    char textureFilepath[128];
    int numSprites;
    int numAnims;
    int numTags;

    Sprite* sprites;
    SpriteAnimation* anims;
    FrameTag* tags;

    SpriteSheet()
    {
        strcpy(textureFilepath, "");
        numSprites = 0;
        numAnims = 0;
        numTags = 0;
        sprites = nullptr;
        anims = nullptr;
        tags = nullptr;
    }
};

struct SheetProject
{
    SpriteSheet* sheet;
    int imageSize[2];
    uint8_t* pixels;
    termite::Texture image;
    char rootPath[128];

    int selectedSprite;
    int selectedAnim;
    int selectedTag;

    SheetProject()
    {
        sheet = nullptr;
        imageSize[0] = imageSize[1] = 512;
        pixels = nullptr;
        selectedSprite = -1;
        selectedAnim = -1;
        selectedTag = -1;
        strcpy(rootPath, "");
    }
};

struct TextureItemType
{
    enum Enum
    {
        Image,
        Directory
    };
};

struct TextureItem
{
    TextureItemType::Enum type;
    char filepath[128];
    termite::ResourceHandle handle;
};

struct TextureDatabase
{
    bx::Array<TextureItem> textures;
    termite::ResourceLibHelper resLib;
    int loadedIdx;
    termite::ResourceHandle folderImg;

    TextureDatabase(termite::ResourceLibHelper _resLib)
    {
        resLib = _resLib;
        loadedIdx = 0;
    }
};

struct App
{
    SDL_Window* wnd;
    termite::ImGuiApi_v0* gui;
    NVGcontext* nvg;
    SheetProject* project;
    TextureDatabase* textureDb;
    termite::VectorGfxContext* vg;

    App()
    {
        wnd = nullptr;
        gui = nullptr;
        nvg = nullptr;
        project = nullptr;
        textureDb = nullptr;
        vg = nullptr;
    }
};

static App theApp;

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

static void recurseTextureDirectories(TextureDatabase* db, const char* baseDir, const char* rootDir, const char* dir)
{
    bx::Path dirpath(baseDir);
    dirpath.join(dir).normalizeSelf();
    
    DIR* d = opendir(dirpath.cstr());
    if (!d)
        return;

    dirent* ent;
    bool foundImageDir = false;

    while ((ent = readdir(d)) != nullptr) {
        bx::Path filename(ent->d_name);
        // If we find an image inside the directory then add the directory 
        if (!foundImageDir && ent->d_type == DT_REG && fileIsValidTexture(filename.getFileExt())) {
            TextureItem* item = db->textures.push();
            item->type = TextureItemType::Directory;
            item->handle = db->folderImg;
            bx::strlcpy(item->filepath, dir, sizeof(item->filepath));
            foundImageDir = true;
        } else if (ent->d_type == DT_DIR && !filename.isEqual(".") && !filename.isEqual("..")) {
            bx::Path newdir(dir);
            newdir.joinUnix(ent->d_name);
            recurseTextureDirectories(db, baseDir, rootDir, newdir.cstr());
        }
    }
    closedir(d);   
}

static TextureDatabase* createTextureDatabase(const char* baseDir, const char* rootDir, termite::ResourceLibHelper resLib)
{
    bx::AllocatorI* alloc = termite::getHeapAlloc();
    bx::Path fullDir(baseDir);
    fullDir.join(rootDir).normalizeSelf();

    DIR* d = opendir(fullDir.cstr());
    if (!d)
        return nullptr;

    TextureDatabase* db = BX_NEW(alloc, TextureDatabase)(resLib);
    if (!db->textures.create(256, 512, alloc))
        return nullptr;

    termite::LoadTextureParams tparams;
    db->folderImg = db->resLib.loadResourceFromMem("image", "folder_png", 
                                                   termite::refMemoryBlockPtr(folder_png, sizeof(folder_png)), &tparams);
    
    dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        bx::Path filename(ent->d_name);
        bx::Path filepath(rootDir);
        filepath.joinUnix(filename.cstr());

        if (ent->d_type == DT_REG && fileIsValidTexture(filename.getFileExt())) {
            TextureItem* item = db->textures.push();
            item->type = TextureItemType::Image;
            bx::strlcpy(item->filepath, filepath.cstr(), sizeof(item->filepath));
            item->handle.reset();
        } else if (ent->d_type == DT_DIR && !filename.isEqual(".") && !filename.isEqual("..")) {
            recurseTextureDirectories(db, baseDir, rootDir, filepath.cstr());
        }
    }
    closedir(d);

    return db;
}

static void loadTextureInDirectory(TextureDatabase* db, const char* rootDir, bx::Array<int>* textureIndices)
{
    termite::IoDriverApi* ioDriver = db->resLib.getResourceLibIoDriver();
    const char* baseDir = ioDriver->getUri();    

    bx::Path fullDir(baseDir);
    fullDir.join(rootDir).normalizeSelf();
    
    DIR* d = opendir(fullDir.cstr());
    if (!d)
        return;

    dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        bx::Path filename(ent->d_name);
        bx::Path filepath(rootDir);
        filepath.joinUnix(filename.cstr());

        if (ent->d_type == DT_REG && fileIsValidTexture(filename.getFileExt())) {
            TextureItem* item = db->textures.push();
            item->type = TextureItemType::Image;
            bx::strlcpy(item->filepath, filepath.cstr(), sizeof(item->filepath));
            item->handle.reset();

            int num = db->textures.getCount();
            *textureIndices->push() = num - 1;
        }
    }
    closedir(d);
}


static SheetProject* createProject(const char* rootPath)
{
    bx::AllocatorI* alloc = termite::getHeapAlloc();
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
    bx::AllocatorI* alloc = termite::getHeapAlloc();
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
    termite::ResourceLibHelper resLib = db->resLib;

    int index = db->loadedIdx;
    if (index == db->textures.getCount())
        return;

    TextureItem* item = db->textures.itemPtr(index);
    if (!item->handle.isValid()) {
        if (item->type == TextureItemType::Image) {
            termite::LoadTextureParams params;
            params.flags |= termite::TextureFlag::MipPoint;
            item->handle = resLib.loadResource("image", item->filepath, &params);
        }
    }
    db->loadedIdx++;
}

static void destroyTextureDatabase(TextureDatabase* db)
{
    bx::AllocatorI* alloc = termite::getHeapAlloc();

    // Release all textures
    termite::ResourceLibHelper resLib = db->resLib;
    if (resLib.isValid()) {
        if (db->folderImg.isValid())
            resLib.unloadResource(db->folderImg);

        for (int i = 0; i < db->textures.getCount(); i++) {
            TextureItem* item = db->textures.itemPtr(i);
            if (item->type == TextureItemType::Image && item->handle.isValid()) {
                resLib.unloadResource(item->handle);
            }
        }
    }
    db->textures.destroy();
    BX_DELETE(alloc, db);
}

static bool showMessageBox(const char* name, const char* fmt, ...)
{
    if (theApp.gui->beginPopupModal(name, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        va_list args;
        va_start(args, fmt);
        theApp.gui->textV(fmt, args);
        va_end(args);

        if (theApp.gui->button("Ok", ImVec2(150.0f, 0)))
            theApp.gui->closeCurrentPopup();

        theApp.gui->endPopup();
        return true;
    }
    return false;
}

static bool saveSpriteSheet(const char* filepath, const SpriteSheet* sheetInfo, const uint8_t* pixels, int width, int height)
{
    if (!pixels)
        return false;
    assert(pixels);

    termite::IoDriverApi* io = termite::getBlockingIoDriver();
    bx::AllocatorI* alloc = termite::getHeapAlloc();

    // Save Image
    bx::Path filepathp(filepath);
    bx::Path filename = filepathp.getFilename();
    bx::Path filedir = filepathp.getDirectory();
    filename += ".tga";
    filepathp = io->getUri();
    filepathp.join(filedir.cstr()).join(filename.cstr());
    if (!stbi_write_tga(filepathp.cstr(), width, height, 4, pixels)) {
        return false;
    }
        
    // Save Descriptor file
    bx::MemoryBlock mem(alloc);
    bx::MemoryWriter writer(&mem);
    bx::Error err;

    termite::tsHeader header;
    header.sign = TSPRITE_SIGN;
    header.version = TSPRITE_VERSION;
    header.numAnims = sheetInfo->numAnims;
    header.numSprites = sheetInfo->numSprites;

    filepathp = filedir;
    filepathp.joinUnix(filename.cstr());
    bx::strlcpy(header.textureFilepath, filepathp.cstr(), sizeof(header.textureFilepath));
    writer.write(&header, sizeof(header), &err);

    // Write sprites
    termite::tsSprite* sprites = (termite::tsSprite*)alloca(sizeof(termite::tsSprite)*header.numSprites);
    if (header.numSprites) {
        assert(sprites);
        for (int i = 0; i < header.numSprites; i++) {
            const Sprite& sprite = sheetInfo->sprites[i];
            termite::tsSprite& tsprite = sprites[i];

            tsprite.tx0 = sprite.tx0;
            tsprite.ty0 = sprite.ty0;
            tsprite.tx1 = sprite.tx1;
            tsprite.ty1 = sprite.ty1;

            // Tag
            if (sprite.tag != -1 && sprite.tag < sheetInfo->numTags) {
                const FrameTag& tag = sheetInfo->tags[sprite.tag];
                tsprite.tag = bx::hashMurmur2A(tag.name, (uint32_t)strlen(tag.name));
            } else {
                tsprite.tag = 0;
            }
        }
        writer.write(sprites, sizeof(termite::tsSprite)*header.numSprites, &err);
    }

    // Write Anims
    termite::tsAnimation* anims = (termite::tsAnimation*)alloca(sizeof(termite::tsAnimation)*header.numAnims);
    if (header.numAnims) {
        assert(anims);
        for (int i = 0; i < header.numAnims; i++) {
            const SpriteAnimation& anim = sheetInfo->anims[i];
            termite::tsAnimation& tanim = anims[i];

            bx::strlcpy(tanim.name, anim.name, sizeof(tanim.name));
            tanim.startFrame = anim.frameStart;
            tanim.endFrame = anim.frameEnd;
            tanim.fps = anim.fps;
        }
        writer.write(anims, sizeof(termite::tsAnimation)*header.numAnims, &err);
    }

    // Write to file
    termite::MemoryBlock* block = termite::refMemoryBlockPtr(mem.more(), (uint32_t)writer.seek());
    bool r = io->write(filepath, block) > 0 ? true : false;
    termite::releaseMemoryBlock(block);

    return r;
}

static void generateSpriteSheet(Sprite* sprites, int numSprites, int width, int height)
{
    bx::AllocatorI* alloc = termite::getHeapAlloc();

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
        const TextureItem& tex = theApp.textureDb->textures[sprite.textureItem];
        termite::Texture* t = theApp.textureDb->resLib.getResourcePtr<termite::Texture>(tex.handle);

        rects[i].id = i;
        rects[i].w = t->info.width;
        rects[i].h = t->info.height;
    }

    stbrp_pack_rects(&ctx, rects, numSprites);

    theApp.project->pixels = (uint8_t*)BX_REALLOC(alloc, theApp.project->pixels, width*height*sizeof(uint32_t));
    if (!theApp.project->pixels) {
        theApp.gui->openPopup("msg");
        return;
    }
    uint8_t* destPixels = theApp.project->pixels;
    memset(destPixels, 0x00, width*height * sizeof(uint32_t));
        
    for (int i = 0; i < numSprites; i++) {
        if (!rects[i].was_packed) {
            BX_WARN("Increase the image size. Not all sprites fit the target image %dx%d", width, height);
            break;
        }

        const Sprite& sprite = sprites[i];
        const TextureItem& tex = theApp.textureDb->textures[sprite.textureItem];
        termite::Texture* t = theApp.textureDb->resLib.getResourcePtr<termite::Texture>(tex.handle);

        sprites[i].tx0 = float(rects[i].x) / float(width);
        sprites[i].ty0 = float(rects[i].y) / float(height);
        sprites[i].tx1 = sprites[i].tx0 + float(t->info.width)/float(width);
        sprites[i].ty1 = sprites[i].ty0 + float(t->info.height) / float(height);
        
        // Load image data
        termite::IoDriverApi* io = termite::getBlockingIoDriver();
        termite::MemoryBlock* imageData = io->read(tex.filepath);
        if (imageData) {
            int srcWidth, srcHeight, comp;
            stbi_uc* srcPixels = stbi_load_from_memory(imageData->data, imageData->size, &srcWidth, &srcHeight, &comp, 4);
            termite::releaseMemoryBlock(imageData);
            if (srcPixels) {
                termite::blitRawPixels(destPixels, rects[i].x, rects[i].y, width, height, srcPixels, 0, 0,
                                       t->info.width, t->info.height, sizeof(uint32_t));
                stbi_image_free(srcPixels);
            }   
        }
    }    

    // create texture for preview
    termite::GfxDriverApi* driver = termite::getGfxDriver();
    if (theApp.project->image.handle.isValid()) {
        driver->destroyTexture(theApp.project->image.handle);
    }
    theApp.project->image.handle = driver->createTexture2D(width, height, false, 1, termite::TextureFormat::RGBA8,
                                                      termite::TextureFlag::MinPoint | termite::TextureFlag::MagPoint,
                                                      driver->makeRef(theApp.project->pixels, width*height * sizeof(uint32_t), nullptr, nullptr));
    theApp.project->image.info.width = width;
    theApp.project->image.info.height = height;
}

static int showTexturesPopup(const char* popupName)
{
    int selectedIdx = -1;
    if (theApp.gui->beginPopupModal(popupName, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        if (theApp.gui->button("Close", ImVec2(0, 0)))
            theApp.gui->closeCurrentPopup();

        theApp.gui->beginChild("Gallery", ImVec2(300.0f, 300.0f), true, ImGuiWindowFlags_AlwaysAutoResize);
        const int numColumns = 4;
        theApp.gui->columns(numColumns, nullptr, false);

        int textureIdx = 0;
        for (int i = 0, c = theApp.textureDb->textures.getCount(); i < c; i += numColumns) {
            for (int k = 0; k < numColumns; k++) {
                if (textureIdx >= c)
                    break;

                const TextureItem& tex = theApp.textureDb->textures[textureIdx];
                if (tex.handle.isValid()) {
                    termite::TextureHandle* handle = &theApp.textureDb->resLib.getResourcePtr<termite::Texture>(tex.handle)->handle;
                    theApp.gui->pushIDInt(textureIdx);
                    if (theApp.gui->imageButton((ImTextureID)handle, ImVec2(64, 64), ImVec2(0, 0),
                                                ImVec2(1.0f, 1.0f), 1, ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, 1.0f))) 
                    {
                        selectedIdx = textureIdx;
                        theApp.gui->closeCurrentPopup();
                    }

                    if (theApp.gui->isItemHovered())
                        theApp.gui->setTooltip("%s", tex.filepath);
                    theApp.gui->popID();
                }

                theApp.gui->nextColumn();
                textureIdx++;
            }
        }

        theApp.gui->endChild();
        theApp.gui->endPopup();
    }

    return selectedIdx;
}

static bool showEditPopup(const char* name, const char* caption, char* value, size_t valueSize)
{
    static char text[256];
    bool r = false;
    if (theApp.gui->beginPopupModal(name, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        theApp.gui->inputText(caption, text, sizeof(text), 0, nullptr, nullptr);
        if (theApp.gui->button("Ok", ImVec2(100.0f, 0))) {
            bx::strlcpy(value, text, valueSize);
            theApp.gui->closeCurrentPopup();
            r = true;
        }
        theApp.gui->sameLine(0, -1.0f);
        if (theApp.gui->button("Cancel", ImVec2(100.0f, 0))) {
            theApp.gui->closeCurrentPopup();
        }
        theApp.gui->endPopup();
    }
    return r;
}


static void renderGui(float dt)
{
    bx::AllocatorI* alloc = termite::getHeapAlloc();
    bx::AllocatorI* tmpAlloc = termite::getTempAlloc();
    SpriteSheet* sheet = theApp.project->sheet;

    static bool opened = true;
    theApp.gui->begin("SheetMaker", &opened, 0);

    if (theApp.gui->button("New Sheet", ImVec2(150.0f, 0))) {
    }

    if (theApp.gui->button("Open", ImVec2(150.0f, 0))) {
    }

    if (theApp.gui->button("Save", ImVec2(150.0f, 0))) {
    }

    if (theApp.gui->button("Export", ImVec2(150.0f, 0))) {
        theApp.gui->openPopup("Export Spritesheet");
    }
    char sheetName[128];
    if (showEditPopup("Export Spritesheet", "Name", sheetName, sizeof(sheetName))) {
        bx::Path filepath("assets/spritesheets");
        filepath.joinUnix(sheetName);
        filepath += ".tsheet";
        saveSpriteSheet(filepath.cstr(), sheet, theApp.project->pixels, theApp.project->imageSize[0], theApp.project->imageSize[1]);
    }

    theApp.gui->inputInt2("Size", theApp.project->imageSize, 0);

    // Sprites
    if (theApp.gui->collapsingHeader("Sprites", nullptr, true, true)) {
        if (theApp.gui->button("Add", ImVec2(100.0f, 0))) {
            theApp.gui->openPopup("Add Texture");
        }

        int textureIdx = showTexturesPopup("Add Texture");
        if (textureIdx != -1) {
            if (theApp.textureDb->textures[textureIdx].type == TextureItemType::Directory) {
                // Add the whole directory to list
                bx::Array<int> indices;
                indices.create(128, 256, tmpAlloc);
                loadTextureInDirectory(theApp.textureDb, theApp.textureDb->textures[textureIdx].filepath, &indices);
                if (indices.getCount()) {
                    sheet->sprites = (Sprite*)BX_REALLOC(alloc, sheet->sprites, 
                        (sheet->numSprites + indices.getCount()) * sizeof(Sprite));
                    for (int i = 0; i < indices.getCount(); i++) {
                        Sprite& sprite = sheet->sprites[sheet->numSprites + i];
                        sprite.textureItem = indices[i];
                        sprite.tx0 = sprite.ty0 = sprite.tx1 = sprite.ty1 = -1.0f;
                        sprite.tag = -1;
                    }
                    sheet->numSprites += indices.getCount();
                }
                indices.destroy();
            } else {
                // Add to list
                sheet->sprites = (Sprite*)BX_REALLOC(alloc, sheet->sprites, (sheet->numSprites + 1) * sizeof(Sprite));
                assert(sheet->sprites);
                Sprite& sprite = sheet->sprites[sheet->numSprites];
                sprite.textureItem = textureIdx;
                sprite.tx0 = sprite.ty0 = sprite.tx1 = sprite.ty1 = -1.0f;
                sprite.tag = -1;
                sheet->numSprites++;
            }
        }

        theApp.gui->sameLine(0, -1.0f);
        if (theApp.gui->button("Remove", ImVec2(100.0f, 0)) && theApp.project->selectedSprite != -1) {
            int index = theApp.project->selectedSprite;
            int nextIndex = index + 1 < sheet->numSprites ? (index + 1) : index;
            if (index != nextIndex)
                memmove(&sheet->sprites[index], &sheet->sprites[nextIndex], (sheet->numSprites - nextIndex) * sizeof(Sprite));
            sheet->numSprites--;
            theApp.project->selectedSprite = sheet->numSprites ? index : -1;
        }

        // Sprite List
        if (sheet->numSprites) {
            char** names = (char**)BX_ALLOC(tmpAlloc, sizeof(char*)*sheet->numSprites);
            for (int i = 0; i < sheet->numSprites; i++) {
                const Sprite& sprite = sheet->sprites[i];
                const TextureItem& tex = theApp.textureDb->textures[sprite.textureItem];

                names[i] = (char*)BX_ALLOC(tmpAlloc, sizeof(char) * 128);
                bx::strlcpy(names[i], tex.filepath, 128);
            }
            theApp.gui->listBox("Sprites", &theApp.project->selectedSprite, (const char**)names, sheet->numSprites, -1);
            if (theApp.gui->isItemHovered() && theApp.project->selectedSprite != -1) {
                const Sprite& sprite = sheet->sprites[theApp.project->selectedSprite];
                const TextureItem& texItem = theApp.textureDb->textures[sprite.textureItem];
                termite::Texture* tex = theApp.textureDb->resLib.getResourcePtr<termite::Texture>(texItem.handle);
                termite::TextureHandle* handle = &tex->handle;
                float ratio = float(tex->info.width) / float(tex->info.height);

                theApp.gui->beginTooltip();
                theApp.gui->image((ImTextureID)handle, ImVec2(128.0f, 128.0f/ratio), ImVec2(0, 0), ImVec2(1.0f, 1.0f),
                             ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0, 0, 0, 0));
                theApp.gui->text("%dx%d Index=%d", tex->info.width, tex->info.height, theApp.project->selectedSprite);
                theApp.gui->endTooltip();
            }

            if (theApp.gui->button("Generate Sheet", ImVec2(150.0f, 0))) {
                generateSpriteSheet(sheet->sprites, sheet->numSprites, theApp.project->imageSize[0], theApp.project->imageSize[1]);
            }
        }
    }

    // Tags
    if (theApp.gui->collapsingHeader("Tags", nullptr, true, false)) {
        if (theApp.gui->button("Add Tag", ImVec2(150.0f, 0))) {
            theApp.gui->openPopup("Add Tag");
        }

        char name[32]; name[0] = 0;
        if (showEditPopup("Add Tag", "Name", name, sizeof(name))) {
            sheet->tags = (FrameTag*)BX_REALLOC(alloc, sheet->tags, (sheet->numTags + 1) * sizeof(FrameTag));
            assert(sheet->tags);
            FrameTag& tag = sheet->tags[sheet->numTags++];
            bx::strlcpy(tag.name, name, sizeof(tag.name));
        }
        theApp.gui->sameLine(0, -1);
        if (theApp.gui->button("Remove Tag", ImVec2(150.0f, 0)) && theApp.project->selectedTag != -1) {
            int index = theApp.project->selectedTag;
            int nextIndex = index + 1 < sheet->numTags ? (index + 1) : index;
            if (index != nextIndex)
                memmove(&sheet->tags[index], &sheet->tags[nextIndex], (sheet->numTags - nextIndex) * sizeof(FrameTag));
            sheet->numTags--;
            theApp.project->selectedTag = sheet->numTags ? index : -1;
        }
        
        // Tag List
        if (sheet->numTags) {
            char** names = (char**)BX_ALLOC(tmpAlloc, sizeof(char*)*sheet->numTags);
            for (int i = 0; i < sheet->numTags; i++) {
                const FrameTag& tag = sheet->tags[i];
                names[i] = (char*)BX_ALLOC(tmpAlloc, sizeof(char) * 32);
                bx::strlcpy(names[i], tag.name, 32);
            }
            theApp.gui->listBox("Tags", &theApp.project->selectedTag, (const char**)names, sheet->numTags, -1);
        }
    }

    // Animation Clips
    if (theApp.gui->collapsingHeader("Animation Clips", nullptr, true, true)) {
        if (theApp.gui->button("Add Clip", ImVec2(150.0f, 0))) {
            theApp.gui->openPopup("Add Clip");
        }

        char name[32];
        name[0] = 0;
        if (showEditPopup("Add Clip", "Name", name, sizeof(name))) {
            sheet->anims = (SpriteAnimation*)BX_REALLOC(alloc, sheet->anims, (sheet->numAnims + 1)*sizeof(SpriteAnimation));
            assert(sheet->anims);
            SpriteAnimation& anim = sheet->anims[sheet->numAnims++];
            memset(&anim, 0x00, sizeof(anim));
            anim.fps = 30;
            bx::strlcpy(anim.name, name, sizeof(anim.name));
        }

        theApp.gui->sameLine(0, -1);
        if (theApp.gui->button("Remove Clip", ImVec2(150.0f, 0)) && theApp.project->selectedAnim != -1) {
            int index = theApp.project->selectedSprite;
            int nextIndex = index + 1 < sheet->numSprites ? (index + 1) : index;
            if (index != nextIndex)
                memmove(&sheet->sprites[index], &sheet->sprites[nextIndex], (sheet->numSprites - nextIndex) * sizeof(Sprite));
            sheet->numSprites--;
            theApp.project->selectedAnim = sheet->numAnims ? index : -1;
        }

        // Animation list
        if (sheet->numAnims) {
            char** names = (char**)BX_ALLOC(tmpAlloc, sizeof(char*)*sheet->numAnims);
            for (int i = 0; i < sheet->numAnims; i++) {
                const SpriteAnimation& anim = sheet->anims[i];
                names[i] = (char*)BX_ALLOC(tmpAlloc, sizeof(char) * 32);
                bx::strlcpy(names[i], anim.name, 32);
            }

            theApp.gui->listBox("Clips", &theApp.project->selectedAnim, (const char**)names, sheet->numAnims, -1);
        }

        if (theApp.project->selectedAnim != -1 && sheet->numSprites) {
            static bool playChecked = false;
            static int frameIdx = 0;

            SpriteAnimation& anim = sheet->anims[theApp.project->selectedAnim];
            theApp.gui->sliderInt("Fps", &anim.fps, 1, 60, "%.0f");
            theApp.gui->sliderInt("Start", &anim.frameStart, 0, sheet->numSprites - 1, "%.0f");
            anim.frameEnd = bx::uint32_max(anim.frameEnd, anim.frameStart);
            theApp.gui->sliderInt("End", &anim.frameEnd, anim.frameStart, sheet->numSprites, "%.0f");
            theApp.gui->checkbox("Play", &playChecked);
            if (!playChecked) {
                theApp.gui->sliderInt("Frame", &frameIdx, 0, sheet->numSprites - 1, "%.0f");
                if (theApp.project->selectedTag != -1)  {
                    if (theApp.gui->button("Tag", ImVec2(150.0f, 0))) {
                        sheet->sprites[frameIdx].tag = theApp.project->selectedTag;
                    }
                    theApp.gui->sameLine(0, -1);
                    if (theApp.gui->button("Clear Tag", ImVec2(150.0f, 0))) {
                        sheet->sprites[frameIdx].tag = -1;
                    }
                }
            }
                
            float availWidth = theApp.gui->getContentRegionAvailWidth();
            float ratio = float(theApp.project->image.info.width) / float(theApp.project->image.info.height);

            if (theApp.project->image.handle.isValid() && anim.frameEnd - anim.frameStart > 0) {
                if (playChecked) {
                    static float animTime = 0;

                    // Play sprite animation (looped)
                    int frameCount = anim.frameEnd - anim.frameStart;
                    float totalTime = float(frameCount) / float(anim.fps);
                    animTime = bx::fwrap(animTime + dt, totalTime);
                    float animFrameTime = 1.0f / float(anim.fps);

                    frameIdx = std::min<int>(anim.frameStart + int(animTime / animFrameTime), anim.frameEnd - 1);
                } 

                const Sprite& sprite = sheet->sprites[frameIdx];
                if (sprite.tag != -1 && sprite.tag < sheet->numTags) {
                    theApp.gui->textColored(ImVec4(1.0f, 0, 0, 1.0f), "Tag: %s", sheet->tags[sprite.tag].name);
                } else {
                    theApp.gui->textColored(ImVec4(0.2f, 0.2f, 0.2f, 1.0f), "Not Tagged");
                }

                float w = (sprite.tx1 - sprite.tx0)*float(theApp.project->image.info.width);
                float h = (sprite.ty1 - sprite.ty0)*float(theApp.project->image.info.height);
                float ratio = w / h;
                
                theApp.gui->image((ImTextureID)&theApp.project->image.handle, ImVec2(availWidth, availWidth / ratio),
                                  ImVec2(sprite.tx0, sprite.ty0), ImVec2(sprite.tx1, sprite.ty1), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                                  ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
        }
    }

    theApp.gui->end();
}

static void update(float dt)
{
    static float loadTextureElapsed = 0;
    if (loadTextureElapsed >= 0.0f) {
        loadTexturesIterative(theApp.textureDb);
        loadTextureElapsed = 0;
    }
    loadTextureElapsed += dt;

    renderGui(dt);

    float imageWidth = float(theApp.project->imageSize[0]);
    float imageHeight = float(theApp.project->imageSize[1]);
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
    
    termite::vgBegin(theApp.vg, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (theApp.project->image.handle.isValid()) {
        vgImageRect(theApp.vg, termite::rectfwh(imageX, imageY, imageWidth, imageHeight), &theApp.project->image);
    }
    termite::vgEnd(theApp.vg);
    
    nvgBeginFrame(theApp.nvg, WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f);

    // Image Border
    nvgBeginPath(theApp.nvg);
    nvgStrokeWidth(theApp.nvg, 1.0f);
    nvgStrokeColor(theApp.nvg, nvgRGB(128, 128, 128));
    nvgRect(theApp.nvg, imageX - 1.0f, imageY - 1.0f, imageWidth + 2.0f, imageHeight + 2.0f);
    nvgStroke(theApp.nvg);

    // Sprite Border
    if (theApp.project->selectedSprite != -1) {
        const Sprite& sprite = theApp.project->sheet->sprites[theApp.project->selectedSprite];
        if (sprite.tx0 >= 0.0f && sprite.ty0 >= 0.0f) {
            float x = imageWidth*sprite.tx0;
            float y = imageHeight*sprite.ty0;
            float w = (sprite.tx1 - sprite.tx0)*imageWidth;
            float h = (sprite.ty1 - sprite.ty0)*imageHeight;
            float ratio = w / h;
            nvgBeginPath(theApp.nvg);
            nvgStrokeWidth(theApp.nvg, 1.0f);
            nvgStrokeColor(theApp.nvg, nvgRGB(0, 128, 0));
            nvgRect(theApp.nvg, imageX + x, imageY + y, w, h);
            nvgStroke(theApp.nvg);
        }
    }

    nvgEndFrame(theApp.nvg);
}

static void showHelp()
{
    puts("sheetmaker - Termite engine Sprite tool");
    puts("Arguments");
    puts("  -p --project Project root path");
    puts("");
}

static void onFileModified(termite::ResourceLib* resLib, const char* uri, void* userParam)
{
    BX_VERBOSE("File changed: %s", uri);
}

int main(int argc, char* argv[])
{
    bx::CommandLine cmd(argc, argv);

    if (cmd.hasArg('h', "--help")) {
        showHelp();
        return 0;
    }
    bx::enableLogToFileHandle(stdout, stderr);

    // Get Project Root from command line
    // If Not provided, choose current path
    char curDir[256];
    bx::pwd(curDir, sizeof(curDir));
    const char* projectRoot = cmd.findOption('p', "project", curDir);

    if (SDL_Init(0) != 0) {
        BX_FATAL("SDL Init failed");
        return -1;
    }

    termite::Config conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    theApp.wnd = SDL_CreateWindow("SheetMaker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!theApp.wnd) {
        BX_FATAL("SDL window creation failed");
        termite::shutdown();
        return -1;
    }
    // Initialize Engine
    termite::GfxPlatformData platformData;
    termite::sdlGetNativeWindowHandle(theApp.wnd, &platformData.nwh, &platformData.ndt);
    conf.gfxWidth = WINDOW_WIDTH;
    conf.gfxHeight = WINDOW_HEIGHT;
    conf.engineFlags = termite::InitEngineFlags::None;
    conf.gfxDriverFlags = uint32_t(termite::GfxResetFlag::VSync);
    strcpy(conf.uiIniFilename, "sheetmaker.ini");
    conf.pageSize = 64;

    bx::Path projectDir(projectRoot);
    projectDir.normalizeSelf();
    bx::Path dataDir(projectDir);
    bx::strlcpy(conf.dataUri, dataDir.cstr(), sizeof(conf.dataUri));
    termite::sdlMapImGuiKeys(&conf);

    if (termite::initialize(conf, update, &platformData)) {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        SDL_DestroyWindow(theApp.wnd);
        SDL_Quit();
        return -1;
    }

    // Initialize sheetmaker stuff
    bx::AllocatorI* alloc = termite::getHeapAlloc();
    termite::IoDriverApi* io = termite::getAsyncIoDriver();
    termite::ResourceLibHelper resLib = termite::getDefaultResourceLib();

    resLib.setFileModifiedCallback(onFileModified, nullptr);

    theApp.gui = (termite::ImGuiApi_v0*)termite::getEngineApi(uint16_t(termite::ApiId::ImGui), 0);    
    theApp.nvg = nvgCreate(1, 254, termite::getGfxDriver(), 
                           (termite::GfxApi_v0*)termite::getEngineApi(uint16_t(termite::ApiId::Gfx), 0), alloc);
    theApp.textureDb = createTextureDatabase(io->getUri(), "library/sprites", resLib);
    theApp.vg = termite::createVectorGfxContext(1);
    
    if (!theApp.textureDb) {
        BX_FATAL("Could not load texture database");
        termite::shutdown();
        SDL_DestroyWindow(theApp.wnd);
        SDL_Quit();
        return -1;
    }
    theApp.project = createProject(projectDir.cstr());

    SDL_Event e;
    while (true) {
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                break;
            termite::sdlHandleEvent(e);
        }
        termite::doFrame();
    }

    termite::destroyVectorGfxContext(theApp.vg);
    destroyProject(theApp.project);
    destroyTextureDatabase(theApp.textureDb);
    nvgDelete(theApp.nvg);
    termite::shutdown();
    SDL_DestroyWindow(theApp.wnd);
    SDL_Quit();

    return 0;
}

