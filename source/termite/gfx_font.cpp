#include "pch.h"
#include "io_driver.h"
#include "gfx_font.h"
#include "gfx_texture.h"
#include "error_report.h"
#include "gfx_driver.h"
#include "vec_math.h"

#include "bx/hash.h"
#include "bx/uint32_t.h"
#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/hash_table.h"
#include "bxx/pool.h"
#include "bx/string.h"

#include "utf8/utf8.h"

#include T_MAKE_SHADER_PATH(shaders_h, font_normal.vso)
#include T_MAKE_SHADER_PATH(shaders_h, font_normal.fso)
#include T_MAKE_SHADER_PATH(shaders_h, font_normal_shadow.vso)
#include T_MAKE_SHADER_PATH(shaders_h, font_normal_shadow.fso)
#include T_MAKE_SHADER_PATH(shaders_h, font_normal_outline.vso)
#include T_MAKE_SHADER_PATH(shaders_h, font_normal_outline.fso)

#include T_MAKE_SHADER_PATH(shaders_h, font_df.vso)
#include T_MAKE_SHADER_PATH(shaders_h, font_df.fso)
#include T_MAKE_SHADER_PATH(shaders_h, font_df_shadow.vso)
#include T_MAKE_SHADER_PATH(shaders_h, font_df_shadow.fso)
#include T_MAKE_SHADER_PATH(shaders_h, font_df_outline.vso)
#include T_MAKE_SHADER_PATH(shaders_h, font_df_outline.fso)

#define MAX_FONT_PAGES 1

// FNT file format (Binary)
#define FNT_SIGN "BMF"

