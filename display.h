#ifndef MICROPY_INCLUDED_DISPLAY_H
#define MICROPY_INCLUDED_DISPLAY_H

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"

typedef struct _mp_display_obj_t {
    mp_obj_base_t base;
    mp_obj_t spi;
    mp_obj_t buffer_obj;
    uint16_t width;
    uint16_t height;
    uint8_t rotate;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t debug;
    mp_hal_pin_obj_t dc;
    mp_hal_pin_obj_t cs;
    mp_hal_pin_obj_t rst;
    mp_hal_pin_obj_t bl;
    bool bgr;
    bool inverse;
    bool backlight_active_high;
    bool buffer_allocated;
    void *buffer;
    size_t buffer_size;
    mp_obj_t draw;
} mp_display_obj_t;

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint16_t delay; // delay в миллисекундах
    uint8_t data[16];
} display_init_cmd_t;

extern const mp_obj_type_t mp_type_display;
extern const mp_obj_type_t mp_type_display_core;
extern const mp_obj_type_t mp_type_draw;

// Функции для драйверов
void display_write_cmd_data(mp_display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t data_len);
void display_set_window(mp_display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void display_reset_hw(mp_display_obj_t *self);
void display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);
// Типы

mp_obj_t display_make_new_base(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args);

#endif