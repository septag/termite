#include "pch.h"
#include "io_driver.h"
#include "gfx_font.h"
#include "gfx_texture.h"
#include "error_report.h"
#include "gfx_driver.h"
#include "math.h"
#include "internal.h"

#include "bx/hash.h"
#include "bx/uint32_t.h"
#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/hash_table.h"
#include "bxx/pool.h"
#include "bx/string.h"

#include "utf8/utf8.h"

#include TEE_MAKE_SHADER_PATH(shaders_h, font_normal.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_normal.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_normal_shadow.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_normal_shadow.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_normal_outline.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_normal_outline.fso)

#include TEE_MAKE_SHADER_PATH(shaders_h, font_df.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_df.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_df_shadow.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_df_shadow.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_df_outline.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, font_df_outline.fso)

#define MAX_FONT_PAGES 1

// FNT file format (Binary)
#define FNT_SIGN "BMF"

namespace tee
{
#pragma pack(push, 1)
    struct fntInfo_t
    {
        int16_t font_size;
        int8_t bit_field;
        uint8_t charset;
        uint16_t stretch_h;
        uint8_t aa;
        uint8_t padding_up;
        uint8_t padding_right;
        uint8_t padding_dwn;
        uint8_t padding_left;
        uint8_t spacing_horz;
        uint8_t spacing_vert;
        uint8_t outline;
        /* name (str) */
    };

    struct fntCommon_t
    {
        uint16_t line_height;
        uint16_t base;
        uint16_t scale_w;
        uint16_t scale_h;
        uint16_t pages;
        int8_t bit_field;
        uint8_t alpha_channel;
        uint8_t red_channel;
        uint8_t green_channel;
        uint8_t blue_channel;
    };

    struct fntPage_t
    {
        char path[255];
    };

    struct fntChar_t
    {
        uint32_t id;
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        int16_t xoffset;
        int16_t yoffset;
        int16_t xadvance;
        uint8_t page;
        uint8_t channel;
    };

    struct fntKernPair_t
    {
        uint32_t first;
        uint32_t second;
        int16_t amount;
    };

    struct fntBlock_t
    {
        uint8_t id;
        uint32_t size;
    };
#pragma pack(pop)

    struct UnicodeReplaceRule
    {
        uint32_t code;
        uint32_t after;     // If character comes after another word
        uint32_t before;    // If character comes in before another word
        uint32_t middle;    // if character comes in the middle
        UnicodeReplaceRule(uint32_t _code, uint32_t _after, uint32_t _before = 0, uint32_t _middle = 0) :
            code(_code),
            after(_after),
            before(_before),
            middle(_middle)
        {
        }
    };

    // Persian glyph bindings
    static const UnicodeReplaceRule kPersianGlyphs[] = {
        UnicodeReplaceRule(1575, 65166), // Alef
        UnicodeReplaceRule(1576, 65168, 65169, 65170), // Be
        UnicodeReplaceRule(1662, 64343, 64344, 64345), // Pe
        UnicodeReplaceRule(1578, 65174, 65175, 65176), // Te
        UnicodeReplaceRule(1579, 65178, 65179, 65180), // Tse
        UnicodeReplaceRule(1580, 65182, 65183, 65184), // jim
        UnicodeReplaceRule(1670, 64379, 64380, 64381), // che
        UnicodeReplaceRule(1581, 65186, 65187, 65188), // He
        UnicodeReplaceRule(1582, 65190, 65191, 65192), // Khe
        UnicodeReplaceRule(1583, 65194), // De
        UnicodeReplaceRule(1584, 65196), // Dal zal
        UnicodeReplaceRule(1585, 65198), // Re
        UnicodeReplaceRule(1586, 65200), // Ze
        UnicodeReplaceRule(1688, 64395), // Zhe
        UnicodeReplaceRule(1587, 65202, 65203, 65204), // Se
        UnicodeReplaceRule(1588, 65206, 65207, 65208), // She
        UnicodeReplaceRule(1589, 65210, 65211, 65212), // Sa, zaad
        UnicodeReplaceRule(1590, 65214, 65215, 65216), // Ze zaad
        UnicodeReplaceRule(1591, 65218, 65219, 65220), // Taa daste dar
        UnicodeReplaceRule(1592, 65222, 65223, 65224), // Ze daste dar
        UnicodeReplaceRule(1593, 65226, 65227, 65228), // Eyn
        UnicodeReplaceRule(1594, 65230, 65231, 65232), // Gheyn
        UnicodeReplaceRule(1601, 65234, 65235, 65236), // Fe
        UnicodeReplaceRule(1602, 65238, 65239, 65240), // Ghaf
        UnicodeReplaceRule(1705, 65242, 65243, 65244), // Ke
        UnicodeReplaceRule(1711, 64403, 64404, 64405), // Gaf
        UnicodeReplaceRule(1604, 65246, 65247, 65248), // Laam
        UnicodeReplaceRule(1605, 65250, 65251, 65252), // mim
        UnicodeReplaceRule(1606, 65254, 65255, 65256), // Ne
        UnicodeReplaceRule(1608, 65262), // Ve
        UnicodeReplaceRule(1607, 65258, 65259, 65260), // He
        UnicodeReplaceRule(1740, 65266, 65267, 65268)  // Ye
    };

    class FontLoader : public AssetLibCallbacksI
    {
    public:
        bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
        void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
        void onReload(AssetHandle handle, bx::AllocatorI* alloc) override;
    };

    struct FontKerning
    {
        uint16_t secondCharId;
        float amount;
    };

    struct Font
    {
        char name[32];
        uint16_t size;
        FontFlags::Bits flags;
        uint16_t lineHeight;
        uint16_t base;
        uint16_t scaleW;
        uint16_t scaleH;
        uint16_t charWidth;
        int16_t padding[4];
        int16_t spacing[2];
        int numPages;
        AssetHandle texHandles[MAX_FONT_PAGES];
        int numGlyphs;
        FontGlyph* glyphs;
        int numKerns;
        FontKerning* kerns;
        bx::HashTable<int, uint16_t> glyphTable;    // CharId -> index to glyphs

        Font() : 
            glyphTable(bx::HashTableType::Immutable)
        {
            name[0] = 0;
            size = 0;
            flags = 0;
            lineHeight = 0;
            base = 0;
            scaleW = scaleH = 0;
            numPages = 0;
            numGlyphs = 0;
            glyphs = nullptr;
            numKerns = 0;
            kerns = nullptr;
            for (int i = 0; i < MAX_FONT_PAGES; i++)
                texHandles[i].reset();
        }
    };

    struct TextVertex
    {
        float x;
        float y;
        float alpha;
        float tx;
        float ty;

        static void init()
        {
            gfx::beginDecl(&Decl);
            gfx::addAttrib(&Decl, VertexAttrib::Position, 3, VertexAttribType::Float);
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
            gfx::endDecl(&Decl);
        }

        static VertexDecl Decl;
    };
    VertexDecl TextVertex::Decl;


    struct TextDraw
    {
        bx::AllocatorI* alloc;
        AssetHandle fontHandle;
        int maxChars;
        int numChars;
        TextVertex* verts;
        uint16_t* indices;
        uint32_t mtxHash;
        vec2_t screenSize;
        mat4_t viewProjMtx;
        mat4_t transformMtx;

