// spi_displays/draw.c
#include "draw.h"
#include "fonts.h"
#include "py/runtime.h"
#include "py/objfun.h"


static uint32_t decode_utf8(const char **ptr) {
    const uint8_t *s = (const uint8_t *)*ptr;
    uint32_t res = *s++;
    if (res >= 0x80) {
        if (res < 0xe0) {
            res = ((res & 0x1f) << 6) | (*s++ & 0x3f);
        } else if (res < 0xf0) {
            res = ((res & 0x0f) << 12) | ((s[0] & 0x3f) << 6) | (s[1] & 0x3f);
            s += 2;
        }
    }
    *ptr = (const char *)s;
    return res;
}
static void draw_char(mp_display_obj_t *display, int16_t x, int16_t y, uint16_t c, uint16_t color, const GFXfont *font) {
    if (c < font->first || c > font->last) return;
    uint16_t glyph_index = c - font->first;
    GFXglyph *glyph = &(font->glyph[glyph_index]);
    uint8_t *bitmap = font->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t w = glyph->width;
    uint8_t h = glyph->height;
    int8_t xo = glyph->xOffset;
    int8_t yo = glyph->yOffset;
    uint8_t bits = 0, bit = 0;
    uint16_t *buffer = (uint16_t *)display->buffer;
    for (uint8_t yy = 0; yy < h; yy++) {
        for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) {
                bits = bitmap[bo++];
            }
            if (bits & 0x80) {
                int16_t cur_x = x + xo + xx;
                int16_t cur_y = y + yo + yy;
                if (cur_x >= 0 && cur_x < display->width && cur_y >= 0 && cur_y < display->height) {
                    buffer[cur_y * display->width + cur_x] = (color << 8) | (color >> 8);
                }
            }
            bits <<= 1;
        }
    }
}
void draw_text(mp_display_obj_t *display, const char *str, int16_t x, int16_t y, uint16_t color, int font_size) {
    const GFXfont *f_latin = &Font_L_6;
    const GFXfont *f_cyrillic = &Font_C_6;
    switch (font_size) {
        case 8:  f_latin = &Font_L_8;  f_cyrillic = &Font_C_8;  break;
        case 12: f_latin = &Font_L_12; f_cyrillic = &Font_C_12; break;
        case 16: f_latin = &Font_L_16; f_cyrillic = &Font_C_16; break;
        case 20: f_latin = &Font_L_20; f_cyrillic = &Font_C_20; break;
        case 24: f_latin = &Font_L_24; f_cyrillic = &Font_C_24; break;
        default: f_latin = &Font_L_6;  f_cyrillic = &Font_C_6;  break;
    }
    int16_t top_offset = 0;
    if (f_latin->glyph) {
        top_offset = -(f_latin->glyph[65 - f_latin->first].yOffset);
    }
    int16_t baseline_y = y + top_offset;
    while (*str) {
        uint32_t code = decode_utf8(&str);
        const GFXfont *current_font = NULL;
        if (code >= 32 && code <= 127) {
            current_font = f_latin;
        } else if (code >= 1025 && code <= 1105) {
            current_font = f_cyrillic;
        }
        if (current_font) {
            draw_char(display, x, baseline_y, (uint16_t)code, color, current_font);
            x += current_font->glyph[code - current_font->first].xAdvance;
        } else {
            x += (font_size / 2 + 2);
        }
        if (x >= display->width) break;
    }
}

