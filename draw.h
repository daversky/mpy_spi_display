// spi_displays/draw.h
#ifndef MICROPY_INCLUDED_DRAW_H
#define MICROPY_INCLUDED_DRAW_H

#include "py/obj.h"
#include "display.h"

// ---------- Структура draw объекта ----------
typedef struct _draw_obj_t
{
    mp_obj_base_t base;
    mp_display_obj_t* display;
} draw_obj_t;

// Инициализация
mp_obj_t draw_make_new(mp_display_obj_t* display);

// Базовые методы
void draw_fill(mp_display_obj_t* display, uint16_t color);
void draw_text(mp_display_obj_t* display, const char* text, int16_t x, int16_t y, uint16_t color, int font_size);

// Примитивы рисования
void draw_pixel(mp_display_obj_t *display, int x, int y, uint16_t color);
void draw_line(mp_display_obj_t *display, int x1, int y1, int x2, int y2, uint16_t color);
void draw_lines(mp_display_obj_t *display, int *points, int num_points, uint16_t color, bool closed);
void draw_rect(mp_display_obj_t *display, int x, int y, int w, int h, uint16_t color, bool fill);
void draw_circle(mp_display_obj_t *display, int cx, int cy, int r, uint16_t color, bool fill);
void draw_ellipse(mp_display_obj_t *display, int cx, int cy, int rx, int ry, uint16_t color, bool fill);


// Внешний тип
extern const mp_obj_type_t mp_type_draw;

#endif
