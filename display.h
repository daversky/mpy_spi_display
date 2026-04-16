#ifndef MICROPY_INCLUDED_SPI_DISPLAYS_DISPLAY_H
#define MICROPY_INCLUDED_SPI_DISPLAYS_DISPLAY_H

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"

typedef struct _mp_display_obj_t {
    mp_obj_base_t base;
    mp_obj_t spi;
    mp_hal_pin_obj_t dc;
    mp_hal_pin_obj_t cs;
    mp_hal_pin_obj_t rst;
    mp_hal_pin_obj_t bl;
    bool backlight_active_high;
    bool bgr;
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
    void (*set_rotation_func)(struct _mp_display_obj_t*, uint8_t);
    uint8_t *buffer;
    size_t buffer_size;
    bool buffer_allocated;
    mp_obj_t buffer_obj;
} mp_display_obj_t;

extern const mp_obj_type_t mp_type_display;

void display_send_cmd(mp_display_obj_t *self, uint8_t cmd);
void display_send_cmd_data(mp_display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t data_len);
void display_write_data(mp_display_obj_t *self, const uint8_t *data, size_t len);
void display_reset_hw(mp_display_obj_t *self);
void display_set_rotation_default(mp_display_obj_t *self, uint8_t rot);
void display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);
size_t display_get_pixel_offset(mp_display_obj_t *self, uint16_t x, uint16_t y);
mp_obj_t display_make_new_base(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args);

#endif