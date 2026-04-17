// spi_displays/display.c
#include "display.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "py/objtype.h"
#include "extmod/virtpin.h"
#if MICROPY_PY_MACHINE_RP2
#include "hardware/spi.h"
#endif
#include "extmod/modmachine.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "fonts.h"
#include <stdio.h>

#define CS_LOW(self)  if ((self)->cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write((self)->cs, 0)
#define CS_HIGH(self) if ((self)->cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write((self)->cs, 1)
#define DC_LOW(self)  mp_hal_pin_write((self)->dc, 0)
#define DC_HIGH(self) mp_hal_pin_write((self)->dc, 1)

static void display_cs_low(mp_display_obj_t *self) { CS_LOW(self); }
static void display_cs_high(mp_display_obj_t *self) { CS_HIGH(self); }

void spi_write(mp_obj_t spi_obj, const uint8_t *data, size_t len) {
    if (len == 0) return;
    mp_obj_base_t *s = (mp_obj_base_t *)MP_OBJ_TO_PTR(spi_obj);
    mp_machine_spi_p_t *spi_p = (mp_machine_spi_p_t *)MP_OBJ_TYPE_GET_SLOT(s->type, protocol);
    if (spi_p && spi_p->transfer) {
        spi_p->transfer(s, len, data, NULL);
    }
}

void display_send_cmd(mp_display_obj_t *self, uint8_t cmd) {
    display_cs_low(self);
    DC_LOW(self);
    spi_write(self->spi, &cmd, 1);
    display_cs_high(self);
}

void display_write_data(mp_display_obj_t *self, const uint8_t *data, size_t len) {
    display_cs_low(self);
    DC_HIGH(self);
    spi_write(self->spi, data, len);
    display_cs_high(self);
}

void display_send_cmd_data(mp_display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t data_len) {
    mp_printf(&mp_plat_print, "CMD: 0x%02X", cmd);
    if (data_len > 0) {
        mp_printf(&mp_plat_print, " DATA[%u]:", (unsigned int)data_len);
        for (size_t i = 0; i < data_len && i < 8; i++) { // Выведем первые 8 байт
            mp_printf(&mp_plat_print, " %02X", data[i]);
        }
        if (data_len > 8) mp_printf(&mp_plat_print, "...");
    }
    mp_printf(&mp_plat_print, "\n");
    display_send_cmd(self, cmd);
    if (data_len > 0) {
        display_write_data(self, data, data_len);
    }
}

void display_reset_hw(mp_display_obj_t *self) {
    if (self->rst != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_write(self->rst, 0);
        mp_hal_delay_ms(50);
        mp_hal_pin_write(self->rst, 1);
        mp_hal_delay_ms(50);
    }
}

void display_set_window(mp_display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint8_t x_buf[4] = {x >> 8, x & 0xFF, (x + w - 1) >> 8, (x + w - 1) & 0xFF};
    uint8_t y_buf[4] = {y >> 8, y & 0xFF, (y + h - 1) >> 8, (y + h - 1) & 0xFF};
    display_send_cmd_data(self, 0x2A, x_buf, 4);
    display_send_cmd_data(self, 0x2B, y_buf, 4);
    display_send_cmd(self, 0x2C);
}

size_t display_get_pixel_offset(mp_display_obj_t *self, uint16_t x, uint16_t y) {
    uint16_t w = self->width, h = self->height, src_x, src_y;
    switch (self->rotation) {
        case 0: src_x = x; src_y = y; break;
        case 1: src_x = h - 1 - y; src_y = x; break;
        case 2: src_x = w - 1 - x; src_y = h - 1 - y; break;
        case 3: src_x = y; src_y = w - 1 - x; break;
        default: src_x = x; src_y = y;
    }
    return ((size_t)src_y * w + src_x) * 2;
}

void display_set_rotation_default(mp_display_obj_t *self, uint8_t rot) {
    self->rotation = rot;
}

