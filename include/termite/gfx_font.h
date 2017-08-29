#pragma once

#include "bx/allocator.h"
#include "bxx/hash_table.h"
#include "resource_lib.h"
#include "vec_math.h"

namespace termite
{
    struct Font;
    struct TextBatch;

    struct FontFileFormat
    {
        enum Enum
        {
            Text = 0,
            Binary
        };
    };

    struct FontFlags
    {
        enum Enum
        {
            Normal = 0,
            Bold = 0x1,
            Italic = 0x2,
            DistantField = 0x4,
            Unicode = 0x8,
            Persian = 0x10
        };

        typedef uint8_t Bits;
    };

    struct LoadFontParams
    {
        FontFileFormat::Enum format;
        bool generateMips;
        FontFlags::Bits flags;
        uint8_t padding[2];

        LoadFontParams()
        {
            format = FontFileFormat::Text;
            generateMips = true;
            flags = 0;
            padding[0] = padding[1] = 0;
        }
    };

    struct FontGlyph
    {
        uint16_t charId;
        float x;
        float y;
        float width;
        float height;
        float xoffset;
        float yoffset;
        float xadvance;
        int numKerns;
        int kernIdx;
    };

    result_t initFontSystem(bx::AllocatorI* alloc, const vec2_t refScreenSize = vec2f(-1.0f, -1.0f));
    void shutdownFontSystem();

    bool initFontSystemGraphics();
    void shutdownFontSystemGraphics();

    // Font Info (Custom rendering)
    TERMITE_API ResourceHandle getFontTexture(Font* font, int pageId = 0);
    TERMITE_API vec2_t getFontTextureSize(Font* font);
    TERMITE_API float getFontLineHeight(Font* font);
    TERMITE_API float getFontTextWidth(Font* font, const char* text, int len, float* firstcharWidth);
    TERMITE_API int findFontCharGlyph(Font* font, uint16_t chId);
    TERMITE_API const FontGlyph& getFontGlyph(Font* font, int index);
    TERMITE_API float getFontGlyphKerning(Font* font, int glyphIdx, int nextGlyphIdx);

    struct TextFlags
    {
        enum Enum
        {
            AlignCenter = 0x1,
            AlignRight = 0x2,
            AlignLeft = 0x4,
            RightToLeft = 0x8,
            LeftToRight = 0x10,
            Narrow = 0x20
        };

        typedef uint8_t Bits;
    };

    TERMITE_API TextBatch* createTextBatch(int maxChars, ResourceHandle fontHandle, bx::AllocatorI* alloc);
    TERMITE_API void beginText(TextBatch* batch, const mtx4x4_t& viewProjMtx, const vec2_t screenSize);
    TERMITE_API void addText(TextBatch* batch, color_t color, float scale,
                             const rect_t& rectFit, TextFlags::Bits flags,
                             const char* text);
    TERMITE_API void addTextf(TextBatch* batch, color_t color, float scale,
                              const rect_t& rectFit, TextFlags::Bits flags,
                              const char* fmt, ...);
    TERMITE_API void drawText(TextBatch* batch, uint8_t viewId);
    TERMITE_API void drawTextDropShadow(TextBatch* batch, uint8_t viewId, color_t shadowColor = color1n(0xff000000), 
                                        vec2_t shadowAmount = vec2f(2.0f, 2.0f));
    TERMITE_API void drawTextOutline(TextBatch* batch, uint8_t viewId, color_t outlineColor = color1n(0xff000000), 
                                     float outlineAmount = 0.5f);
    TERMITE_API void drawTextDropShadowTwoPass(TextBatch* batch, uint8_t viewId, color_t shadowColor = color1n(0xff000000),
                                               vec2_t shadowAmount = vec2f(2.0f, 2.0f));

    TERMITE_API void destroyTextBatch(TextBatch* batch);

    void registerFontToResourceLib();
} // namespace termite
