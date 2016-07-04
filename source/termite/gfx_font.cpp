#include "pch.h"
#include "io_driver.h"
#include "gfx_font.h"
#include "gfx_texture.h"

#include "bx/hash.h"
#include "bx/uint32_t.h"
#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"

using namespace termite;

// FNT file format
#define FNT_SIGN "BMF"

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

struct FontItem
{
    char name[32];
    uint16_t size;
    FontFlags flags;
    Font* font;
    
    typedef bx::ListNode<FontItem*> LNode;
    LNode lnode;
};

struct FontLibrary
{
    IoDriverI* ioDriver;
    bx::AllocatorI* alloc;
    bx::HashTable<FontItem*> fontTable;
    FontItem::LNode* fontList;
    bx::Pool<FontItem> fontItemPool;

    FontLibrary() : fontTable(bx::HashTableType::Mutable)
    {
        ioDriver = nullptr;
        alloc = nullptr;
        fontList = nullptr;
    }
};

static FontLibrary* g_fontLib = nullptr;

termite::Font::Font(bx::AllocatorI* alloc) :
    m_charTable(bx::HashTableType::Immutable),
    m_alloc(alloc)
{
    m_name[0] = 0;
    m_numChars = m_numKerns = m_lineHeight = m_baseValue = m_fsize = m_charWidth = 0;
    m_kerns = nullptr;
    m_glyphs = nullptr;
    m_resLib = nullptr;
}

Font* termite::Font::create(const MemoryBlock* mem, const char* fntFilepath, bx::AllocatorI* alloc, ResourceLib* resLib)
{
    bx::Error err;
    bx::MemoryReader ms(mem->data, mem->size);

    Font* font = BX_NEW(alloc, Font)(alloc);
    if (!font)
        return nullptr;
    font->m_resLib = resLib;    

    // Sign
    char sign[4];
    ms.read(sign, 3, &err);  sign[3] = 0;
    if (strcmp(FNT_SIGN, sign) != 0) {
        T_ERROR("Loading font '%s' failed: Invalid FNT (bmfont) binary file", fntFilepath);
        BX_DELETE(alloc, font);
        return nullptr;
    }

    // File version
    uint8_t fileVersion;
    ms.read(&fileVersion, sizeof(fileVersion), &err);
    if (fileVersion != 3) {
        T_ERROR("Loading font '%s' failed: Invalid file version", fntFilepath);
        BX_DELETE(alloc, font);
        return nullptr;
    }

    //
    fntBlock_t block;
    fntInfo_t info;
    char fontName[256];
    ms.read(&block, sizeof(block), &err);
    ms.read(&info, sizeof(info), &err);
    ms.read(fontName, block.size - sizeof(info), &err);
    font->m_fsize = info.font_size;
    strcpy(font->m_name, fontName);

    // Common
    fntCommon_t common;
    ms.read(&block, sizeof(block), &err);
    ms.read(&common, sizeof(common), &err);

    if (common.pages != 1) {
        T_ERROR("Loading font '%s' failed: Invalid number of pages", fntFilepath);
        BX_DELETE(alloc, font);
        return nullptr;
    }

    font->m_lineHeight = common.line_height;
    font->m_baseValue = common.base;

    // Font pages (textures)
    fntPage_t page;

    ms.read(&block, sizeof(block), &err);
    ms.read(page.path, block.size, &err);
    bx::Path texFilepath = bx::Path(fntFilepath).getDirectory();
    texFilepath.join(page.path).toUnix();
    LoadTextureParams texParams;

    if (texFilepath.getFileExt().isEqualNoCase("dds"))
        font->m_textureHandle = loadResource(resLib, "texture", texFilepath.cstr(), &texParams);
    else
        font->m_textureHandle = loadResource(resLib, "image", texFilepath.cstr(), &texParams);

    if (!font->m_textureHandle.isValid()) {
        T_ERROR("Loading font '%s' failed: Loading texture '%s' failed", fntFilepath, texFilepath.cstr());
        BX_DELETE(alloc, font);
        return nullptr;
    }

    // Characters
    fntChar_t ch;
    ms.read(&block, sizeof(block), &err);
    uint32_t numChars = block.size / sizeof(ch);
    font->m_numChars = numChars;

    // Hashtable for characters
    if (!font->m_charTable.create(numChars, alloc)) {
        BX_DELETE(alloc, font);
        return nullptr;
    }

    font->m_glyphs = (FontGlyph*)BX_ALLOC(alloc, sizeof(FontGlyph)*numChars);
    assert(font->m_glyphs);
    memset(font->m_glyphs, 0x00, sizeof(FontGlyph)*numChars);

    uint32_t charWidth = 0;
    for (int i = 0; i < (int)numChars; i++) {
        ms.read(&ch, sizeof(ch), &err);
        font->m_glyphs[i].glyphId = ch.id;
        font->m_glyphs[i].width = (float)ch.width;
        font->m_glyphs[i].height = (float)ch.height;
        font->m_glyphs[i].x = (float)ch.x;
        font->m_glyphs[i].y = (float)ch.y;
        font->m_glyphs[i].xadvance = (float)ch.xadvance;
        font->m_glyphs[i].xoffset = (float)ch.xoffset;
        font->m_glyphs[i].yoffset = (float)ch.yoffset;

        font->m_charTable.add(ch.id, i);

        charWidth = bx::uint32_max(charWidth, ch.xadvance);
    }
    font->m_charWidth = charWidth;

    // Kernings
    fntKernPair_t kern;
    int last_r = ms.read(&block, sizeof(block), &err);
    uint32_t numKerns = block.size / sizeof(kern);
    if (numKerns > 0 && last_r > 0) {
        font->m_kerns = (FontKerning*)BX_ALLOC(alloc, sizeof(FontKerning)*numKerns);
        assert(font->m_kerns);
        memset(font->m_kerns, 0x00, sizeof(FontKerning)*numKerns);

        for (uint32_t i = 0; i < numKerns; i++) {
            ms.read(&kern, sizeof(kern), &err);

            // Find char and set it's kerning index
            uint32_t id = kern.first;
            for (uint32_t k = 0; k < numChars; k++) {
                if (id == font->m_glyphs[k].glyphId) {
                    if (font->m_glyphs[k].numKerns == 0)
                        font->m_glyphs[k].kernIdx = i;
                    font->m_glyphs[k].numKerns++;
                    break;
                }
            }

            font->m_kerns[i].secondGlyph = kern.second;
            font->m_kerns[i].amount = (float)kern.amount;
        }
    }

    return font;
}

