// spi_displays/draw.h
#ifndef MICROPY_INCLUDED_DRAW_H
#define MICROPY_INCLUDED_DRAW_H

#include "py/obj.h"
#include "display.h"

// ---------- Структура draw объекта ----------
typedef struct _draw_obj_t {
    mp_obj_base_t base;
    mp_display_obj_t *display;
} draw_obj_t;

// Инициализация
mp_obj_t draw_make_new(mp_display_obj_t *display);

// Базовые методы
void draw_fill(mp_display_obj_t *display, uint16_t color);
void draw_text(mp_display_obj_t *display, const char *text, int16_t x, int16_t y, uint16_t color, int font_size);

// Внешний тип
extern const mp_obj_type_t mp_type_draw;

#endif