#include "pch.h"
#include "datastore.h"
#include "datastore_driver.h"
#include "gfx_font.h"

#include "bx/readerwriter.h"
#include "bx/string.h"

using namespace termite;

// FNT file format
#define FNT_SIGN "BMF"

#pragma pack(push, 1)
struct fnt_info
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

struct fnt_common
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

struct fnt_page
{
    char path[255];
};

struct fnt_char
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

struct fnt_kernpair
{
    uint32_t first;
    uint32_t second;
    int16_t amount;
};

struct fnt_block
{
    uint8_t id;
    uint32_t size;
};
#pragma pack(pop)

static gfx_font* loadFntFile(const MemoryBlock* mem, bx::AllocatorI* alloc)
{
    bx::Error err;
    bx::MemoryReader ms(mem->data, mem->size);
    gfx_font* font = BX_NEW(alloc, gfx_font);
    if (!font)
        return nullptr;
    
    // Sign
    char sign[4];
    ms.read(sign, 3, &err);  sign[3] = 0;
    if (strcmp(FNT_SIGN, sign) != 0)
        return nullptr;

    // File version
    uint8_t file_ver;
    ms.read(&file_ver, sizeof(file_ver), &err);
    if (file_ver != 3)
        return nullptr;

    //
    fnt_block block;
    fnt_info info;
    char font_name[256];
    ms.read(&block, sizeof(block), &err);
    ms.read(&info, sizeof(info), &err);
    ms.read(font_name, block.size - sizeof(info), &err);
    font->fsize = info.font_size;
    strcpy(font->name, font_name);

    // Common
    fnt_common common;
    ms.read(&block, sizeof(block), &err);
    ms.read(&common, sizeof(common), &err);

    if (common.pages != 1)
        return nullptr;

    font->line_height = common.line_height;
    font->base_value = common.base;

    /* pages */
    fnt_page page;
    char texpath[255];

    ms.read(&block, sizeof(block), &err);
    ms.read(page.path, block.size, &err);
    path_getdir(texpath, fio_getpath(f));
    path_join(texpath, texpath, page.path, NULL);
    path_tounix(texpath, texpath);
    font->tex_hdl = rs_load_texture(texpath, 0, FALSE, 0);
    if (font->tex_hdl == INVALID_HANDLE)
        return nullptr;

    /* chars */
    fnt_char ch;
    ms.read(f, &block, sizeof(block), 1);
    uint32_t char_cnt = block.size / sizeof(ch);
    font->char_cnt = char_cnt;

    /* hash table */
    if (IS_FAIL(hashtable_fixed_create(alloc, &font->char_table, char_cnt, MID_GFX)))
        return nullptr;

    font->chars = (gfx_font_chardesc*)A_ALLOC(alloc, sizeof(gfx_font_chardesc)*char_cnt, MID_GFX);
    ASSERT(font->chars);
    memset(font->chars, 0x00, sizeof(struct gfx_font_chardesc)*char_cnt);

    uint32_t cw_max = 0;
    for (uint32_t i = 0; i < char_cnt; i++) {
        ms.read(f, &ch, sizeof(ch), 1);
        font->chars[i].char_id = ch.id;
        font->chars[i].width = (float)ch.width;
        font->chars[i].height = (float)ch.height;
        font->chars[i].x = (float)ch.x;
        font->chars[i].y = (float)ch.y;
        font->chars[i].xadvance = (float)ch.xadvance;
        font->chars[i].xoffset = (float)ch.xoffset;
        font->chars[i].yoffset = (float)ch.yoffset;

        if (cw_max < (uint32_t)ch.xadvance)
            cw_max = ch.width;

        hashtable_fixed_add(&font->char_table, ch.id, i);
    }

    if (char_cnt > 0)
        font->char_width = (uint32_t)font->chars[0].xadvance;

    /* kerning */
    struct fnt_kernpair kern;
    size_t last_r = ms.read(f, &block, sizeof(block), 1);
    uint32_t kern_cnt = block.size / sizeof(kern);
    if (kern_cnt > 0 && last_r > 0) {
        font->kerns = (struct gfx_font_kerning*)A_ALLOC(alloc,
            sizeof(struct gfx_font_kerning)*kern_cnt, MID_GFX);
        ASSERT(font->kerns);
        memset(font->kerns, 0x00, sizeof(struct gfx_font_kerning)*kern_cnt);

        for (uint32_t i = 0; i < kern_cnt; i++) {
            ms.read(f, &kern, sizeof(kern), 1);

            /* find char id and set it's kerning */
            uint32_t id = kern.first;
            for (uint32_t k = 0; k < char_cnt; k++) {
                if (id == font->chars[k].char_id) {
                    if (font->chars[k].kern_cnt == 0)
                        font->chars[k].kern_idx = i;
                    font->chars[k].kern_cnt++;
                    break;
                }
            }

            font->kerns[i].second_char = kern.second;
            font->kerns[i].amount = (float)kern.amount;
        }
    }

    return nullptr;
}

void unload_font(struct bx::AllocatorI* alloc, struct gfx_font* font)
{
    ASSERT(font);

    hashtable_fixed_destroy(&font->char_table);

    if (font->tex_hdl != INVALID_HANDLE)
        rs_unload(font->tex_hdl);

    if (font->chars != NULL)
        A_FREE(alloc, font->chars);

    if (font->kerns != NULL)
        A_FREE(alloc, font->kerns);

    if (font->meta_rules != NULL)
        A_FREE(alloc, font->meta_rules);

    memset(font, 0x00, sizeof(struct gfx_font));
}
