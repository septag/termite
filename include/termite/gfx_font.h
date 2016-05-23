#pragma once

#include "bx/allocator.h"
#include "bxx/hash_table.h"
#include "datastore.h"

namespace termite
{
    struct MemoryBlock;
    class fntFont;
    class dsDriverI;
    struct gfxTexture;

    enum class fntFlags : uint8_t
    {
        Normal = 0x0,
        Bold = 0x1,
        Italic = 0x2,
        Underline = 0x4
    };

    // Font Library
    result_t fntInitLibrary(bx::AllocatorI* alloc, dsDriverI* datastoreDriver);
    void fntShutdownLibrary();

    TERMITE_API result_t fntRegister(const char* fntFilepath, const char* name, uint16_t size = 0, fntFlags flags = fntFlags::Normal);
    TERMITE_API void fntUnregister(const char* name, uint16_t size = 0, fntFlags flags = fntFlags::Normal);
    TERMITE_API const fntFont* fntGet(const char* name, uint16_t size = 0, fntFlags flags = fntFlags::Normal);

    struct fntKerning
    {
        uint32_t secondGlyph;
        float amount;
    };

    struct fntGlyph
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

    class fntFont
    {
        friend result_t termite::fntRegister(const char*, const char*, uint16_t, fntFlags);

    private:
        bx::AllocatorI* m_alloc;
        char m_name[32];
        dsResourceHandle m_textureHandle;
        uint32_t m_numChars;
        uint32_t m_numKerns;
        uint16_t m_lineHeight;
        uint16_t m_baseValue;
        uint32_t m_fsize;
        uint32_t m_charWidth;    // fixed width fonts
        fntGlyph* m_glyphs;
        fntKerning* m_kerns;
        bx::HashTableInt m_charTable;
        dsDataStore* m_ds;

    public:
        fntFont(bx::AllocatorI* alloc);
        static fntFont* create(const MemoryBlock* mem, const char* fntFilepath, bx::AllocatorI* alloc, dsDataStore* ds = nullptr);

    public:
        ~fntFont();

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

        gfxTexture* getTexture() const
        {
            return dsGetObjPtr<gfxTexture*>(m_ds, m_textureHandle);
        }

        int findGlyph(uint32_t glyphId) const
        {
            int index = m_charTable.find(glyphId);
            return index != -1 ? m_charTable.getValue(index) : -1;
        }

        const fntGlyph& getGlyph(int glyphIdx) const
        {
            assert(glyphIdx != -1);
            return m_glyphs[glyphIdx];
        }

        float getTextWidth(const char* text, int len = -1, float* firstcharWidth = nullptr);
        float applyKern(int glyphIdx, int nextGlyphIdx) const;
    };
} // namespace termite

C11_DEFINE_FLAG_TYPE(termite::fntFlags);