termite::Font::~Font()
{
    if (m_alloc) {
        bx::AllocatorI* alloc = m_alloc;

        if (m_textureHandle.isValid())
            unloadResource(m_resLib, m_textureHandle);

        m_charTable.destroy();

        if (m_glyphs)
            BX_FREE(alloc, m_glyphs);

        if (m_kerns)
            BX_FREE(alloc, m_kerns);
    }
}

float termite::Font::getTextWidth(const char* text, int len, float* firstcharWidth)
{
    if (len <= 0)
        len = (int)strlen(text);

    float width = 0;
    int glyphIdx = this->findGlyph(text[0]);
    if (glyphIdx != -1) {
        width = m_glyphs[glyphIdx].xadvance;
        if (firstcharWidth)
            *firstcharWidth = width;
    }

    for (int i = 1; i < (len - 1); i++) {
        glyphIdx = this->findGlyph(text[i]);
        if (glyphIdx != -1) {
            width += m_glyphs[glyphIdx].xadvance;
            int nextIdx = this->findGlyph(text[i + 1]);
            if (nextIdx != -1)
                width += this->applyKern(glyphIdx, nextIdx);
        }
    }

    return width;
}

float termite::Font::applyKern(int glyphIdx, int nextGlyphIdx) const
{
    const FontGlyph& ch = m_glyphs[glyphIdx];
    const FontGlyph& nextCh = m_glyphs[nextGlyphIdx];

    for (uint32_t i = 0, c = ch.numKerns; i < c; i++) {
        if (m_kerns[ch.kernIdx + i].secondGlyph == nextCh.glyphId)
            return m_kerns[ch.kernIdx + i].amount;
    }
    return 0;
}

