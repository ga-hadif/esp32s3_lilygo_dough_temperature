/*******************************************************************************
 * Size: 48 px
 * Bpp: 1
 * Opts: --bpp 1 --size 48 --no-compress --font fa-solid-900.woff --range 62155 --format lvgl -o temp_icon.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef TEMP_ICON
#define TEMP_ICON 1
#endif

#if TEMP_ICON

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap_temp[] = {
    /* U+F2CB "" */
    0x0, 0x7e, 0x0, 0x1, 0xff, 0x80, 0x3, 0xff,
    0xc0, 0x7, 0xff, 0xe0, 0xf, 0xc3, 0xf0, 0xf,
    0x81, 0xf0, 0xf, 0x0, 0xf0, 0x1f, 0x0, 0xf8,
    0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0,
    0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f,
    0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8,
    0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0,
    0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f,
    0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8,
    0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0,
    0xf8, 0x1f, 0x0, 0xf8, 0x1f, 0x0, 0xf8, 0x3f,
    0x0, 0xfc, 0x7e, 0x3e, 0x7e, 0x7c, 0xff, 0x3e,
    0xfd, 0xff, 0xbe, 0xfb, 0xff, 0x9f, 0xfb, 0xff,
    0xdf, 0xfb, 0xff, 0xdf, 0xfb, 0xff, 0xdf, 0xfb,
    0xff, 0xdf, 0xfb, 0xff, 0x9f, 0xfd, 0xff, 0xbe,
    0x7c, 0xff, 0x3e, 0x7e, 0x7e, 0x7e, 0x3f, 0x0,
    0xfc, 0x1f, 0xc3, 0xf8, 0xf, 0xff, 0xf0, 0x7,
    0xff, 0xe0, 0x3, 0xff, 0xc0, 0x0, 0xfe, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc_temp[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 384, .box_w = 24, .box_h = 48, .ofs_x = 0, .ofs_y = -6}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps_temp[] =
{
    {
        .range_start = 62155, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache_temp;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc_temp = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap_temp,
    .glyph_dsc = glyph_dsc_temp,
    .cmaps = cmaps_temp,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache_temp
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t temp_icon = {
#else
lv_font_t temp_icon = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 48,          /*The maximum line height required by the font*/
    .base_line = 6,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -3,
    .underline_thickness = 2,
#endif
    .dsc = &font_dsc_temp,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if TEMP_ICON*/

