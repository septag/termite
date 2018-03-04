#pragma once

#include "bx/allocator.h"
#include "bxx/hash_table.h"
#include "assetlib.h"
#include "math.h"

namespace tee
{
    struct Font;
    struct TextDraw;

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

    struct TextFlags
    {
        enum Enum
        {
            AlignCenter = 0x1,
            AlignRight = 0x2,
            AlignLeft = 0x4,
            RightToLeft = 0x8,
            LeftToRight = 0x10,
            Narrow = 0x20,
            Multiline = 0x40,
            Dim = 0x80
        };

        typedef uint8_t Bits;
    };

    namespace gfx {
        // Font Info (Custom rendering)
        TEE_API AssetHandle getFontTexture(Font* font, int pageId = 0);
        TEE_API vec2_t getFontTextureSize(Font* font);
        TEE_API float getFontLineHeight(Font* font);
        TEE_API float getFontTextWidth(Font* font, const char* text, int len, float* firstcharWidth);
        TEE_API int findFontCharGlyph(Font* font, uint16_t chId);
        TEE_API const FontGlyph& getFontGlyph(Font* font, int index);
        TEE_API float getFontGlyphKerning(Font* font, int glyphIdx, int nextGlyphIdx);
        TEE_API bool fontIsUnicode(Font* font);

        // Text Drawing
        TEE_API TextDraw* createTextDraw(int maxChars, AssetHandle fontHandle, bx::AllocatorI* alloc);
        TEE_API void beginText(TextDraw* batch, const mat4_t& viewProjMtx, const vec2_t screenSize);
        TEE_API void endText(TextDraw* batch);
        TEE_API void addText(TextDraw* batch, float scale, const rect_t& rectFit, TextFlags::Bits flags, const char* text);
        TEE_API void addTextf(TextDraw* batch, float scale, const rect_t& rectFit, TextFlags::Bits flags, const char* fmt, ...);
        TEE_API void resetText(TextDraw* batch);   // Reset char buffer, so we can render with another color
        TEE_API void drawText(TextDraw* batch, uint8_t viewId, ucolor_t color);
        TEE_API void drawTextDropShadow(TextDraw* batch, uint8_t viewId, ucolor_t color,
                                        ucolor_t shadowColor = ucolor(0xff000000), vec2_t shadowAmount = vec2(2.0f, 2.0f));
        TEE_API void drawTextOutline(TextDraw* batch, uint8_t viewId, ucolor_t color,
                                     ucolor_t outlineColor = ucolor(0xff000000), float outlineAmount = 0.5f);
        TEE_API void destroyTextDraw(TextDraw* batch);
    }
} // namespace tee