result_t termite::initFontLib(bx::AllocatorI* alloc, IoDriverI* ioDriver)
{
    assert(ioDriver);
    assert(alloc);

    if (g_fontLib) {
        assert(false);
        return T_ERR_FAILED;
    }

    FontLibrary* lib = BX_NEW(alloc, FontLibrary);
    if (!lib)
        return T_ERR_OUTOFMEM;

    lib->alloc = alloc;
    lib->ioDriver = ioDriver;

    if (!lib->fontItemPool.create(32, alloc) ||
        !lib->fontTable.create(32, alloc)) {
        return T_ERR_OUTOFMEM;
    }

    g_fontLib = lib;

    return 0;
}
 
void termite::shutdownFontLib()
{
    if (!g_fontLib)
        return;

    bx::AllocatorI* alloc = g_fontLib->alloc;

    // Delete all fonts
    FontItem::LNode* node = g_fontLib->fontList;
    while (node) {
        FontItem* libItem = node->data;
        if (libItem->font)
            BX_DELETE(alloc, libItem->font);
        g_fontLib->fontItemPool.deleteInstance(libItem);
        node = node->next;
    }
    g_fontLib->fontList = nullptr;
    g_fontLib->fontItemPool.destroy();
    g_fontLib->fontTable.destroy();
    BX_DELETE(alloc, g_fontLib);
    g_fontLib = nullptr;
}

static uint32_t hashFontItem(const char* name, uint16_t size, FontFlags flags)
{
    bx::HashMurmur2A hasher;
    uint32_t flags_ = uint32_t(flags);
    hasher.begin();
    hasher.add(name, (int)strlen(name));
    hasher.add(&size, (int)sizeof(size));
    hasher.add(&flags_, (int)sizeof(uint32_t));
    return hasher.end();
}

result_t termite::registerFont(const char* fntFilepath, const char* name, uint16_t size, FontFlags flags)
{
    assert(fntFilepath);
    assert(g_fontLib);

    bx::AllocatorI* alloc = g_fontLib->alloc;

    // Search for existing font
    uint32_t hash = hashFontItem(name, size, flags);
    int fontIndex = g_fontLib->fontTable.find(hash);
    if (fontIndex != -1)
        return T_ERR_ALREADY_EXISTS;

    // Load the file into memory
    MemoryBlock* mem = g_fontLib->ioDriver->read(fntFilepath);
    if (!mem) {
        T_ERROR("Could not open font file '%s'", fntFilepath);
        return T_ERR_IO_FAILED;
    }
    Font* font = Font::create(mem, fntFilepath, g_fontLib->alloc);
    releaseMemoryBlock(mem);

    if (!font) {
        T_ERROR("Loading font '%s' failed", fntFilepath);
        return T_ERR_FAILED;
    }

    // Add font to library
    FontItem* item = g_fontLib->fontItemPool.newInstance();
    if (!item) {
        BX_DELETE(alloc, font);
        return T_ERR_OUTOFMEM;
    }
    bx::strlcpy(item->name, name, sizeof(item->name));
    item->size = size;
    item->flags = flags;
    item->font = font;

    bx::addListNode<FontItem*>(&g_fontLib->fontList, &item->lnode, item);
    g_fontLib->fontTable.add(hash, item);    
    return 0;
}

void termite::unregisterFont(const char* name, uint16_t size /*= 0*/, FontFlags flags /*= fntFlags::Normal*/)
{
    uint32_t hash = hashFontItem(name, size, flags);
    int fontIndex = g_fontLib->fontTable.find(hash);
    if (fontIndex != -1) {
        FontItem* item = g_fontLib->fontTable.getValue(fontIndex);
        
        bx::removeListNode<FontItem*>(&g_fontLib->fontList, &item->lnode);
        g_fontLib->fontTable.remove(fontIndex);
        
        if (item->font)
            BX_DELETE(g_fontLib->alloc, item->font);

        g_fontLib->fontItemPool.deleteInstance(item);
    }
}

const Font* termite::getFont(const char* name, uint16_t size /*= 0*/, FontFlags flags /*= fntFlags::Normal*/)
{
    uint32_t hash = hashFontItem(name, size, flags);
    int fontIndex = g_fontLib->fontTable.find(hash);
    if (fontIndex != -1)
        return g_fontLib->fontTable.getValue(fontIndex)->font;
    else
        return nullptr;
}
