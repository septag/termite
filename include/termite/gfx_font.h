#pragma once

#include "bx/bx.h"
#include "bxx/hash_table.h"

namespace termite
{
    enum class gfx_font_flags : uint8_t
    {
        Normal = 0x1,
        Bold = 0x2,
        Italic = 0x4,
        Underline = 0x8
    };

    struct gfx_font_kerning
    {
        uint32_t second_char;
        float amount;
    };

    struct gfx_font_chardesc
    {
        uint32_t char_id;
        float x;
        float y;
        float width;
        float height;
        float xoffset;
        float yoffset;
        float xadvance;
        uint16_t kern_cnt;
        uint32_t kern_idx;
    };

    class gfx_font
    {
    public:
        char name[32];
        dsResourceHandle tex_hdl;
        uint32_t char_cnt;
        uint32_t kern_cnt;
        uint16_t line_height;
        uint16_t base_value;
        uint32_t fsize;
        uint32_t flags;
        uint32_t char_width;    // fixed width fonts
        gfx_font_chardesc* chars;
        gfx_font_kerning* kerns;
        uint32_t meta_cnt;
        bx::HashTableInt char_table;
    };
} // namespace termite