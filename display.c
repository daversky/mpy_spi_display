// spi_displays/display.c
#include "display.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "py/objtype.h"
#include "extmod/virtpin.h"
#ifdef PICO_BOARD
    #include "hardware/spi.h"
#endif
#include "extmod/modmachine.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "draw.h"
#include <stdio.h>

#define CS_LOW(self)  if ((self)->cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write((self)->cs, 0)
#define CS_HIGH(self) if ((self)->cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_write((self)->cs, 1)
#define DC_LOW(self)  mp_hal_pin_write((self)->dc, 0)
#define DC_HIGH(self) mp_hal_pin_write((self)->dc, 1)

void spi_write(mp_obj_t spi_obj, const uint8_t *data, size_t len) {
    if (len == 0) return;
    mp_obj_base_t *s = (mp_obj_base_t *)MP_OBJ_TO_PTR(spi_obj);
    mp_machine_spi_p_t *spi_p = (mp_machine_spi_p_t *)MP_OBJ_TYPE_GET_SLOT(s->type, protocol);
    if (spi_p && spi_p->transfer) {
        spi_p->transfer(s, len, data, NULL);
    }
}

void display_write_cmd_data(mp_display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t data_len) {
    if (self->debug >= 2) {
        mp_printf(&mp_plat_print, "CMD: 0x%02X", cmd);
        if (data_len > 0) {
            mp_printf(&mp_plat_print, " DATA:");
            for (size_t i = 0; i < data_len && i < 8; i++) {
                mp_printf(&mp_plat_print, " %02X", data[i]);
            }
            if (data_len > 8) mp_printf(&mp_plat_print, "...");
        }
        mp_printf(&mp_plat_print, "\n");
    }
    CS_LOW(self);
    DC_LOW(self);
    spi_write(self->spi, &cmd, 1);
    if (data_len > 0) {
        DC_HIGH(self);
        spi_write(self->spi, data, data_len);
    }
    CS_HIGH(self);
}

void display_reset_hw(mp_display_obj_t *self) {
    if (self->rst != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_write(self->rst, 0);
        mp_hal_delay_ms(10);
        mp_hal_pin_write(self->rst, 1);
        mp_hal_delay_ms(120);
    }
}

void display_set_window(mp_display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x_start = x + self->x_offset;
    uint16_t y_start = y + self->y_offset;
    uint16_t x_end = x_start + w - 1;
    uint16_t y_end = y_start + h - 1;
    uint8_t x_buf[4] = {x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF};
    uint8_t y_buf[4] = {y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF};
    display_write_cmd_data(self, 0x2A, x_buf, 4);
    display_write_cmd_data(self, 0x2B, y_buf, 4);
    display_write_cmd_data(self, 0x2C, NULL, 0);
}

