// spi_displays/draw.c
#include "draw.h"
#include "fonts.h"
#include "py/runtime.h"
#include "py/objfun.h"
#include <stdlib.h>


static uint16_t convert_color(mp_obj_t color_obj) {
    // Если это кортеж (r,g,b)
    if (mp_obj_is_type(color_obj, &mp_type_tuple)) {
        mp_obj_t *items;
        size_t len;
        mp_obj_tuple_get(color_obj, &len, &items);
        if (len != 3) {
            mp_raise_ValueError(MP_ERROR_TEXT("color tuple must have 3 elements (r,g,b)"));
        }
        int r = mp_obj_get_int(items[0]);
        int g = mp_obj_get_int(items[1]);
        int b = mp_obj_get_int(items[2]);
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            mp_raise_ValueError(MP_ERROR_TEXT("RGB values must be 0-255"));
        }
        // Конвертация RGB в RGB565 (little-endian)
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        // Конвертация в big-endian для буфера
        return (rgb565 << 8) | (rgb565 >> 8);
    }
    // Если это число (уже RGB565)
    if (mp_obj_is_int(color_obj)) {
        uint16_t raw = mp_obj_get_int(color_obj);
        // Конвертация в big-endian для буфера
        return (raw << 8) | (raw >> 8);
    }
    // Неподдерживаемый тип
    mp_raise_TypeError(MP_ERROR_TEXT("color must be int or tuple (r,g,b)"));
}

// text
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

// ---------- Методы рисования ----------
void draw_fill(mp_display_obj_t *display, uint16_t color) {
    uint16_t *buf = (uint16_t *)display->buffer;
    size_t count = display->buffer_size / 2;
    for (size_t i = 0; i < count; i++) {
        buf[i] = color;
    }
}

// Пиксель
void draw_pixel(mp_display_obj_t *display, int x, int y, uint16_t color) {
    if (x >= 0 && x < display->width && y >= 0 && y < display->height) {
        uint16_t *buf = (uint16_t *)display->buffer;
        buf[y * display->width + x] = color;
    }
}

