#pragma once

#include "bx/allocator.h"
#include "bxx/hash_table.h"
#include "resource_lib.h"

namespace termite
{
    struct MemoryBlock;
    class Font;
    struct IoDriverApi;
    struct Texture;

    struct FontFlags
    {
        enum Enum
        {
            Normal = 0x0,
            Bold = 0x1,
            Italic = 0x2,
            Underline = 0x4
        };

        typedef uint8_t Bits;
    };

    // Font Library
    result_t initFontLib(bx::AllocatorI* alloc, IoDriverApi* ioDriver);
    void shutdownFontLib();

    TERMITE_API result_t registerFont(const char* fntFilepath, const char* name, uint16_t size = 0, 
                                      FontFlags::Enum flags = FontFlags::Normal);
    TERMITE_API void unregisterFont(const char* name, uint16_t size = 0, FontFlags::Enum flags = FontFlags::Normal);
    TERMITE_API const Font* getFont(const char* name, uint16_t size = 0, FontFlags::Enum flags = FontFlags::Normal);

    struct FontKerning
    {
        uint32_t secondGlyph;
        float amount;
    };

    struct FontGlyph
    {
        uint32_t glyphId;
        float x;
        float y;
        float width;
        float height;
        float xoffset;
        float yoffset;
        float xadvance;
        uint16_t numKerns;
        uint32_t kernIdx;
    };

    class Font
    {
        friend result_t termite::registerFont(const char*, const char*, uint16_t, FontFlags::Enum);

    private:
        bx::AllocatorI* m_alloc;
        char m_name[32];
        ResourceHandle m_textureHandle;
        uint32_t m_numChars;
        uint32_t m_numKerns;
        uint16_t m_lineHeight;
        uint16_t m_baseValue;
        uint32_t m_fsize;
        uint32_t m_charWidth;    // fixed width fonts
        FontGlyph* m_glyphs;
        FontKerning* m_kerns;
        bx::HashTableInt m_charTable;
        ResourceLibHelper m_resLib;

    public:
        Font(bx::AllocatorI* alloc);
        static Font* create(const MemoryBlock* mem, const char* fntFilepath, bx::AllocatorI* alloc);

    public:
        ~Font();

        const char* getInternalName() const
        {
            return m_name;
        }

        uint16_t getLineHeight() const
        {
            return m_lineHeight;
        }

        int getCharCount() const
        {
            return m_numChars;
        }

        uint32_t getCharWidth() const
        {
            return m_charWidth;
        }

        Texture* getTexture() const
        {
            return m_resLib.getResourcePtr<Texture>(m_textureHandle);
        }

        int findGlyph(uint32_t glyphId) const
        {
            int index = m_charTable.find(glyphId);
            return index != -1 ? m_charTable.getValue(index) : -1;
        }

        const FontGlyph& getGlyph(int glyphIdx) const
        {
            assert(glyphIdx != -1);
            return m_glyphs[glyphIdx];
        }

        float getTextWidth(const char* text, int len = -1, float* firstcharWidth = nullptr);
        float applyKern(int glyphIdx, int nextGlyphIdx) const;
    };
} // namespace termite