        TextDraw(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            maxChars(0),
            numChars(0),
            verts(nullptr),
            indices(nullptr),
            mtxHash(0)
        {
        }
    };

    struct FontManager
    {
        bx::AllocatorI* alloc;
        FontLoader loader;
        Font* failFont;
        Font* asyncFont;
        bx::Pool<TextDraw> batchPool;
        vec2_t refScreenSize;
        ProgramHandle normalProg;
        ProgramHandle normalShadowProg;
        ProgramHandle normalOutlineProg;
        ProgramHandle dfProg;
        ProgramHandle dfShadowProg;
        ProgramHandle dfOutlineProg;
        UniformHandle uTexture;
        UniformHandle uParams;
        UniformHandle uTextureSize;
        UniformHandle uShadowColor;
        UniformHandle uOutlineColor;
        UniformHandle uTransformMtx;
        UniformHandle uColor;

        FontManager(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            failFont(nullptr),
            asyncFont(nullptr)
        {
        }
    };

    static FontManager* gFontMgr = nullptr;    

    static Font* createDummyFont()
    {
        Font* font = BX_NEW(gFontMgr->alloc, Font);
        font->lineHeight = 20;
        font->scaleW = 1;
        font->scaleH = 1;
        font->numPages = 1;
        font->texHandles[0] = asset::getAsyncHandle("texture");
        font->numGlyphs = 0;
        font->numKerns = 0;
        return font;
    }

    bool gfx::initFontSystem(bx::AllocatorI* alloc, const vec2_t refScreenSize)
    {
        BX_ASSERT(alloc);

        if (gFontMgr) {
            BX_ASSERT(false);
            return false;
        }

        gFontMgr = BX_NEW(alloc, FontManager)(alloc);
        if (!gFontMgr)
            return false;

        if (!gFontMgr->batchPool.create(16, alloc))
            return false;            

        TextVertex::init();

        gFontMgr->failFont = createDummyFont();
        gFontMgr->asyncFont = createDummyFont();

        if (!initFontSystemGraphics()) {
            return false;
        }

        gFontMgr->refScreenSize = refScreenSize;

        return true;
    }

    void gfx::shutdownFontSystem()
    {
        if (!gFontMgr)
            return;

        bx::AllocatorI* alloc = gFontMgr->alloc;

        shutdownFontSystemGraphics();

        if (gFontMgr->failFont)
            gFontMgr->loader.unloadObj(uintptr_t(gFontMgr->failFont), gFontMgr->alloc);
        if (gFontMgr->asyncFont)
            gFontMgr->loader.unloadObj(uintptr_t(gFontMgr->asyncFont), gFontMgr->alloc);

        gFontMgr->batchPool.destroy();
        BX_DELETE(alloc, gFontMgr);
        gFontMgr = nullptr;
    }