// Линия (алгоритм Брезенхема)
void draw_line(mp_display_obj_t *display, int x1, int y1, int x2, int y2, uint16_t color) {
    int dx = abs(x2 - x1);
    int dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        draw_pixel(display, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// Несколько линий
void draw_lines(mp_display_obj_t *display, int *points, int num_points, uint16_t color, bool closed) {
    for (int i = 0; i < num_points - 1; i++) {
        draw_line(display, points[i*2], points[i*2+1],
                  points[(i+1)*2], points[(i+1)*2+1], color);
    }
    if (closed && num_points > 2) {
        draw_line(display, points[(num_points-1)*2], points[(num_points-1)*2+1],
                  points[0], points[1], color);
    }
}

// Прямоугольник
void draw_rect(mp_display_obj_t *display, int x, int y, int w, int h, uint16_t color, bool fill) {
    if (fill) {
        for (int i = 0; i < h; i++) {
            draw_line(display, x, y + i, x + w - 1, y + i, color);
        }
    } else {
        draw_line(display, x, y, x + w - 1, y, color);
        draw_line(display, x, y + h - 1, x + w - 1, y + h - 1, color);
        draw_line(display, x, y, x, y + h - 1, color);
        draw_line(display, x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

// Круг (алгоритм Брезенхема)
void draw_circle(mp_display_obj_t *display, int cx, int cy, int r, uint16_t color, bool fill) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    if (fill) {
        // Залитый круг - рисуем горизонтальные линии
        while (y >= x) {
            draw_line(display, cx - x, cy + y, cx + x, cy + y, color);
            draw_line(display, cx - x, cy - y, cx + x, cy - y, color);
            draw_line(display, cx - y, cy + x, cx + y, cy + x, color);
            draw_line(display, cx - y, cy - x, cx + y, cy - x, color);
            x++;
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
        }
    } else {
        // Контур круга
        while (x <= y) {
            draw_pixel(display, cx + x, cy + y, color);
            draw_pixel(display, cx - x, cy + y, color);
            draw_pixel(display, cx + x, cy - y, color);
            draw_pixel(display, cx - x, cy - y, color);
            draw_pixel(display, cx + y, cy + x, color);
            draw_pixel(display, cx - y, cy + x, color);
            draw_pixel(display, cx + y, cy - x, color);
            draw_pixel(display, cx - y, cy - x, color);
            x++;
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
        }
    }
}

// Эллипс
// void draw_ellipse(mp_display_obj_t *display, int cx, int cy, int rx, int ry, uint16_t color, bool fill) {
//     int x = 0;
//     int y = ry;
//     int rx2 = rx * rx;
//     int ry2 = ry * ry;
//     int err = ry2 - rx2 * ry;
//
//     if (fill) {
//         while (y >= 0) {
//             draw_line(display, cx - x, cy + y, cx + x, cy + y, color);
//             if (y != 0) {
//                 draw_line(display, cx - x, cy - y, cx + x, cy - y, color);
//             }
//             if (err < 0) {
//                 x++;
//                 err += 2 * ry2 * x + ry2;
//             } else {
//                 x++;
//                 y--;
//                 err += 2 * ry2 * x - 2 * rx2 * y + ry2;
//             }
//         }
//     } else {
//         // Контур эллипса - сложнее, пока можно через пиксели
//         while (y >= 0) {
//             draw_pixel(display, cx + x, cy + y, color);
//             draw_pixel(display, cx - x, cy + y, color);
//             draw_pixel(display, cx + x, cy - y, color);
//             draw_pixel(display, cx - x, cy - y, color);
//             if (err < 0) {
//                 x++;
//                 err += 2 * ry2 * x + ry2;
//             } else {
//                 x++;
//                 y--;
//                 err += 2 * ry2 * x - 2 * rx2 * y + ry2;
//             }
//         }
//     }
// }
// --------------------------------------- wrappers -----------------------------------------------
// fill
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


mp_obj_t draw_line_wrapper(size_t n_args, const mp_obj_t *args) {
    draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t color = convert_color(args[1]);
    // Первая точка (x1, y1)
    mp_obj_t *p1_items;
    size_t p1_len;
    mp_obj_tuple_get(args[2], &p1_len, &p1_items);
    if (p1_len != 2) mp_raise_ValueError(MP_ERROR_TEXT("point must have x,y"));
    // Вторая точка (x2, y2)
    mp_obj_t *p2_items;
    size_t p2_len;
    mp_obj_tuple_get(args[3], &p2_len, &p2_items);
    if (p2_len != 2) mp_raise_ValueError(MP_ERROR_TEXT("point must have x,y"));
    int x1 = mp_obj_get_int(p1_items[0]);
    int y1 = mp_obj_get_int(p1_items[1]);
    int x2 = mp_obj_get_int(p2_items[0]);
    int y2 = mp_obj_get_int(p2_items[1]);
    draw_line(self->display, x1, y1, x2, y2, color);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_line_obj, 4, 4, draw_line_wrapper);

// lines
mp_obj_t draw_lines_wrapper(size_t n_args, const mp_obj_t *args) {
    draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t color = convert_color(args[1]);

    size_t points_len;
    mp_obj_t *points_items;
    mp_obj_get_array(args[2], &points_len, &points_items);  // работает с list и tuple

    int *coords = m_malloc(points_len * 2 * sizeof(int));
    for (size_t i = 0; i < points_len; i++) {
        mp_obj_t *point_items;
        size_t point_len;
        mp_obj_get_array(points_items[i], &point_len, &point_items);  // тоже
        if (point_len != 2) mp_raise_ValueError(MP_ERROR_TEXT("each point must have x,y"));
        coords[i*2] = mp_obj_get_int(point_items[0]);
        coords[i*2+1] = mp_obj_get_int(point_items[1]);
    }

    bool closed = mp_obj_is_true(args[3]);
    draw_lines(self->display, coords, points_len, color, closed);

    m_free(coords);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_lines_obj, 4, 4, draw_lines_wrapper);

// rect
mp_obj_t draw_rect_wrapper(size_t n_args, const mp_obj_t *args) {
    draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t color = convert_color(args[1]);
    mp_obj_t *rect_items;
    size_t rect_len;
    mp_obj_tuple_get(args[2], &rect_len, &rect_items);
    if (rect_len != 4) mp_raise_ValueError(MP_ERROR_TEXT("rect must have x,y,w,h"));
    int x = mp_obj_get_int(rect_items[0]);
    int y = mp_obj_get_int(rect_items[1]);
    int w = mp_obj_get_int(rect_items[2]);
    int h = mp_obj_get_int(rect_items[3]);
    bool fill = mp_obj_is_true(args[3]);
    draw_rect(self->display, x, y, w, h, color, fill);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_rect_obj, 4, 4, draw_rect_wrapper);

// circle
mp_obj_t draw_circle_wrapper(size_t n_args, const mp_obj_t *args) {
    draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t color = convert_color(args[1]);
    mp_obj_t *center_items;
    size_t center_len;
    mp_obj_tuple_get(args[2], &center_len, &center_items);
    if (center_len != 2) mp_raise_ValueError(MP_ERROR_TEXT("center must have x,y"));
    int cx = mp_obj_get_int(center_items[0]);
    int cy = mp_obj_get_int(center_items[1]);
    int r = mp_obj_get_int(args[3]);
    bool fill = mp_obj_is_true(args[4]);
    draw_circle(self->display, cx, cy, r, color, fill);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_circle_obj, 5, 5, draw_circle_wrapper);

// ellipse
// mp_obj_t draw_ellipse_wrapper(size_t n_args, const mp_obj_t *args) {
//     draw_obj_t *self = MP_OBJ_TO_PTR(args[0]);
//     uint16_t color = convert_color(args[1]);
//     mp_obj_t *center_items;
//     size_t center_len;
//     mp_obj_tuple_get(args[2], &center_len, &center_items);
//     if (center_len != 2) mp_raise_ValueError(MP_ERROR_TEXT("center must have x,y"));
//     int cx = mp_obj_get_int(center_items[0]);
//     int cy = mp_obj_get_int(center_items[1]);
//     int rx = mp_obj_get_int(args[3]);
//     int ry = mp_obj_get_int(args[4]);
//     bool fill = mp_obj_is_true(args[5]);
//     draw_ellipse(self->display, cx, cy, rx, ry, color, fill);
//     return mp_const_none;
// }
// MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(draw_ellipse_obj, 6, 6, draw_ellipse_wrapper);

// text
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

//  ------------------------- reg -----------------------------
static const mp_rom_map_elem_t draw_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&draw_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&draw_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&draw_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_lines), MP_ROM_PTR(&draw_lines_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&draw_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_circle), MP_ROM_PTR(&draw_circle_obj) },
    // { MP_ROM_QSTR(MP_QSTR_ellipse), MP_ROM_PTR(&draw_ellipse_obj) },
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