// ---------- Base constructor ----------
mp_obj_t display_make_new_base(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_spi, ARG_width, ARG_height, ARG_dc, ARG_cs, ARG_rst, ARG_bl, ARG_bgr, ARG_backlight_active_high, ARG_rotation, ARG_buffer };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_cs, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_rst, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_bl, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_bgr, MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_rotation, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_display_obj_t *self = mp_obj_malloc(mp_display_obj_t, type);
    self->spi = args[ARG_spi].u_obj;
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;
    self->dc = mp_hal_get_pin_obj(args[ARG_dc].u_obj);
    self->cs = (args[ARG_cs].u_obj != MP_OBJ_NULL) ? mp_hal_get_pin_obj(args[ARG_cs].u_obj) : (mp_hal_pin_obj_t)-1;
    self->rst = (args[ARG_rst].u_obj != MP_OBJ_NULL) ? mp_hal_get_pin_obj(args[ARG_rst].u_obj) : (mp_hal_pin_obj_t)-1;
    self->bl = (args[ARG_bl].u_obj != MP_OBJ_NULL) ? mp_hal_get_pin_obj(args[ARG_bl].u_obj) : (mp_hal_pin_obj_t)-1;
    self->bgr = args[ARG_bgr].u_bool;
    self->backlight_active_high = args[ARG_backlight_active_high].u_bool;
    self->rotation = args[ARG_rotation].u_int;
    self->set_rotation_func = display_set_rotation_default;

    mp_hal_pin_output(self->dc);
    if (self->cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_output(self->cs);
    if (self->rst != (mp_hal_pin_obj_t)-1) mp_hal_pin_output(self->rst);
    if (self->bl != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_output(self->bl);
        mp_hal_pin_write(self->bl, self->backlight_active_high);
    }
    display_cs_high(self);

    self->buffer_size = (size_t)self->width * self->height * 2;
    if (args[ARG_buffer].u_obj != MP_OBJ_NULL) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_buffer].u_obj, &bufinfo, MP_BUFFER_RW);
        if (bufinfo.len < self->buffer_size) mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
        self->buffer = bufinfo.buf;
        self->buffer_allocated = false;
        self->buffer_obj = args[ARG_buffer].u_obj;
    } else {
        self->buffer = m_malloc(self->buffer_size);
        if (self->buffer == NULL) mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("failed to allocate display buffer"));
        self->buffer_allocated = true;
        self->buffer_obj = mp_obj_new_bytearray_by_ref(self->buffer_size, self->buffer);
    }

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    return display_make_new_base(type, n_args, n_kw, all_args);
}