mp_obj_t display_show(mp_obj_t self_in) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);

    display_set_window(self, 0, 0, self->width, self->height);

    uint16_t *buf = (uint16_t *)self->buffer;
    size_t count = self->buffer_size / 2;
    for (size_t i = 0; i < count; i++) {
        uint16_t v = buf[i];
        buf[i] = (v << 8) | (v >> 8);
    }

    DC_HIGH(self);
    CS_LOW(self);
    spi_write(self->spi, (const uint8_t *)self->buffer, self->buffer_size);
    CS_HIGH(self);

    for (size_t i = 0; i < count; i++) {
        uint16_t v = buf[i];
        buf[i] = (v << 8) | (v >> 8);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(display_show_obj, display_show);

// ---------- Base constructor ----------
mp_obj_t display_make_new_base(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_spi,
        ARG_width,
        ARG_height,
        ARG_rotate,
        ARG_x_offset,
        ARG_y_offset,
        ARG_bgr,
        ARG_inverse,
        ARG_dc,
        ARG_cs,
        ARG_rst,
        ARG_bl,
        ARG_backlight_active_high,
        ARG_debug,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi,                   MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width,                 MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height,                MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_rotate,                MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_x_offset,              MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_y_offset,              MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_bgr,                   MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_inverse,               MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_dc,                    MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_cs,                    MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_rst,                   MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_bl,                    MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_debug,                 MP_ARG_INT,  {.u_int = 0} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    uint16_t user_width = args[ARG_width].u_int;
    uint16_t user_height = args[ARG_height].u_int;
    uint16_t user_rotate_deg = args[ARG_rotate].u_int;
    uint16_t user_x_offset = args[ARG_x_offset].u_int;
    uint16_t user_y_offset = args[ARG_y_offset].u_int;
    uint8_t debug = args[ARG_debug].u_int;
    uint8_t rotate;
    switch (user_rotate_deg) {
        case 0: rotate = 0; break;
        case 90: rotate = 1; break;
        case 180: rotate = 2; break;
        case 270: rotate = 3; break;
        default: mp_raise_ValueError(MP_ERROR_TEXT("rotate must be 0, 90, 180, or 270"));
    }
    uint16_t width, height, x_offset, y_offset;
    if (rotate & 1) {
        width = user_height;
        height = user_width;
        x_offset = user_y_offset;
        y_offset = user_x_offset;
    } else {
        width = user_width;
        height = user_height;
        x_offset = user_x_offset;
        y_offset = user_y_offset;
    }

    mp_display_obj_t *self = mp_obj_malloc(mp_display_obj_t, type);

    self->spi = args[ARG_spi].u_obj;
    self->width = width;
    self->height = height;
    self->rotate = rotate;
    self->x_offset = x_offset;
    self->y_offset = y_offset;
    self->debug = debug;
    self->dc = mp_hal_get_pin_obj(args[ARG_dc].u_obj);
    self->cs = (args[ARG_cs].u_obj != MP_OBJ_NULL) ? mp_hal_get_pin_obj(args[ARG_cs].u_obj) : (mp_hal_pin_obj_t)-1;
    self->rst = (args[ARG_rst].u_obj != MP_OBJ_NULL) ? mp_hal_get_pin_obj(args[ARG_rst].u_obj) : (mp_hal_pin_obj_t)-1;
    self->bl = (args[ARG_bl].u_obj != MP_OBJ_NULL) ? mp_hal_get_pin_obj(args[ARG_bl].u_obj) : (mp_hal_pin_obj_t)-1;
    self->bgr = args[ARG_bgr].u_bool;
    self->backlight_active_high = args[ARG_backlight_active_high].u_bool;
    self->inverse = args[ARG_inverse].u_bool;
    //
    mp_hal_pin_output(self->dc);
    if (self->cs != (mp_hal_pin_obj_t)-1) mp_hal_pin_output(self->cs);
    if (self->rst != (mp_hal_pin_obj_t)-1) mp_hal_pin_output(self->rst);
    if (self->bl != (mp_hal_pin_obj_t)-1) {
        mp_hal_pin_output(self->bl);
        mp_hal_pin_write(self->bl, self->backlight_active_high);
    }
    CS_HIGH(self);
    self->buffer_size = (size_t)width * height * 2;
    self->buffer = m_malloc(self->buffer_size);
    if (self->buffer == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("failed to allocate display buffer"));
    }
    self->buffer_allocated = true;
    self->buffer_obj = mp_obj_new_bytearray_by_ref(self->buffer_size, self->buffer);
    self->draw = draw_make_new(self);
    if (self->debug >= 2) {
        mp_printf(&mp_plat_print, "Init parameters:\n");
        mp_printf(&mp_plat_print, "  - width:                 %u\n", (unsigned int)width);
        mp_printf(&mp_plat_print, "  - height:                %u\n", (unsigned int)height);
        mp_printf(&mp_plat_print, "  - rotate:                %u\n", (unsigned int)rotate);
        mp_printf(&mp_plat_print, "  - x_offset:              %d\n", (int)x_offset);
        mp_printf(&mp_plat_print, "  - y_offset:              %d\n", (int)y_offset);
        mp_printf(&mp_plat_print, "  - bgr:                   %s\n", self->bgr ? "True" : "False");
        mp_printf(&mp_plat_print, "  - inverse:               %s\n", self->inverse ? "True" : "False");
        mp_printf(&mp_plat_print, "  - backlight_active_high: %s\n", self->backlight_active_high ? "True" : "False");
    }
    return MP_OBJ_FROM_PTR(self);
}

void display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_display_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_width) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->height);
        } else if (attr == MP_QSTR_buffer) {
            dest[0] = self->buffer_obj;
        } else if (attr == MP_QSTR_draw) {
            dest[0] = self->draw;  // <-- ДОБАВИТЬ ЭТО
        } else {
            const mp_obj_type_t *type = mp_obj_get_type(self_in);
            mp_obj_dict_t *locals_dict = MP_OBJ_TYPE_GET_SLOT(type, locals_dict);
            if (locals_dict != NULL) {
                mp_map_elem_t *elem = mp_map_lookup(&locals_dict->map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
                if (elem != NULL) {
                    dest[0] = elem->value;
                    dest[1] = self_in;  // bound method
                    return;
                }
            }
            const mp_obj_type_t *parent = MP_OBJ_TYPE_GET_SLOT(type, parent);
            if (parent != NULL && parent != type) {
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
        // Запись атрибута - пока не поддерживаем
    }
}

static const mp_rom_map_elem_t display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&display_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&mp_type_draw) },
};
MP_DEFINE_CONST_DICT(display_locals_dict, display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_display_core,
    MP_QSTR_DisplayCore,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new_base,
    locals_dict, &display_locals_dict,
    attr, display_attr
);

mp_obj_t display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    return display_make_new_base(&mp_type_display_core, n_args, n_kw, all_args);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_display,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, display_make_new,
    locals_dict, &display_locals_dict,
    attr, display_attr,
    parent, &mp_type_display_core
);