namespace termite
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

    class FontLoader : public ResourceCallbacksI
    {
    public:
        bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
        void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
        void onReload(ResourceHandle handle, bx::AllocatorI* alloc) override;
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
        ResourceHandle texHandles[MAX_FONT_PAGES];
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
        float tx;
        float ty;

        static void init()
        {
            vdeclBegin(&Decl);
            vdeclAdd(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);
            vdeclAdd(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
            vdeclEnd(&Decl);
        }

        static VertexDecl Decl;
    };
    VertexDecl TextVertex::Decl;


    struct TextBatch
    {
        bx::AllocatorI* alloc;
        ResourceHandle fontHandle;
        int maxChars;
        int numChars;
        TextVertex* verts;
        uint16_t* indices;
        uint32_t mtxHash;
        vec2_t screenSize;
        mtx4x4_t viewProjMtx;
        mtx4x4_t transformMtx;

        TextBatch(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            maxChars(0),
            numChars(0),
            verts(nullptr),
            indices(nullptr),
            mtxHash(0)
        {
        }
    };

    struct FontSystem
    {
        bx::AllocatorI* alloc;
        FontLoader loader;
        Font* failFont;
        Font* asyncFont;
        bx::Pool<TextBatch> batchPool;
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

        FontSystem(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            failFont(nullptr),
            asyncFont(nullptr)
        {
        }
    };

    static FontSystem* g_fontSys = nullptr;    

    static Font* createDummyFont()
    {
        Font* font = BX_NEW(g_fontSys->alloc, Font);
        font->lineHeight = 20;
        font->scaleW = 1;
        font->scaleH = 1;
        font->numPages = 1;
        font->texHandles[0] = getResourceAsyncHandle("texture");
        font->numGlyphs = 0;
        font->numKerns = 0;
        return font;
    }

    result_t initFontSystem(bx::AllocatorI* alloc, const vec2_t refScreenSize)
    {
        assert(alloc);

        if (g_fontSys) {
            assert(false);
            return T_ERR_FAILED;
        }

        g_fontSys = BX_NEW(alloc, FontSystem)(alloc);
        if (!g_fontSys)
            return T_ERR_OUTOFMEM;

        if (!g_fontSys->batchPool.create(16, alloc))
            return T_ERR_OUTOFMEM;            

        TextVertex::init();

        g_fontSys->failFont = createDummyFont();
        g_fontSys->asyncFont = createDummyFont();

        if (!initFontSystemGraphics()) {
            return T_ERR_FAILED;
        }

        g_fontSys->refScreenSize = refScreenSize;

        return 0;
    }

    void shutdownFontSystem()
    {
        if (!g_fontSys)
            return;

        bx::AllocatorI* alloc = g_fontSys->alloc;

        shutdownFontSystemGraphics();

        if (g_fontSys->failFont)
            g_fontSys->loader.unloadObj(uintptr_t(g_fontSys->failFont), g_fontSys->alloc);
        if (g_fontSys->asyncFont)
            g_fontSys->loader.unloadObj(uintptr_t(g_fontSys->asyncFont), g_fontSys->alloc);

        g_fontSys->batchPool.destroy();
        BX_DELETE(alloc, g_fontSys);
        g_fontSys = nullptr;
    }

    bool initFontSystemGraphics()
    {
        GfxDriverApi* gDriver = getGfxDriver();

        g_fontSys->normalProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_normal_vso, sizeof(font_normal_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_normal_fso, sizeof(font_normal_fso), nullptr, nullptr)), true);
        if (!g_fontSys->normalProg.isValid()) {
            T_ERROR("Creating font shader failed");
            return false;
        }
        g_fontSys->normalShadowProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_normal_shadow_vso, sizeof(font_normal_shadow_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_normal_shadow_fso, sizeof(font_normal_shadow_fso), nullptr, nullptr)), true);
        if (!g_fontSys->normalShadowProg.isValid()) {
            T_ERROR("Creating font shader failed");
            return false;
        }

        g_fontSys->normalOutlineProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_normal_outline_vso, sizeof(font_normal_outline_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_normal_outline_fso, sizeof(font_normal_outline_fso), nullptr, nullptr)), true);
        if (!g_fontSys->normalOutlineProg.isValid()) {
            T_ERROR("Creating font shader failed");
            return false;
        }

        g_fontSys->dfProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_df_vso, sizeof(font_df_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_df_fso, sizeof(font_df_fso), nullptr, nullptr)), true);
        if (!g_fontSys->dfProg.isValid()) {
            T_ERROR("Creating font shader failed");
            return false;
        }

        g_fontSys->dfShadowProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_df_shadow_vso, sizeof(font_df_shadow_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_df_shadow_fso, sizeof(font_df_shadow_fso), nullptr, nullptr)), true);
        if (!g_fontSys->dfShadowProg.isValid()) {
            T_ERROR("Creating font shader failed");
            return false;
        }

        g_fontSys->dfOutlineProg = gDriver->createProgram(
            gDriver->createShader(gDriver->makeRef(font_df_outline_vso, sizeof(font_df_outline_vso), nullptr, nullptr)),
            gDriver->createShader(gDriver->makeRef(font_df_outline_fso, sizeof(font_df_outline_fso), nullptr, nullptr)), true);
        if (!g_fontSys->dfOutlineProg.isValid()) {
            T_ERROR("Creating font shader failed");
            return false;
        }

        g_fontSys->uTexture = gDriver->createUniform("u_texture", UniformType::Int1, 1);
        g_fontSys->uParams = gDriver->createUniform("u_params", UniformType::Vec4, 1);
        g_fontSys->uTextureSize = gDriver->createUniform("u_textureSize", UniformType::Vec4, 1);
        g_fontSys->uShadowColor = gDriver->createUniform("u_shadowColor", UniformType::Vec4, 1);
        g_fontSys->uOutlineColor = gDriver->createUniform("u_outlineColor", UniformType::Vec4, 1);
        g_fontSys->uTransformMtx = gDriver->createUniform("u_transformMtx", UniformType::Mat4, 1);
        g_fontSys->uColor = gDriver->createUniform("u_color", UniformType::Vec4, 1);
        return true;

    }

    void shutdownFontSystemGraphics()
    {

        GfxDriverApi* gDriver = getGfxDriver();
        if (g_fontSys->normalProg.isValid())
            gDriver->destroyProgram(g_fontSys->normalProg);
        if (g_fontSys->normalShadowProg.isValid())
            gDriver->destroyProgram(g_fontSys->normalShadowProg);
        if (g_fontSys->normalOutlineProg.isValid())
            gDriver->destroyProgram(g_fontSys->normalOutlineProg);
        if (g_fontSys->dfProg.isValid())
            gDriver->destroyProgram(g_fontSys->dfProg);
        if (g_fontSys->dfShadowProg.isValid())
            gDriver->destroyProgram(g_fontSys->dfShadowProg);
        if (g_fontSys->dfOutlineProg.isValid())
            gDriver->destroyProgram(g_fontSys->dfOutlineProg);

        if (g_fontSys->uTexture.isValid())
            gDriver->destroyUniform(g_fontSys->uTexture);
        if (g_fontSys->uParams.isValid())
            gDriver->destroyUniform(g_fontSys->uParams);
        if (g_fontSys->uColor.isValid())
            gDriver->destroyUniform(g_fontSys->uColor);
        if (g_fontSys->uShadowColor.isValid())
            gDriver->destroyUniform(g_fontSys->uShadowColor);
        if (g_fontSys->uOutlineColor.isValid())
            gDriver->destroyUniform(g_fontSys->uOutlineColor);
        if (g_fontSys->uTransformMtx.isValid())
            gDriver->destroyUniform(g_fontSys->uTransformMtx);
        if (g_fontSys->uTextureSize.isValid())
            gDriver->destroyUniform(g_fontSys->uTextureSize);
    }

    void registerFontToResourceLib()
    {
        ResourceTypeHandle handle;
        handle = registerResourceType("font", &g_fontSys->loader, sizeof(LoadFontParams),
                                      uintptr_t(g_fontSys->failFont), uintptr_t(g_fontSys->asyncFont));
        assert(handle.isValid());
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
                bx::strCopy(key, std::min<size_t>(maxChars, size_t(equal-token)+1), token);
                if (*(equal + 1) != '"')
                    bx::strCopy(value, maxChars, equal + 1);
                else
                    bx::strCopy(value, maxChars, equal + 2);
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
                bx::strCopy(snum, std::min<size_t>(sizeof(snum), size_t(token-value)+1), value);
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
                    assert(charId < UINT16_MAX);
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
                    charWidth = std::max<uint16_t>(charWidth, (uint16_t)xadvance);
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

            assert(firstGlyphIdx != -1);
            if (glyphs[firstGlyphIdx].numKerns == 0)
                glyphs[firstGlyphIdx].kernIdx = kernIdx;
            ++glyphs[firstGlyphIdx].numKerns;
        };
        
        char line[1024];
        char* eol = strchr(strbuff, '\n');
        int kernIdx = 0;
        int charIdx = 0;
        while (eol) {
            bx::strCopy(line, std::min<size_t>(sizeof(line), size_t(eol - strbuff)+1), strbuff);
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
                    assert(numGlyphs > 0);
                    glyphs = (FontGlyph*)BX_ALLOC(tmpAlloc, sizeof(FontGlyph)*numGlyphs);
                    assert(glyphs);
                    bx::memSet(glyphs, 0x00, sizeof(FontGlyph)*numGlyphs);
                } else if (strcmp(token, "char") == 0) {
                    assert(charIdx < numGlyphs);
                    readChar(glyphs[charIdx++]);
                } else if (strcmp(token, "kernings") == 0) {
                    readKernings();
                    if (numKernings > 0) {
                        kernings = (FontKerning*)BX_ALLOC(tmpAlloc, sizeof(FontKerning)*numKernings);
                        assert(kernings);
                        bx::memSet(kernings, 0x00, sizeof(FontKerning)*numKernings);
                    }
                } else if (strcmp(token, "kerning") == 0) {
                    assert(kernIdx < numKernings);
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
            assert(false);
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
            tparams.generateMips = params.generateMips;
            font->texHandles[0] = loadResource("texture", texFilepath.cstr(), &tparams, 0,
                                               alloc == g_fontSys->alloc ? nullptr : alloc);
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
            T_ERROR("Loading font '%s' failed: Invalid FNT (bmfont) binary file", filepath);
            return nullptr;
        }

        // File version
        uint8_t fileVersion;
        ms.read(&fileVersion, sizeof(fileVersion), &err);
        if (fileVersion != 3) {
            T_ERROR("Loading font '%s' failed: Invalid file version", filepath);
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
            T_ERROR("Loading font '%s' failed: Invalid number of pages", filepath);
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
            assert(false);
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
        font->texHandles[0] = loadResource("texture", texFilepath.cstr(), &tparams, 0,
                                           alloc == g_fontSys->alloc ? nullptr : alloc);
        font->numPages = 1;
        bx::memSet(font->glyphs, 0x00, sizeof(FontGlyph)*numGlyphs);

        uint16_t charWidth = 0;
        for (int i = 0; i < numGlyphs; i++) {
            const fntChar_t& ch = chars[i];
            assert(ch.id < UINT16_MAX);
            font->glyphs[i].charId = uint16_t(ch.id);
            font->glyphs[i].width = (float)ch.width;
            font->glyphs[i].height = (float)ch.height;
            font->glyphs[i].x = (float)ch.x;
            font->glyphs[i].y = (float)ch.y;
            font->glyphs[i].xadvance = (float)ch.xadvance;
            font->glyphs[i].xoffset = (float)ch.xoffset;
            font->glyphs[i].yoffset = (float)ch.yoffset;

            font->glyphTable.add(uint16_t(ch.id), i);

            charWidth = std::max<uint16_t>(charWidth, ch.xadvance);
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

    bool FontLoader::loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
    {
        const LoadFontParams* fparams = (const LoadFontParams*)params.userParams;
        Font* font = nullptr;
        if (fparams->format == FontFileFormat::Text) {
            font = loadFontText(mem, params.uri, *fparams, alloc ? alloc : g_fontSys->alloc);
        } else if (fparams->format == FontFileFormat::Binary) {
            font = loadFontBinary(mem, params.uri, *fparams, alloc ? alloc : g_fontSys->alloc);
        }
        *obj = uintptr_t(font);
        return font != nullptr ? true : false;
    }

    void FontLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
    {
        Font* font = (Font*)obj;
        
        for (int i = 0; i < font->numPages; i++) {
            if (font->texHandles[i].isValid()) {
                unloadResource(font->texHandles[i]);
                font->texHandles[i].reset();
            }
        }
        font->glyphTable.destroy();

        BX_FREE(alloc ? alloc : g_fontSys->alloc, font);
    }

    void FontLoader::onReload(ResourceHandle handle, bx::AllocatorI* alloc)
    {
    }

    ResourceHandle getFontTexture(Font* font, int pageId /*= 0*/)
    {
        assert(pageId < MAX_FONT_PAGES);
        return font->texHandles[pageId];
    }

    vec2_t getFontTextureSize(Font* font)
    {
        return vec2f(float(font->scaleW), float(font->scaleH));
    }

    float getFontLineHeight(Font* font)
    {
        return font->lineHeight;
    }

    float getFontTextWidth(Font* font, const char* text, int len, float* firstcharWidth)
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

    float getFontGlyphKerning(Font* font, int glyphIdx, int nextGlyphIdx)
    {
        const FontGlyph& ch = font->glyphs[glyphIdx];
        uint16_t nextCharId = font->glyphs[nextGlyphIdx].charId;

        for (int i = ch.kernIdx, end = ch.kernIdx + ch.numKerns; i < end; i++) {
            if (font->kerns[i].secondCharId == nextCharId)
                return font->kerns[i].amount;
        }
        return 0;
    }

    int findFontCharGlyph(Font* font, uint16_t chId)
    {
        int index = font->glyphTable.find(chId);
        return index != -1 ? font->glyphTable[index] : -1;
    }

    const FontGlyph& getFontGlyph(Font* font, int index)
    {
        assert(index < font->numGlyphs);
        return font->glyphs[index];
    }

    TextBatch* createTextBatch(int maxChars, ResourceHandle fontHandle, bx::AllocatorI* alloc)
    {
        assert(g_fontSys);
        assert(fontHandle.isValid());

        TextBatch* tbatch = g_fontSys->batchPool.newInstance<bx::AllocatorI*>(alloc);
        tbatch->verts = (TextVertex*)BX_ALLOC(alloc, sizeof(TextVertex)*maxChars*4);
        tbatch->indices = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*maxChars*6);
        if (!tbatch->verts || !tbatch->indices)
            return nullptr;
        tbatch->maxChars = maxChars;
        tbatch->fontHandle = fontHandle;
        return tbatch;
    }

    void beginText(TextBatch* batch, const mtx4x4_t& viewProjMtx, const vec2_t screenSize)
    {
        bx::HashMurmur2A hasher;
        hasher.begin();
        hasher.add(viewProjMtx.f, sizeof(viewProjMtx));
        hasher.add<vec2_t>(screenSize);
        uint32_t mtxHash = hasher.end();

        if (mtxHash != batch->mtxHash) {
            mtx4x4_t inv;
            mtx4x4_t proj;
            bx::mtxOrtho(proj.f, 0, screenSize.x, screenSize.y, 0, -1.0f, 1.0f, 0, false);
            bx::mtxInverse(inv.f, viewProjMtx.f);
            bx::mtxMul(batch->transformMtx.f, proj.f, inv.f);
            batch->screenSize = screenSize;
            batch->viewProjMtx = viewProjMtx;
            batch->mtxHash = mtxHash;
        }

        batch->numChars = 0;
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
            int len = std::min<int>(maxChars, (int)strlen(text));
            for (int i = 0; i < len; i++) {
                int gIdx = font->glyphTable.find(text[i]);
                if (gIdx != -1) {
                    memcpy(&glyphs[i], &font->glyphs[font->glyphTable[gIdx]], sizeof(FontGlyph));

                    if (kerns) {
                        int nextGlyphIdx = font->glyphTable.find(text[i+1]);
                        if (nextGlyphIdx != -1)
                            kerns[i] = getFontGlyphKerning(font, font->glyphTable[gIdx], font->glyphTable[nextGlyphIdx]);
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
                            kerns[i] = getFontGlyphKerning(font, font->glyphTable[gIdx], font->glyphTable[nextGlyphIdx]);
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

    void addText(TextBatch* batch, float scale, const rect_t& rectFit, TextFlags::Bits flags, const char* text)
    {
        assert(batch);

        Font* font = getResourcePtr<Font>(batch->fontHandle);
        if ((flags & (TextFlags::AlignLeft | TextFlags::AlignRight | TextFlags::AlignCenter)) == 0) {
            flags |= TextFlags::AlignCenter;
        }
        if ((flags & (TextFlags::RightToLeft | TextFlags::LeftToRight)) == 0) {
            flags |= !(font->flags & FontFlags::Persian) ? TextFlags::LeftToRight : TextFlags::RightToLeft;
        }

        vec2_t screenSize = batch->screenSize;
        mtx4x4_t viewProjMtx = batch->viewProjMtx;
        vec2_t refScreenSize = g_fontSys->refScreenSize;
        if (refScreenSize.x > 0) {
            float screenRefFactor = screenSize.x/refScreenSize.x;
            scale *= screenRefFactor;
        }
        
        vec2_t texSize = vec2f(font->scaleW, font->scaleH);
        FontGlyph glyphs[256];
        int len = resolveGlyphs(text, font, glyphs, BX_COUNTOF(glyphs));
        if (len == 0)
            return;

        // Crop Characters
        len = std::min<int>(len, batch->maxChars - batch->numChars);

        // Convert to screen rectangle
        auto projectToScreen = [viewProjMtx, screenSize](float x, float y)->vec2_t
        {
            float wh = screenSize.x*0.5f;
            float hh = screenSize.y*0.5f;

            vec4_t proj;
            bx::vec4MulMtx(proj.f, vec4f(x, y, 0, 1.0f).f, viewProjMtx.f);

            float sx = (proj.x/proj.w)*wh + wh + 0.5f;
            float sy = -(proj.y/proj.w)*hh + hh + 0.5f;

            return vec2f(sx, sy);
        };

        rect_t screenRect = rectv(projectToScreen(rectFit.xmin, rectFit.ymax), 
                                  projectToScreen(rectFit.xmax, rectFit.ymin));

        // Make Quads
        int vertexIdx = 0;
        int indexIdx = 0;
        int maxVerts = len*4;
        float x = 0;

        int firstVertIdx = batch->numChars*4;
        TextVertex* verts = batch->verts + firstVertIdx;
        uint16_t* indices = batch->indices + batch->numChars*6;

        float dir = 1.0f;
        float dirInvFactor = 0;
        if (flags & TextFlags::RightToLeft) {
            dir = -1.0f;
            dirInvFactor = 1.0f;
        }
        float advanceScale = 1.0f;
        if (flags & TextFlags::Narrow)
            advanceScale = 0.9f;
       
        for (int i = 0; i < len; i++) {
            const FontGlyph& glyph = glyphs[i];
                
            TextVertex& v0 = verts[vertexIdx];
            TextVertex& v1 = verts[vertexIdx + 1];
            TextVertex& v2 = verts[vertexIdx + 2];
            TextVertex& v3 = verts[vertexIdx + 3];

            float gw = glyph.width*scale;
            float gh = glyph.height*scale;
            float xoffset = glyph.xoffset*scale;
            float yoffset = glyph.yoffset*scale;
            float gx = glyph.x;
            float gy = glyph.y;

            x -= glyph.xadvance*scale*dirInvFactor*advanceScale;

            // Top-Left
            v0.x = x + xoffset;
            v0.y = yoffset;
            v0.tx = gx / texSize.x;
            v0.ty = gy / texSize.y;

            // Top-Right
            v1.x = x + xoffset + gw;
            v1.y = yoffset;
            v1.tx = (gx + glyph.width) / texSize.x;
            v1.ty = gy / texSize.y;

            // Bottom-Left
            v2.x = x + xoffset;
            v2.y = yoffset + gh;
            v2.tx = gx / texSize.x;
            v2.ty = (gy + glyph.height) / texSize.y;

            // Bottom-Right
            v3.x = x + xoffset + gw;
            v3.y = yoffset + gh;
            v3.tx = (gx + glyph.width) / texSize.x;
            v3.ty = (gy + glyph.height) / texSize.y;

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

        batch->numChars += len;

        // We have the final width, Calculate alignment
        vec2_t pos = vec2f(0, -font->lineHeight*0.5f*scale);

        if (flags & TextFlags::AlignCenter) {
            pos = pos + ((screenRect.vmax + screenRect.vmin)*0.5f - vec2f(x*0.5f, 0));
        } else if (flags & TextFlags::AlignLeft) {
            pos = pos + vec2f(screenRect.xmin - x*dirInvFactor, (screenRect.ymax + screenRect.ymin)*0.5f);
        } else if (flags & TextFlags::AlignRight) {
            pos = pos + vec2f(screenRect.xmax - x*(1.0f - dirInvFactor), (screenRect.ymax + screenRect.ymin)*0.5f);
        }

        // Transform vertices by pos
        for (int i = 0; i < maxVerts; i++) {
            verts[i].x += pos.x;
            verts[i].y += pos.y;
        }
    }

    void addTextf(TextBatch* batch, float scale, const rect_t& rectFit, TextFlags::Bits flags, const char* fmt, ...)
    {
        char text[256];
        va_list args;
        va_start(args, fmt);
        bx::vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        addText(batch, scale, rectFit, flags, text);
    }

    static void drawTextBatch(GfxDriverApi* gDriver, TextBatch* batch, uint8_t viewId, ProgramHandle prog, Font* font,
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

            mtx4x4_t transformMat = batch->transformMtx;
            transformMat.m41 += offsetPos.x;
            transformMat.m42 += offsetPos.y;
            gDriver->setUniform(g_fontSys->uTransformMtx, transformMat.f, 1);
            gDriver->setUniform(g_fontSys->uColor, color.f, 1);
            gDriver->setState(gfxStateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite, 0);
            gDriver->setTexture(0, g_fontSys->uTexture, getResourcePtr<Texture>(font->texHandles[0])->handle,
                                TextureFlag::FromTexture);
            gDriver->setTransientVertexBuffer(0, &tvb);
            gDriver->setTransientIndexBuffer(&tib);
            gDriver->submit(viewId, prog, 0, false);
        }
    }

    void drawText(TextBatch* batch, uint8_t viewId, const vec2_t& offset, const vec4_t& color)
    {
        if (batch->numChars > 0) {
            GfxDriverApi* gDriver = getGfxDriver();
            Font* font = getResourcePtr<Font>(batch->fontHandle);

            gDriver->setUniform(g_fontSys->uParams, vec4f(0, 0, 0, 0).f, 1);
            ProgramHandle prog = (font->flags & FontFlags::DistantField) ? g_fontSys->dfProg : g_fontSys->normalProg;
            drawTextBatch(gDriver, batch, viewId, prog, font, offset, color);
        }
    }

    void drawText(TextBatch* batch, uint8_t viewId, color_t color)
    {
        if (batch->numChars > 0) {
            GfxDriverApi* gDriver = getGfxDriver();
            Font* font = getResourcePtr<Font>(batch->fontHandle);

            ProgramHandle prog = (font->flags & FontFlags::DistantField) ? g_fontSys->dfProg : g_fontSys->normalProg;
            drawTextBatch(gDriver, batch, viewId, prog, font, vec2f(0, 0), colorToVec4(color));
        }
    }

    void drawTextDropShadow(TextBatch* batch, uint8_t viewId, color_t color, color_t shadowColor, vec2_t shadowAmount)
    {
        if (batch->numChars > 0) {
            GfxDriverApi* gDriver = getGfxDriver();
            Font* font = getResourcePtr<Font>(batch->fontHandle);

            if (!(font->flags & FontFlags::Persian)) {
                vec2_t shadowOffset = vec2f(shadowAmount.x/font->scaleW, shadowAmount.y/font->scaleH);
                gDriver->setUniform(g_fontSys->uParams, vec4f(0, 0, shadowOffset.x, shadowOffset.y).f, 1);
                gDriver->setUniform(g_fontSys->uShadowColor, colorToVec4(shadowColor).f, 1);
                ProgramHandle prog = (font->flags & FontFlags::DistantField) ? g_fontSys->dfShadowProg : g_fontSys->normalShadowProg;
                drawTextBatch(gDriver, batch, viewId, prog, font, vec2f(0, 0), colorToVec4(color));
            } else {
                vec2_t shadowOffset = vec2f(2.0f*shadowAmount.x/batch->screenSize.x, -2.0f*shadowAmount.y/batch->screenSize.y);
                ProgramHandle prog = (font->flags & FontFlags::DistantField) ? g_fontSys->dfProg : g_fontSys->normalProg;
                drawTextBatch(gDriver, batch, viewId, prog, font, shadowOffset, colorToVec4(shadowColor));
                drawTextBatch(gDriver, batch, viewId, prog, font, vec2f(0, 0), colorToVec4(color));
            }
        }
    }

    void drawTextOutline(TextBatch* batch, uint8_t viewId, color_t color, color_t outlineColor, float outlineAmount)
    {
        if (batch->numChars > 0) {
            GfxDriverApi* gDriver = getGfxDriver();
            Font* font = getResourcePtr<Font>(batch->fontHandle);

            float outline = 0.5f*outlineAmount;
            gDriver->setUniform(g_fontSys->uParams, vec4f(0, 0, outline, 0).f, 1);
            gDriver->setUniform(g_fontSys->uOutlineColor, colorToVec4(outlineColor).f, 1);
            gDriver->setUniform(g_fontSys->uTextureSize, vec4f(float(font->scaleW), float(font->scaleH), 0, 0).f, 1);
            ProgramHandle prog = (font->flags & FontFlags::DistantField) ? g_fontSys->dfOutlineProg : g_fontSys->normalOutlineProg;
            drawTextBatch(gDriver, batch, viewId, prog, font, vec2f(0, 0), vec4f(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    void destroyTextBatch(TextBatch* batch)
    {
        assert(g_fontSys);
        assert(batch->alloc);

        if (batch->verts)
            BX_FREE(batch->alloc, batch->verts);
        if (batch->indices)
            BX_FREE(batch->alloc, batch->indices);

        g_fontSys->batchPool.deleteInstance(batch);
    }

}