mp_obj_t draw_text_wrapper(size_t n_args, const mp_obj_t *args) {
    draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    const char *text = mp_obj_str_get_str(args[1]);
    int16_t x = mp_obj_get_int(args[2]);
    int16_t y = mp_obj_get_int(args[3]);

    // Обработка цвета (4-й аргумент)
    uint16_t color = 0xFFFF; // белый по умолчанию
    if (n_args > 4) {
        if (mp_obj_is_type(args[4], &mp_type_tuple)) {
            // Цвет как кортеж (r,g,b)
            mp_obj_t *items;
            size_t len;
            mp_obj_tuple_get(args[4], &len, &items);
            if (len != 3) {
                mp_raise_ValueError(MP_ERROR_TEXT("color tuple must have 3 elements (r,g,b)"));
            }
            int r = mp_obj_get_int(items[0]);
            int g = mp_obj_get_int(items[1]);
            int b = mp_obj_get_int(items[2]);
            uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            color = (rgb565 << 8) | (rgb565 >> 8);
        } else {
            uint16_t raw_color = (uint16_t)mp_obj_get_int(args[4]);
            color = (raw_color << 8) | (raw_color >> 8);
        }
    }
    int font_size = (n_args > 5) ? mp_obj_get_int(args[5]) : 0;
    draw_text(self->display, text, x, y, color, font_size);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_text_obj, 4, 6, draw_text_wrapper);

// ---------- Методы рисования ----------
void draw_fill(mp_display_obj_t *display, uint16_t color) {
    uint16_t *buf = (uint16_t *)display->buffer;
    size_t count = display->buffer_size / 2;
    for (size_t i = 0; i < count; i++) {
        buf[i] = color;
    }
}
//mp_obj_t draw_fill_wrapper(mp_obj_t self_in, mp_obj_t color_obj) {
//    draw_obj_t *self = MP_OBJ_TO_PTR(self_in);
//    uint16_t color;
//
//    if (mp_obj_is_type(color_obj, &mp_type_tuple)) {
//        mp_obj_t *items;
//        size_t len;
//        mp_obj_tuple_get(color_obj, &len, &items);
//        if (len != 3) {
//            mp_raise_ValueError(MP_ERROR_TEXT("color tuple must have 3 elements"));
//        }
//        int r = mp_obj_get_int(items[0]);
//        int g = mp_obj_get_int(items[1]);
//        int b = mp_obj_get_int(items[2]);
//        uint1or = (raw << 8) | (raw >> 8);
//    }6_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
//        color = (rgb565 << 8) | (rgb565 >> 8);
//    } else {
//        uint16_t raw = mp_obj_get_int(color_obj);
//        col
//
//    draw_fill(self->display, color);
//    return mp_const_none;
//}
//MP_DEFINE_CONST_FUN_OBJ_2(draw_fill_obj, draw_fill_wrapper);

mp_obj_t draw_fill_wrapper(mp_obj_t self_in, mp_obj_t color_obj) {
    draw_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint16_t color;

    // Отладка: выводим тип аргумента
    mp_printf(&mp_plat_print, "fill: arg type=%d\n", mp_obj_get_type(color_obj)->base.type);

    if (mp_obj_is_type(color_obj, &mp_type_tuple)) {
        mp_obj_t *items;
        size_t len;
        mp_obj_tuple_get(color_obj, &len, &items);
        if (len != 3) {
            mp_raise_ValueError(MP_ERROR_TEXT("color tuple must have 3 elements"));
        }
        int r = mp_obj_get_int(items[0]);
        int g = mp_obj_get_int(items[1]);
        int b = mp_obj_get_int(items[2]);
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        color = (rgb565 << 8) | (rgb565 >> 8);
    } else if (mp_obj_is_int(color_obj)) {
        uint16_t raw = mp_obj_get_int(color_obj);
        color = (raw << 8) | (raw >> 8);
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("color must be int or tuple (r,g,b)"));
    }

    draw_fill(self->display, color);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(draw_fill_obj, draw_fill_wrapper);


static const mp_rom_map_elem_t draw_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&draw_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&draw_text_obj) },
};
static MP_DEFINE_CONST_DICT(draw_locals_dict, draw_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_draw,
    MP_QSTR_Draw,
    MP_TYPE_FLAG_NONE,
    locals_dict, &draw_locals_dict
);

mp_obj_t draw_make_new(mp_display_obj_t *display) {
    draw_obj_t *self = mp_obj_malloc(draw_obj_t, &mp_type_draw);
    self->display = display;
    return MP_OBJ_FROM_PTR(self);
}