static mp_obj_t display_del(mp_obj_t self_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->buffer_allocated && self->buffer != NULL) {
        m_free(self->buffer);
        self->buffer = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(display_del_obj, display_del);

static mp_obj_t display_cmd(mp_obj_t self_in, mp_obj_t cmd_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    display_send_cmd(self, mp_obj_get_int(cmd_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_cmd_obj, display_cmd);

static mp_obj_t display_cmd_data(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t data_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t cmd = mp_obj_get_int(cmd_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);
    display_send_cmd_data(self, cmd, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(display_cmd_data_obj, display_cmd_data);


static mp_obj_t display_show(mp_obj_t self_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    display_set_window(self, 0, 0, self->width, self->height);
    uint16_t *buf = (uint16_t *)self->buffer;
    size_t count = self->buffer_size / 2;
    for (size_t i = 0; i < count; i++) {
        uint16_t v = buf[i];
        buf[i] = (v << 8) | (v >> 8);
    }
    display_cs_low(self);
    DC_HIGH(self);
    spi_write(self->spi, self->buffer, self->buffer_size);
    display_cs_high(self);
    for (size_t i = 0; i < count; i++) {
        uint16_t v = buf[i];
        buf[i] = (v << 8) | (v >> 8);
    }
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(display_show_obj, display_show);

static mp_obj_t display_update_rect(size_t n_args, const mp_obj_t *args) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t x = mp_obj_get_int(args[1]), y = mp_obj_get_int(args[2]);
    mp_int_t w = mp_obj_get_int(args[3]), h = mp_obj_get_int(args[4]);

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > self->width) w = self->width - x;
    if (y + h > self->height) h = self->height - y;
    if (w <= 0 || h <= 0) return mp_const_none;

    display_set_window(self, x, y, w, h);
    uint16_t stride = self->width * 2;
    uint8_t *src = self->buffer + (y * self->width + x) * 2;
    for (int row = 0; row < h; row++) {
        display_write_data(self, src, w * 2);
        src += stride;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_update_rect_obj, 5, 5, display_update_rect);

static mp_obj_t display_set_backlight(mp_obj_t self_in, mp_obj_t percent_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->bl == (mp_hal_pin_obj_t)-1) return mp_const_none;
    int percent = mp_obj_get_int(percent_in);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    bool active = (percent > 0) ? self->backlight_active_high : !self->backlight_active_high;
    mp_hal_pin_write(self->bl, active ? 1 : 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_set_backlight_obj, display_set_backlight);

static mp_obj_t display_reset(mp_obj_t self_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    display_reset_hw(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(display_reset_obj, display_reset);

void display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_width) {
            dest[0] = mp_obj_new_int(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = mp_obj_new_int(self->height);
        } else if (attr == MP_QSTR_buffer) {
            dest[0] = self->buffer_obj;
        } else if (attr == MP_QSTR_rotation) {
            dest[0] = mp_obj_new_int(self->rotation);
        } else {
            const mp_obj_type_t *type = mp_obj_get_type(self_in);
            mp_obj_dict_t *locals_dict = MP_OBJ_TYPE_GET_SLOT(type, locals_dict);
            if (locals_dict != NULL) {
                mp_map_elem_t *elem = mp_map_lookup(&locals_dict->map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
                if (elem != NULL) {
                    dest[0] = elem->value;
                    dest[1] = self_in;
                    return;
                }
            }
            const mp_obj_type_t *parent = MP_OBJ_TYPE_GET_SLOT(type, parent);
            if (parent != NULL) {
                mp_obj_dict_t *p_locals = MP_OBJ_TYPE_GET_SLOT(parent, locals_dict);
                if (p_locals != NULL) {
                    mp_map_elem_t *p_elem = mp_map_lookup(&p_locals->map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
                    if (p_elem != NULL) {
                        dest[0] = p_elem->value;
                        dest[1] = self_in;
                        return;
                    }
                }
            }
        }
    } else {
        if (attr == MP_QSTR_rotation && dest[1] != MP_OBJ_NULL) {
            if (self->set_rotation_func != NULL) {
                self->set_rotation_func(self, (uint8_t)mp_obj_get_int(dest[1]));
                dest[0] = MP_OBJ_NULL;
            }
        }
    }
}

static mp_obj_t display_text(size_t n_args, const mp_obj_t *args) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    const char *str = mp_obj_str_get_str(args[1]);
    int16_t x = mp_obj_get_int(args[2]);
    int16_t y = mp_obj_get_int(args[3]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[4]);
    int font_id = (n_args > 5) ? mp_obj_get_int(args[5]) : 0;
    fonts_draw_text(self, str, x, y, color, font_id);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_text_obj, 5, 6, display_text);

static const mp_rom_map_elem_t display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&display_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_cmd), MP_ROM_PTR(&display_cmd_obj) },
    { MP_ROM_QSTR(MP_QSTR_cmd_data), MP_ROM_PTR(&display_cmd_data_obj) },
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&display_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&display_update_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&display_set_backlight_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&display_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_text ), MP_ROM_PTR(&display_text_obj) },
};
static MP_DEFINE_CONST_DICT(display_locals_dict, display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_display,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new,
    locals_dict, &display_locals_dict,
    attr, display_attr
);