    bool gfx::initFontSystemGraphics()
    {
        GfxDriver* gDriver = getGfxDriver();

        gFontMgr->normalProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_normal_vso, sizeof(font_normal_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_normal_fso, sizeof(font_normal_fso), nullptr, nullptr)), true);
        if (!gFontMgr->normalProg.isValid()) {
            TEE_ERROR("Creating font shader failed");
            return false;
        }
        gFontMgr->normalShadowProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_normal_shadow_vso, sizeof(font_normal_shadow_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_normal_shadow_fso, sizeof(font_normal_shadow_fso), nullptr, nullptr)), true);
        if (!gFontMgr->normalShadowProg.isValid()) {
            TEE_ERROR("Creating font shader failed");
            return false;
        }

        gFontMgr->normalOutlineProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_normal_outline_vso, sizeof(font_normal_outline_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_normal_outline_fso, sizeof(font_normal_outline_fso), nullptr, nullptr)), true);
        if (!gFontMgr->normalOutlineProg.isValid()) {
            TEE_ERROR("Creating font shader failed");
            return false;
        }

        gFontMgr->dfProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_df_vso, sizeof(font_df_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_df_fso, sizeof(font_df_fso), nullptr, nullptr)), true);
        if (!gFontMgr->dfProg.isValid()) {
            TEE_ERROR("Creating font shader failed");
            return false;
        }

        gFontMgr->dfShadowProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_df_shadow_vso, sizeof(font_df_shadow_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_df_shadow_fso, sizeof(font_df_shadow_fso), nullptr, nullptr)), true);
        if (!gFontMgr->dfShadowProg.isValid()) {
            TEE_ERROR("Creating font shader failed");
            return false;
        }

        gFontMgr->dfOutlineProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_df_outline_vso, sizeof(font_df_outline_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_df_outline_fso, sizeof(font_df_outline_fso), nullptr, nullptr)), true);
        if (!gFontMgr->dfOutlineProg.isValid()) {
            TEE_ERROR("Creating font shader failed");
            return false;
        }

        gFontMgr->uTexture = gDriver->createUniform("u_texture", UniformType::Int1, 1);
        gFontMgr->uParams = gDriver->createUniform("u_params", UniformType::Vec4, 1);
        gFontMgr->uTextureSize = gDriver->createUniform("u_textureSize", UniformType::Vec4, 1);
        gFontMgr->uShadowColor = gDriver->createUniform("u_shadowColor", UniformType::Vec4, 1);
        gFontMgr->uOutlineColor = gDriver->createUniform("u_outlineColor", UniformType::Vec4, 1);
        gFontMgr->uTransformMtx = gDriver->createUniform("u_transformMtx", UniformType::Mat4, 1);
        gFontMgr->uColor = gDriver->createUniform("u_color", UniformType::Vec4, 1);
        return true;

    }

    void gfx::shutdownFontSystemGraphics()
    {

        GfxDriver* gDriver = getGfxDriver();
        if (gFontMgr->normalProg.isValid())
            gDriver->destroyProgram(gFontMgr->normalProg);
        if (gFontMgr->normalShadowProg.isValid())
            gDriver->destroyProgram(gFontMgr->normalShadowProg);
        if (gFontMgr->normalOutlineProg.isValid())
            gDriver->destroyProgram(gFontMgr->normalOutlineProg);
        if (gFontMgr->dfProg.isValid())
            gDriver->destroyProgram(gFontMgr->dfProg);
        if (gFontMgr->dfShadowProg.isValid())
            gDriver->destroyProgram(gFontMgr->dfShadowProg);
        if (gFontMgr->dfOutlineProg.isValid())
            gDriver->destroyProgram(gFontMgr->dfOutlineProg);

        if (gFontMgr->uTexture.isValid())
            gDriver->destroyUniform(gFontMgr->uTexture);
        if (gFontMgr->uParams.isValid())
            gDriver->destroyUniform(gFontMgr->uParams);
        if (gFontMgr->uColor.isValid())
            gDriver->destroyUniform(gFontMgr->uColor);
        if (gFontMgr->uShadowColor.isValid())
            gDriver->destroyUniform(gFontMgr->uShadowColor);
        if (gFontMgr->uOutlineColor.isValid())
            gDriver->destroyUniform(gFontMgr->uOutlineColor);
        if (gFontMgr->uTransformMtx.isValid())
            gDriver->destroyUniform(gFontMgr->uTransformMtx);
        if (gFontMgr->uTextureSize.isValid())
            gDriver->destroyUniform(gFontMgr->uTextureSize);
    }

    void gfx::registerFontToAssetLib()
    {
        AssetTypeHandle handle;
        handle = asset::registerType("font", &gFontMgr->loader, sizeof(LoadFontParams),
                                      uintptr_t(gFontMgr->failFont), uintptr_t(gFontMgr->asyncFont));
        BX_ASSERT(handle.isValid());
    }

    static Font* loadFontText(const MemoryBlock* mem, const char* filepath, const LoadFontParams& params, bx::AllocatorI* alloc)
    {
        bx::AllocatorI* tmpAlloc = getTempAlloc();
        char* strbuff = (char*)BX_ALLOC(tmpAlloc, mem->size + 1);
        memcpy(strbuff, mem->data, mem->size);
        strbuff[mem->size] = 0;

        int numGlyphs = 0;
        int numKernings = 0;
        FontGlyph* glyphs = nullptr;
        FontKerning* kernings = nullptr;
        bx::Path texFilepath;
        int lineHeight, base, scaleW, scaleH, size;
        char name[32];
        uint32_t flags = 0;
        uint16_t charWidth = 0;
        int16_t padding[4];
        int16_t spacing[2];

        auto readKeyValue = [](char* token, char* key, char* value, size_t maxChars) {
            char* equal = strchr(token, '=');
            if (equal) {
                bx::strCopy(key, (int)bx::min<size_t>(maxChars, size_t(equal-token)+1), token);
                if (*(equal + 1) != '"')
                    bx::strCopy(value, (int)maxChars, equal + 1);
                else
                    bx::strCopy(value, (int)maxChars, equal + 2);
                size_t vlen = strlen(value);
                if (value[vlen-1] == '"')
                    value[vlen-1] = 0;
            } else {
                key[0] = 0;
                value[0] = 0;
            }
        };

        auto parseNumbers = [](char* value, int16_t* numbers, int maxNumbers)->int {
            int index = 0;
            char snum[16];
            char* token = strchr(value, ',');
            while (token) {
                bx::strCopy(snum, (int)bx::min<size_t>(sizeof(snum), size_t(token-value)+1), value);
                if (index < maxNumbers)
                    numbers[index++] = (int16_t)bx::toInt(snum);
                value = token + 1;
                token = strchr(value, ',');
            }
            bx::strCopy(snum, sizeof(snum), value);
            if (index < maxNumbers)
                numbers[index++] = (int16_t)bx::toInt(snum);
            return index;
        };

        auto readInfo = [&name, &size, &flags, &padding, &spacing, readKeyValue, parseNumbers]() {
            char key[32], value[32];
            char* token = strtok(nullptr, " ");
            while (token) {
                readKeyValue(token, key, value, 32);
                if (strcmp(key, "face") == 0) {
                    bx::strCopy(name, 32, value);
                } else if (strcmp(key, "size") == 0) {
                    size = bx::toInt(value);
                } else if (strcmp(key, "bold") == 0) {
                    flags |= FontFlags::Bold;
                } else if (strcmp(key, "italic") == 0) {
                    flags |= FontFlags::Italic;
                } else if (strcmp(key, "padding") == 0) {
                    parseNumbers(value, padding, 4);
                } else if (strcmp(key, "spacing") == 0) {
                    parseNumbers(value, spacing, 2);
                }
                token = strtok(nullptr, " ");
            }
        };

        auto readCommon = [&lineHeight, &base, &scaleW, &scaleH, readKeyValue]() {
            char key[32], value[32];
            char* token = strtok(nullptr, " ");
            while (token) {
                readKeyValue(token, key, value, 32);
                if (strcmp(key, "lineHeight") == 0) {
                    lineHeight = bx::toInt(value);
                } else if (strcmp(key, "base") == 0) {
                    base = bx::toInt(value);
                } else if (strcmp(key, "scaleW") == 0) {
                    scaleW = bx::toInt(value);
                } else if (strcmp(key, "scaleH") == 0) {
                    scaleH = bx::toInt(value);
                }
                token = strtok(nullptr, " ");
            }
        };
         
        auto readPage = [&texFilepath, filepath, readKeyValue]() {
            char key[32], value[32];
            char* token = strtok(nullptr, " ");
            while (token) {
                readKeyValue(token, key, value, 32);
                if (strcmp(key, "file") == 0) {
                    texFilepath = bx::Path(filepath).getDirectory();
                    texFilepath.joinUnix(value);                    
                }
                token = strtok(nullptr, " ");
            }
        };

        auto readChars = [&numGlyphs, readKeyValue]() {
            char key[32], value[32];
            char* token = strtok(nullptr, " ");
            readKeyValue(token, key, value, 32);
            if (strcmp(key, "count") == 0)
                numGlyphs = bx::toInt(value);
        };

        auto readChar = [readKeyValue, &charWidth](FontGlyph& g) {
            char key[32], value[32];
            char* token = strtok(nullptr, " ");
            while (token) {
                readKeyValue(token, key, value, 32);
                if (strcmp(key, "id") == 0) {
                    uint32_t charId = (uint32_t)bx::toInt(value);
                    BX_ASSERT(charId < UINT16_MAX);
                    g.charId = (uint16_t)charId;
                } else if (strcmp(key, "x") == 0) {
                    g.x = (float)bx::toInt(value);
                } else if (strcmp(key, "y") == 0) {
                    g.y = (float)bx::toInt(value);
                } else if (strcmp(key, "width") == 0) {
                    g.width = (float)bx::toInt(value);
                } else if (strcmp(key, "height") == 0) {
                    g.height = (float)bx::toInt(value);
                } else if (strcmp(key, "xoffset") == 0) {
                    g.xoffset = (float)bx::toInt(value);
                } else if (strcmp(key, "yoffset") == 0) {
                    g.yoffset = (float)bx::toInt(value);
                } else if (strcmp(key, "xadvance") == 0) {
                    int xadvance = bx::toInt(value);
                    g.xadvance = (float)xadvance;
                    charWidth = bx::max<uint16_t>(charWidth, (uint16_t)xadvance);
                }
                token = strtok(nullptr, " ");
            }
        };

        auto readKernings = [&numKernings, readKeyValue]() {
            char key[32], value[32];
            char* token = strtok(nullptr, " ");
            readKeyValue(token, key, value, 32);
            if (strcmp(key, "count") == 0)
                numKernings = bx::toInt(value);
        };

        auto readKerning = [readKeyValue](FontKerning& k, int kernIdx, FontGlyph* glyphs, int numGlyphs) {
            char key[32], value[32];
            int firstGlyphIdx = -1;
            char* token = strtok(nullptr, " ");
            while (token) {
                readKeyValue(token, key, value, 32);
                if (strcmp(key, "first") == 0) {
                    uint16_t firstCharId = (uint16_t)bx::toInt(value);
                    for (int i = 0; i < numGlyphs; i++) {
                        if (glyphs[i].charId != firstCharId)
                            continue;
                        firstGlyphIdx = i;
                        break;
                    }
                } else if (strcmp(key, "second") == 0) {
                    k.secondCharId = (uint16_t)bx::toInt(value);
                } else if (strcmp(key, "amount") == 0) {
                    k.amount = (float)bx::toInt(value);
                } 
                token = strtok(nullptr, " ");
            }

            BX_ASSERT(firstGlyphIdx != -1);
            if (glyphs[firstGlyphIdx].numKerns == 0)
                glyphs[firstGlyphIdx].kernIdx = kernIdx;
            ++glyphs[firstGlyphIdx].numKerns;
        };
        
        char line[1024];
        char* eol = strchr(strbuff, '\n');
        int kernIdx = 0;
        int charIdx = 0;
        while (eol) {
            bx::strCopy(line, (int)bx::min<size_t>(sizeof(line), size_t(eol - strbuff)+1), strbuff);
            size_t lineLen = strlen(line);
            if (lineLen > 0) {
                if (line[lineLen-1] == '\r')
                    line[lineLen-1] = 0;

                char* token = strtok(line, " ");
                // Read line descriptors
                if (strcmp(token, "info") == 0) {
                    readInfo();
                } else if (strcmp(token, "common") == 0) {
                    readCommon();
                } else if (strcmp(token, "page") == 0) {
                    readPage();
                } else if (strcmp(token, "chars") == 0) {
                    readChars();
                    BX_ASSERT(numGlyphs > 0);
                    glyphs = (FontGlyph*)BX_ALLOC(tmpAlloc, sizeof(FontGlyph)*numGlyphs);
                    BX_ASSERT(glyphs);
                    bx::memSet(glyphs, 0x00, sizeof(FontGlyph)*numGlyphs);
                } else if (strcmp(token, "char") == 0) {
                    BX_ASSERT(charIdx < numGlyphs);
                    readChar(glyphs[charIdx++]);
                } else if (strcmp(token, "kernings") == 0) {
                    readKernings();
                    if (numKernings > 0) {
                        kernings = (FontKerning*)BX_ALLOC(tmpAlloc, sizeof(FontKerning)*numKernings);
                        BX_ASSERT(kernings);
                        bx::memSet(kernings, 0x00, sizeof(FontKerning)*numKernings);
                    }
                } else if (strcmp(token, "kerning") == 0) {
                    BX_ASSERT(kernIdx < numKernings);
                    readKerning(kernings[kernIdx], kernIdx, glyphs, numGlyphs);
                    kernIdx++;
                }
            }

            strbuff = eol + 1;
            eol = strchr(strbuff, '\n');
        }

        // Create Font
        size_t totalSz = sizeof(Font) +
            numGlyphs*sizeof(FontGlyph) +
            numKernings*sizeof(FontKerning) +
            bx::HashTable<int, uint16_t>::GetImmutableSizeBytes(numGlyphs);
        uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSz);
        Font* font = new(buff) Font;
        if (!font)
            return nullptr;
        buff += sizeof(Font);
        font->glyphs = (FontGlyph*)buff;
        buff += numGlyphs * sizeof(FontGlyph);
        font->kerns = (FontKerning*)buff;
        buff += numKernings*sizeof(FontKerning);

        // Hashtable for characters
        if (!font->glyphTable.createWithBuffer(numGlyphs, buff)) {
            BX_ASSERT(false);
            return nullptr;
        }
        for (int i = 0; i < numGlyphs; i++)
            font->glyphTable.add(glyphs[i].charId, i);

        strcpy(font->name, name);
        font->base = base;
        font->lineHeight = lineHeight;
        font->scaleH = scaleH;
        font->scaleW = scaleW;
        font->flags = flags;
        font->size = size;
        font->flags |= params.flags;

        font->numGlyphs = numGlyphs;
        font->numKerns = numKernings;
        font->numPages = 1;
        font->charWidth = charWidth;
        memcpy(font->padding, padding, sizeof(padding));
        memcpy(font->spacing, spacing, sizeof(spacing));

        if (!texFilepath.isEmpty()) {
            LoadTextureParams tparams;
            tparams.flags = TextureFlag::U_Clamp | TextureFlag::V_Clamp;
            tparams.generateMips = !(params.flags & FontFlags::DistantField) ? params.generateMips : false;
            font->texHandles[0] = asset::load("texture", texFilepath.cstr(), &tparams, 0,
                                               alloc == gFontMgr->alloc ? nullptr : alloc);
        }

        memcpy(font->glyphs, glyphs, numGlyphs*sizeof(FontGlyph));
        if (numKernings > 0)
            memcpy(font->kerns, kernings, numKernings*sizeof(FontKerning));

        return font;
    }

    static Font* loadFontBinary(const MemoryBlock* mem, const char* filepath, const LoadFontParams& params, bx::AllocatorI* alloc)
    {
        bx::Error err;
        bx::MemoryReader ms(mem->data, mem->size);

        // Sign
        char sign[4];
        ms.read(sign, 3, &err);  sign[3] = 0;
        if (strcmp(FNT_SIGN, sign) != 0) {
            TEE_ERROR("Loading font '%s' failed: Invalid FNT (bmfont) binary file", filepath);
            return nullptr;
        }

        // File version
        uint8_t fileVersion;
        ms.read(&fileVersion, sizeof(fileVersion), &err);
        if (fileVersion != 3) {
            TEE_ERROR("Loading font '%s' failed: Invalid file version", filepath);
            return nullptr;
        }

        //
        fntBlock_t block;
        fntInfo_t info;
        char fontName[256];
        ms.read(&block, sizeof(block), &err);
        ms.read(&info, sizeof(info), &err);
        ms.read(fontName, block.size - sizeof(info), &err);
        
        // Common
        fntCommon_t common;
        ms.read(&block, sizeof(block), &err);
        ms.read(&common, sizeof(common), &err);

        if (common.pages != 1) {
            TEE_ERROR("Loading font '%s' failed: Invalid number of pages", filepath);
            return nullptr;
        }

        // Font pages (textures)
        fntPage_t page;
        ms.read(&block, sizeof(block), &err);
        ms.read(page.path, block.size, &err);
        bx::Path texFilepath = bx::Path(filepath).getDirectory();
        texFilepath.joinUnix(page.path);

        // Characters
        ms.read(&block, sizeof(block), &err);
        int numGlyphs = block.size / sizeof(fntChar_t);
        fntChar_t* chars = (fntChar_t*)alloca(sizeof(fntChar_t)*numGlyphs);
        ms.read(chars, block.size, &err);

        // Kernings
        int last_r = ms.read(&block, sizeof(block), &err);
        int numKerns = block.size / sizeof(fntKernPair_t);
        fntKernPair_t* kerns = (fntKernPair_t*)alloca(sizeof(fntKernPair_t)*(numKerns + 1));
        if (numKerns > 0 && last_r > 0)
            ms.read(kerns, block.size, &err);

        // Create font
        size_t totalSz = sizeof(Font) + 
            numGlyphs*sizeof(FontGlyph) + 
            numKerns*sizeof(FontKerning) + 
            bx::HashTable<int, uint16_t>::GetImmutableSizeBytes(numGlyphs);
        uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSz);
        Font* font = new(buff) Font;
        if (!font)
            return nullptr;
        buff += sizeof(Font);
        font->glyphs = (FontGlyph*)buff;
        buff += numGlyphs * sizeof(FontGlyph);
        font->kerns = (FontKerning*)buff;
        buff += numKerns*sizeof(FontKerning);

        // Hashtable for characters
        if (!font->glyphTable.createWithBuffer(numGlyphs, buff)) {
            BX_ASSERT(false);
            return nullptr;
        }

        bx::strCopy(font->name, sizeof(font->name), fontName);
        font->size = info.font_size;
        font->lineHeight = common.line_height;
        font->base = common.base;
        font->scaleW = common.scale_w;
        font->scaleH = common.scale_h;
        font->flags |= params.flags;

        LoadTextureParams tparams;
        tparams.generateMips = params.generateMips;
        font->texHandles[0] = asset::load("texture", texFilepath.cstr(), &tparams, 0,
                                           alloc == gFontMgr->alloc ? nullptr : alloc);
        font->numPages = 1;
        bx::memSet(font->glyphs, 0x00, sizeof(FontGlyph)*numGlyphs);

        uint16_t charWidth = 0;
        for (int i = 0; i < numGlyphs; i++) {
            const fntChar_t& ch = chars[i];
            BX_ASSERT(ch.id < UINT16_MAX);
            font->glyphs[i].charId = uint16_t(ch.id);
            font->glyphs[i].width = (float)ch.width;
            font->glyphs[i].height = (float)ch.height;
            font->glyphs[i].x = (float)ch.x;
            font->glyphs[i].y = (float)ch.y;
            font->glyphs[i].xadvance = (float)ch.xadvance;
            font->glyphs[i].xoffset = (float)ch.xoffset;
            font->glyphs[i].yoffset = (float)ch.yoffset;

            font->glyphTable.add(uint16_t(ch.id), i);

            charWidth = bx::max<uint16_t>(charWidth, ch.xadvance);
        }
        font->numGlyphs = numGlyphs;
        font->charWidth = charWidth;

        if (numKerns > 0 && last_r > 0) {
            bx::memSet(font->kerns, 0x00, sizeof(FontKerning)*numKerns);

            for (int i = 0; i < numKerns; i++) {
                const fntKernPair_t& kern = kerns[i];
                // Find char and set it's kerning index
                uint32_t id = kern.first;
                for (int k = 0; k < numGlyphs; k++) {
                    if (id == font->glyphs[k].charId) {
                        if (font->glyphs[k].numKerns == 0)
                            font->glyphs[k].kernIdx = i;
                        font->glyphs[k].numKerns++;
                        break;
                    }
                }

                font->kerns[i].secondCharId = (uint16_t)kern.second;
                font->kerns[i].amount = (float)kern.amount;
            }
        }
        font->numKerns = numKerns;

        return font;
    }

    bool FontLoader::loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
    {
        const LoadFontParams* fparams = (const LoadFontParams*)params.userParams;
        Font* font = nullptr;
        if (fparams->format == FontFileFormat::Text) {
            font = loadFontText(mem, params.uri, *fparams, alloc ? alloc : gFontMgr->alloc);
        } else if (fparams->format == FontFileFormat::Binary) {
            font = loadFontBinary(mem, params.uri, *fparams, alloc ? alloc : gFontMgr->alloc);
        }
        *obj = uintptr_t(font);
        return font != nullptr ? true : false;
    }

    void FontLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
    {
        Font* font = (Font*)obj;
        
        for (int i = 0; i < font->numPages; i++) {
            if (font->texHandles[i].isValid()) {
                asset::unload(font->texHandles[i]);
                font->texHandles[i].reset();
            }
        }
        font->glyphTable.destroy();

        BX_FREE(alloc ? alloc : gFontMgr->alloc, font);
    }

    void FontLoader::onReload(AssetHandle handle, bx::AllocatorI* alloc)
    {
    }

    AssetHandle gfx::getFontTexture(Font* font, int pageId /*= 0*/)
    {
        BX_ASSERT(pageId < MAX_FONT_PAGES);
        return font->texHandles[pageId];
    }

    vec2_t gfx::getFontTextureSize(Font* font)
    {
        return vec2(float(font->scaleW), float(font->scaleH));
    }

    float gfx::getFontLineHeight(Font* font)
    {
        return font->lineHeight;
    }

    float gfx::getFontTextWidth(Font* font, const char* text, int len, float* firstcharWidth)
    {
        if (len <= 0)
            len = (int)strlen(text);

        float width = 0;
        int glyphIdx = findFontCharGlyph(font, text[0]);
        if (glyphIdx != -1) {
            width = font->glyphs[glyphIdx].xadvance;
            if (firstcharWidth)
                *firstcharWidth = width;
        }

        for (int i = 1; i < len; i++) {
            glyphIdx = findFontCharGlyph(font, text[i]);
            if (glyphIdx != -1) {
                width += font->glyphs[glyphIdx].xadvance;

                if (i + 1 < len) {
                    int nextIdx = findFontCharGlyph(font, text[i + 1]);
                    if (nextIdx != -1)
                        width += getFontGlyphKerning(font, glyphIdx, nextIdx);
                }
            }
        }

        return width;
    }

    float gfx::getFontGlyphKerning(Font* font, int glyphIdx, int nextGlyphIdx)
    {
        const FontGlyph& ch = font->glyphs[glyphIdx];
        uint16_t nextCharId = font->glyphs[nextGlyphIdx].charId;

        for (int i = ch.kernIdx, end = ch.kernIdx + ch.numKerns; i < end; i++) {
            if (font->kerns[i].secondCharId == nextCharId)
                return font->kerns[i].amount;
        }
        return 0;
    }

    bool gfx::fontIsUnicode(Font* font)
    {
        return (font->flags & FontFlags::Persian) ? true : false;
    }

    int gfx::findFontCharGlyph(Font* font, uint16_t chId)
    {
        int index = font->glyphTable.find(chId);
        return index != -1 ? font->glyphTable[index] : -1;
    }

    const FontGlyph& gfx::getFontGlyph(Font* font, int index)
    {
        BX_ASSERT(index < font->numGlyphs);
        return font->glyphs[index];
    }

    TextDraw* gfx::createTextDraw(int maxChars, AssetHandle fontHandle, bx::AllocatorI* alloc)
    {
        BX_ASSERT(gFontMgr);
        BX_ASSERT(fontHandle.isValid());

        TextDraw* tbatch = gFontMgr->batchPool.newInstance<bx::AllocatorI*>(alloc);
        tbatch->verts = (TextVertex*)BX_ALLOC(alloc, sizeof(TextVertex)*maxChars*4);
        tbatch->indices = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*maxChars*6);
        if (!tbatch->verts || !tbatch->indices)
            return nullptr;
        tbatch->maxChars = maxChars;
        tbatch->fontHandle = fontHandle;
        return tbatch;
    }

    void gfx::beginText(TextDraw* batch, const mat4_t& viewProjMtx, const vec2_t screenSize)
    {
        bx::HashMurmur2A hasher;
        hasher.begin();
        hasher.add(viewProjMtx.f, sizeof(viewProjMtx));
        hasher.add<vec2_t>(screenSize);
        uint32_t mtxHash = hasher.end();

        if (mtxHash != batch->mtxHash) {
            mat4_t inv;
            mat4_t proj;
            bx::mtxOrtho(proj.f, 0, screenSize.x, screenSize.y, 0, -1.0f, 1.0f, 0, false);
            bx::mtxInverse(inv.f, viewProjMtx.f);
            bx::mtxMul(batch->transformMtx.f, proj.f, inv.f);
            batch->screenSize = screenSize;
            batch->viewProjMtx = viewProjMtx;
            batch->mtxHash = mtxHash;
        }

        batch->numChars = 0;
    }

    void gfx::endText(TextDraw* batch)
    {

    }

    inline void inverseGlyphs(FontGlyph* glyphs, int startIdx, int endIdx)
    {
        int sz = endIdx - startIdx;
        if (sz > 1) {
            int midIdx = startIdx + sz/2;
            for (int i = startIdx; i < midIdx; i++) {
                bx::xchg<FontGlyph>(glyphs[i], glyphs[startIdx+endIdx-i-1]);
            }
        }
    }

    // Resolves glyphs from text (UTF8/ASCII)
    // Returns number of glyphs resolved
    static int resolveGlyphs(const char* text, Font* font, FontGlyph* glyphs, int maxChars, float* kerns = nullptr)
    {
        auto getUnicodeReplaceRule = [](uint32_t code, const UnicodeReplaceRule* glyphs, int numGlyphs)->int
        {
            for (int i = 0; i < numGlyphs; i++) {
                if (glyphs[i].code == code)
                    return i;
            }
            return -1;
        };

        if (!(font->flags & (FontFlags::Persian | FontFlags::Unicode))) {
            // Normal font (ascii)
            int len = bx::min<int>(maxChars, (int)strlen(text));
            for (int i = 0; i < len; i++) {
                int gIdx = font->glyphTable.find(text[i]);
                if (gIdx != -1) {
                    memcpy(&glyphs[i], &font->glyphs[font->glyphTable[gIdx]], sizeof(FontGlyph));

                    if (kerns) {
                        int nextGlyphIdx = font->glyphTable.find(text[i+1]);
                        if (nextGlyphIdx != -1)
                            kerns[i] = gfx::getFontGlyphKerning(font, font->glyphTable[gIdx], font->glyphTable[nextGlyphIdx]);
                        else
                            kerns[i] = 0;
                    }                    
                } else {
                    gIdx = font->glyphTable.find('?');
                    if (gIdx != -1) {
                        memcpy(&glyphs[i], &font->glyphs[font->glyphTable[gIdx]], sizeof(FontGlyph));
                    } else {
                        return i;
                    }
                }
            }
            return len;
        } else if (font->flags & FontFlags::Persian) {
            // Persian (Unicode)
            uint32_t* utext = (uint32_t*)alloca(sizeof(uint32_t)*(maxChars + 1));
            int len = u8_toucs(utext, maxChars+1, text, -1);

            const UnicodeReplaceRule* rules = kPersianGlyphs;
            int ruleCount = BX_COUNTOF(kPersianGlyphs);
            int digitIdx = -1;

            for (int i = 0; i < len; i++) {
                uint32_t code = utext[i];
                bool isdigit = (code >= '0' && code <= '9');

                // Replace character code (glyphId) by the rules we have for each language
                // If current character has "before" glyph, look for the next character and see if it has middle or after glyph
                // If current character has "after" glyph, look for previous character and see if it has middle or before glyph
                // If both of the conditions above are met, we have middle glyph for current character
                int curRuleIdx = getUnicodeReplaceRule(code, rules, ruleCount);
                if (curRuleIdx != -1) {
                    int prevRuleIdx = i > 0 ? getUnicodeReplaceRule(utext[i-1], rules, ruleCount) : -1;
                    int nextRuleIdx = i < len - 1 ? getUnicodeReplaceRule(utext[i+1], rules, ruleCount) : -1;
                    uint8_t ruleBits = 0;
                    const UnicodeReplaceRule& curRule = rules[curRuleIdx];
                    if (curRule.after != 0 && prevRuleIdx != -1 && (rules[prevRuleIdx].before + rules[prevRuleIdx].middle) > 0) {
                        code = curRule.after;
                        ruleBits |= 0x1;
                    }

                    if (curRule.before != 0 && nextRuleIdx != -1 && (rules[nextRuleIdx].after + rules[nextRuleIdx].middle) > 0) {
                        code = curRule.before;
                        ruleBits |= 0x2;
                    }

                    if (ruleBits == 0x3 && curRule.middle != 0)
                        code = curRule.middle;
                }

                int gIdx = font->glyphTable.find(code);
                if (gIdx != -1) {
                    memcpy(&glyphs[i], &font->glyphs[font->glyphTable[gIdx]], sizeof(FontGlyph));
                    if (kerns) {
                        int nextGlyphIdx = font->glyphTable.find(utext[i+1]);
                        if (nextGlyphIdx != -1)
                            kerns[i] = gfx::getFontGlyphKerning(font, font->glyphTable[gIdx], font->glyphTable[nextGlyphIdx]);
                        else
                            kerns[i] = 0;
                    }
                } else {
                    return i;
                }

                if (isdigit && digitIdx == -1) {
                    digitIdx = i;
                } else if (!isdigit && digitIdx != -1) {
                    inverseGlyphs(glyphs, digitIdx, i);
                    digitIdx = -1;
                }
            }

            if (digitIdx != -1)
                inverseGlyphs(glyphs, digitIdx, len);

            return len;
        }

        return 0;
    }

    static void resolveMultiline(Font* font, FontGlyph* glyphs, float* xs, float* ys, int numGlyphs, float maxWidth, 
                                 float scale, TextFlags::Bits flags)
    {
        float w = 0;
        float lineIdx = 0;
        float lineHeight = float(font->lineHeight) * scale;
        int spaceIdx = -1;
        int lastLineCharIdx = 0;
        float wToSpace = 0;
        float spaceCharWidth = 0;

        for (int i = 0; i < numGlyphs; i++) {
            FontGlyph* glyph = &glyphs[i];

            float charWidth = bx::ceil(glyph->xadvance * scale);
            w += charWidth;

            if (glyph->charId == 32) {
                spaceIdx = i;
                wToSpace = w - charWidth;
                spaceCharWidth = charWidth;
            }
            ys[i] = lineIdx * lineHeight;

            // proceed to next line ?
            // If we have reached the end of line, cut the line from the last space
            if (w > (maxWidth - 1.0f) && spaceIdx != -1) {
                int newLineCharIdx = spaceIdx + 1;
                float xoffset = (flags & TextFlags::AlignCenter) ? (maxWidth - wToSpace)*0.5f : 0;
                for (int k = lastLineCharIdx; k < newLineCharIdx; k++)
                    xs[k] = xoffset;
                lineIdx++;
                for (int k = spaceIdx + 1; k <= i; k++)
                    ys[k] = lineIdx * lineHeight;
                spaceIdx = -1;
                lastLineCharIdx = newLineCharIdx;
                w = 0;
            }
        }

        float xoffset = (flags & TextFlags::AlignCenter) ? (maxWidth - w - spaceCharWidth)*0.5f : 0;
        for (int k = lastLineCharIdx; k < numGlyphs; k++)
            xs[k] = xoffset;
    }

    void gfx::addText(TextDraw* batch, float scale, const rect_t& rectFit, TextFlags::Bits flags, const char* text)
    {
        BX_ASSERT(batch);

        Font* font = asset::getObjPtr<Font>(batch->fontHandle);
        if ((flags & (TextFlags::AlignLeft | TextFlags::AlignRight | TextFlags::AlignCenter)) == 0) {
            if (!(flags & TextFlags::Multiline))
                flags |= TextFlags::AlignCenter;
            else
                flags |= !(font->flags & FontFlags::Persian) ? TextFlags::AlignLeft : TextFlags::AlignRight;
        }
        if ((flags & (TextFlags::RightToLeft | TextFlags::LeftToRight)) == 0) {
            flags |= !(font->flags & FontFlags::Persian) ? TextFlags::LeftToRight : TextFlags::RightToLeft;
        }

        vec2_t screenSize = batch->screenSize;
        mat4_t viewProjMtx = batch->viewProjMtx;
        vec2_t refScreenSize = gFontMgr->refScreenSize;
        if (refScreenSize.x > 0) {
            float screenRefFactor = screenSize.x/refScreenSize.x;
            scale *= screenRefFactor;
        }
        
        vec2_t texSize = vec2(font->scaleW, font->scaleH);
        FontGlyph glyphs[512];
        int len = resolveGlyphs(text, font, glyphs, BX_COUNTOF(glyphs));
        if (len == 0)
            return;

        // Crop Characters
        len = bx::min<int>(len, batch->maxChars - batch->numChars);

        // Convert to screen rectangle
        auto projectToScreen = [viewProjMtx, screenSize](float x, float y)->vec2_t
        {
            float wh = screenSize.x*0.5f;
            float hh = screenSize.y*0.5f;

            vec4_t proj;
            bx::vec4MulMtx(proj.f, vec4(x, y, 0, 1.0f).f, viewProjMtx.f);

            float sx = (proj.x/proj.w)*wh + wh + 0.5f;
            float sy = -(proj.y/proj.w)*hh + hh + 0.5f;

            return vec2(sx, sy);
        };

        rect_t screenRect = rect(projectToScreen(rectFit.xmin, rectFit.ymax), 
                                  projectToScreen(rectFit.xmax, rectFit.ymin));

        // Make Quads
        int vertexIdx = 0;
        int indexIdx = 0;
        int maxVerts = len*4;
        float x = 0;

        int firstVertIdx = batch->numChars*4;
        TextVertex* verts = batch->verts + firstVertIdx;
        uint16_t* indices = batch->indices + batch->numChars*6;
        float alpha = !(flags & TextFlags::Dim) ? 1.0f : 0.5f;

        float dir = 1.0f;
        float dirInvFactor = 0;
        if (flags & TextFlags::RightToLeft) {
            dir = -1.0f;
            dirInvFactor = 1.0f;
        }
        float advanceScale = 1.0f;
        if (flags & TextFlags::Narrow)
            advanceScale = 0.9f;

        float width = screenRect.xmax - screenRect.xmin;
        float* ys = (float*)alloca(sizeof(float)*len);
        float* xs = (float*)alloca(sizeof(float)*len);
        bx::memSet(ys, 0x00, sizeof(float)*len);
        bx::memSet(xs, 0x00, sizeof(float)*len);
        if (flags & TextFlags::Multiline) {
            resolveMultiline(font, glyphs, xs, ys, len, width, scale, flags);
        }
       
        float xMax = 0;
        for (int i = 0; i < len; i++) {
            const FontGlyph& glyph = glyphs[i];
            if (i > 0 && ys[i] > ys[i-1]) {
                // Move to new line
                xMax = x < 0 ? bx::min(x, xMax) : bx::max(x, xMax);
                x = 0;
            }
                
            TextVertex& v0 = verts[vertexIdx];
            TextVertex& v1 = verts[vertexIdx + 1];
            TextVertex& v2 = verts[vertexIdx + 2];
            TextVertex& v3 = verts[vertexIdx + 3];

            float gw = glyph.width*scale;
            float gh = glyph.height*scale;
            float xoffset = glyph.xoffset*scale + xs[i]*dir;
            float yoffset = glyph.yoffset*scale;
            float gx = glyph.x;
            float gy = glyph.y;

            x -= glyph.xadvance*scale*dirInvFactor*advanceScale;

            // Top-Left
            v0.x = x + xoffset;
            v0.y = yoffset + ys[i];
            v0.tx = gx / texSize.x;
            v0.ty = gy / texSize.y;

            // Top-Right
            v1.x = x + xoffset + gw;
            v1.y = yoffset + ys[i];
            v1.tx = (gx + glyph.width) / texSize.x;
            v1.ty = gy / texSize.y;

            // Bottom-Left
            v2.x = x + xoffset;
            v2.y = yoffset + gh + ys[i];
            v2.tx = gx / texSize.x;
            v2.ty = (gy + glyph.height) / texSize.y;

            // Bottom-Right
            v3.x = x + xoffset + gw;
            v3.y = yoffset + gh + ys[i];
            v3.tx = (gx + glyph.width) / texSize.x;
            v3.ty = (gy + glyph.height) / texSize.y;

            v0.alpha = v1.alpha = v2.alpha = v3.alpha = alpha;

            x += glyph.xadvance*scale*(1.0f - dirInvFactor)*advanceScale;

#if 0
            // Kerning
            int nextGlyphIdx = font->glyphTable.find(text[i+1]);
            if (nextGlyphIdx != -1)
                x += getFontGlyphKerning(font, font->glyphTable[gIdx], font->glyphTable[nextGlyphIdx])*scale;
#endif

            // Make Glyph Quad
            int startVtx = firstVertIdx + vertexIdx;
            indices[indexIdx] = startVtx;
            indices[indexIdx + 1] = startVtx + 1;
            indices[indexIdx + 2] = startVtx + 2;
            indices[indexIdx + 3] = startVtx + 2;
            indices[indexIdx + 4] = startVtx + 1;
            indices[indexIdx + 5] = startVtx + 3;

            vertexIdx += 4;
            indexIdx += 6;
        }

        if (xMax == 0)
            xMax = x;

        batch->numChars += len;

        // We have the final width, Calculate alignment
        vec2_t pos = vec2(0, -font->lineHeight*scale*0.5f);

        if (!(flags & TextFlags::Multiline)) {
            if (flags & TextFlags::AlignCenter) {
                pos = pos + ((screenRect.vmax + screenRect.vmin)*0.5f - vec2(xMax*0.5f, 0));
            } else if (flags & TextFlags::AlignLeft) {
                pos = pos + vec2(screenRect.xmin - xMax*dirInvFactor, (screenRect.ymax + screenRect.ymin)*0.5f);
            } else if (flags & TextFlags::AlignRight) {
                pos = pos + vec2(screenRect.xmax - xMax*(1.0f - dirInvFactor), (screenRect.ymax + screenRect.ymin)*0.5f);
            }
        } else {
            // In multiline, don't vertically align to center
            if (flags & TextFlags::AlignCenter) {
                float x = (flags & TextFlags::RightToLeft) ? screenRect.xmax - xMax*(1.0f - dirInvFactor) :
                    screenRect.xmin - xMax*dirInvFactor;
                pos = pos + vec2(x, screenRect.ymin);
            } else if (flags & TextFlags::AlignLeft) {
                pos = pos + vec2(screenRect.xmin - xMax*dirInvFactor, screenRect.ymin);
            } else if (flags & TextFlags::AlignRight) {
                pos = pos + vec2(screenRect.xmax - xMax*(1.0f - dirInvFactor), screenRect.ymin);
            }
        }

        // Transform vertices by pos
        for (int i = 0; i < maxVerts; i++) {
            verts[i].x += pos.x;
            verts[i].y += pos.y;
        }
    }

    void gfx::addTextf(TextDraw* batch, float scale, const rect_t& rectFit, TextFlags::Bits flags, const char* fmt, ...)
    {
        char text[256];
        va_list args;
        va_start(args, fmt);
        bx::vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        addText(batch, scale, rectFit, flags, text);
    }

    void gfx::resetText(TextDraw* batch)
    {
        batch->numChars = 0;
    }

    static void drawTextBatch(GfxDriver* gDriver, TextDraw* batch, uint8_t viewId, ProgramHandle prog, Font* font,
                              const vec2_t offsetPos, const vec4_t& color)
    {
        int reqVertices = batch->numChars * 4;
        int reqIndices = batch->numChars * 6;
        if (reqVertices == gDriver->getAvailTransientVertexBuffer(reqVertices, TextVertex::Decl) &&
            reqIndices == gDriver->getAvailTransientIndexBuffer(reqIndices))
        {
            TransientVertexBuffer tvb;
            TransientIndexBuffer tib;
            gDriver->allocTransientVertexBuffer(&tvb, reqVertices, TextVertex::Decl);
            gDriver->allocTransientIndexBuffer(&tib, reqIndices);
            memcpy(tvb.data, batch->verts, reqVertices*sizeof(TextVertex));
            memcpy(tib.data, batch->indices, reqIndices*sizeof(uint16_t));

            mat4_t transformMat = batch->transformMtx;
            transformMat.m41 += offsetPos.x;
            transformMat.m42 += offsetPos.y;
            gDriver->setUniform(gFontMgr->uTransformMtx, transformMat.f, 1);
            gDriver->setUniform(gFontMgr->uColor, color.f, 1);
            gDriver->setState(gfx::stateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite, 0);
            gDriver->setTexture(0, gFontMgr->uTexture, asset::getObjPtr<Texture>(font->texHandles[0])->handle,
                                TextureFlag::FromTexture);
            gDriver->setTransientVertexBuffer(0, &tvb);
            gDriver->setTransientIndexBuffer(&tib);
            gDriver->submit(viewId, prog, 0, false);
        }
    }

    static void drawText(TextDraw* batch, uint8_t viewId, const vec2_t& offset, const vec4_t& color)
    {
        if (batch->numChars > 0) {
            GfxDriver* gDriver = getGfxDriver();
            Font* font = asset::getObjPtr<Font>(batch->fontHandle);

            gDriver->setUniform(gFontMgr->uParams, vec4(0, 0, 0, 0).f, 1);
            ProgramHandle prog = (font->flags & FontFlags::DistantField) ? gFontMgr->dfProg : gFontMgr->normalProg;
            drawTextBatch(gDriver, batch, viewId, prog, font, offset, color);
        }
    }

    void gfx::drawText(TextDraw* batch, uint8_t viewId, ucolor_t color)
    {
        if (batch->numChars > 0) {
            GfxDriver* gDriver = getGfxDriver();
            Font* font = asset::getObjPtr<Font>(batch->fontHandle);

            ProgramHandle prog = (font->flags & FontFlags::DistantField) ? gFontMgr->dfProg : gFontMgr->normalProg;
            drawTextBatch(gDriver, batch, viewId, prog, font, vec2(0, 0), tmath::ucolorToVec4(color));
        }
    }

    void gfx::drawTextDropShadow(TextDraw* batch, uint8_t viewId, ucolor_t color, ucolor_t shadowColor, vec2_t shadowAmount)
    {
        if (batch->numChars > 0) {
            GfxDriver* gDriver = getGfxDriver();
            Font* font = asset::getObjPtr<Font>(batch->fontHandle);

            if (!(font->flags & FontFlags::Persian)) {
                vec2_t shadowOffset = vec2(shadowAmount.x/font->scaleW, shadowAmount.y/font->scaleH);
                gDriver->setUniform(gFontMgr->uParams, vec4(0, 0, shadowOffset.x, shadowOffset.y).f, 1);
                gDriver->setUniform(gFontMgr->uShadowColor, tmath::ucolorToVec4(shadowColor).f, 1);
                ProgramHandle prog = (font->flags & FontFlags::DistantField) ? gFontMgr->dfShadowProg : gFontMgr->normalShadowProg;
                drawTextBatch(gDriver, batch, viewId, prog, font, vec2(0, 0), tmath::ucolorToVec4(color));
            } else {
                vec2_t shadowOffset = vec2(2.0f*shadowAmount.x/batch->screenSize.x, -2.0f*shadowAmount.y/batch->screenSize.y);
                ProgramHandle prog = (font->flags & FontFlags::DistantField) ? gFontMgr->dfProg : gFontMgr->normalProg;
                drawTextBatch(gDriver, batch, viewId, prog, font, shadowOffset, tmath::ucolorToVec4(shadowColor));
                drawTextBatch(gDriver, batch, viewId, prog, font, vec2(0, 0), tmath::ucolorToVec4(color));
            }
        }
    }

    void gfx::drawTextOutline(TextDraw* batch, uint8_t viewId, ucolor_t color, ucolor_t outlineColor, float outlineAmount)
    {
        if (batch->numChars > 0) {
            GfxDriver* gDriver = getGfxDriver();
            Font* font = asset::getObjPtr<Font>(batch->fontHandle);

            float outline = 0.5f*outlineAmount;
            gDriver->setUniform(gFontMgr->uParams, vec4(0, 0, outline, 0).f, 1);
            gDriver->setUniform(gFontMgr->uOutlineColor, tmath::ucolorToVec4(outlineColor).f, 1);
            gDriver->setUniform(gFontMgr->uTextureSize, vec4(float(font->scaleW), float(font->scaleH), 0, 0).f, 1);
            ProgramHandle prog = (font->flags & FontFlags::DistantField) ? gFontMgr->dfOutlineProg : gFontMgr->normalOutlineProg;
            drawTextBatch(gDriver, batch, viewId, prog, font, vec2(0, 0), vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    void gfx::destroyTextDraw(TextDraw* batch)
    {
        BX_ASSERT(gFontMgr);
        BX_ASSERT(batch->alloc);

        if (batch->verts)
            BX_FREE(batch->alloc, batch->verts);
        if (batch->indices)
            BX_FREE(batch->alloc, batch->indices);

        gFontMgr->batchPool.deleteInstance(batch);
    }
}
