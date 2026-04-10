#include "py/runtime.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "extmod/machine_spi.h"
#include "display.h"
#include "st7735_init.h"
#include "st7789_init.h"

// Python wrapper for ST7735
typedef struct {
    mp_obj_base_t base;
    display_obj_t *display;
    mp_obj_t framebuffer_obj;
    mp_obj_t framebuf_obj;
    uint16_t width;
    uint16_t height;
} st7735_obj_t;

// Helper function to import and get FrameBuffer class
static mp_obj_t get_framebuf_class(void) {
    mp_obj_t framebuf_module = mp_import_name(MP_QSTR_framebuf, mp_const_none, 0);
    return mp_load_attr(framebuf_module, MP_QSTR_FrameBuffer);
}

// Helper function to get RGB565 constant
static mp_obj_t get_rgb565_format(void) {
    mp_obj_t framebuf_module = mp_import_name(MP_QSTR_framebuf, mp_const_none, 0);
    return mp_load_attr(framebuf_module, MP_QSTR_RGB565);
}

static mp_obj_t st7735_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_spi, ARG_width, ARG_height, ARG_buffer, ARG_dc, ARG_cs, ARG_rst, ARG_bl,
        ARG_backlight_active_high, ARG_use_dma, ARG_dma_threshold
    };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_cs, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_rst, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_bl, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_use_dma, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_dma_threshold, MP_ARG_INT, {.u_int = 64} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Create ST7735 object
    st7735_obj_t *self = m_new_obj(st7735_obj_t);
    self->base.type = type;
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;

    // Create or use provided buffer
    size_t buffer_size = self->width * self->height * 2;
    if (args[ARG_buffer].u_obj != mp_const_none) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_buffer].u_obj, &bufinfo, MP_BUFFER_RW);
        if (bufinfo.len < buffer_size) {
            mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
        }
        self->framebuffer_obj = args[ARG_buffer].u_obj;
    } else {
        self->framebuffer_obj = mp_obj_new_bytearray(buffer_size, NULL);
    }

    // Create Display object
    mp_obj_t display_args[10] = {
        args[ARG_spi].u_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        mp_obj_new_int(args[ARG_dc].u_int),
        mp_obj_new_int(args[ARG_cs].u_int),
        mp_obj_new_int(args[ARG_rst].u_int),
        mp_obj_new_int(args[ARG_bl].u_int),
        mp_obj_new_bool(args[ARG_backlight_active_high].u_bool),
        mp_obj_new_bool(args[ARG_use_dma].u_bool),
        mp_obj_new_int(args[ARG_dma_threshold].u_int),
    };

    mp_obj_t display_obj = display_make_new(&display_type,
        MP_ARRAY_SIZE(display_args), 0, display_args);
    self->display = MP_OBJ_TO_PTR(display_obj);

    // Send initialization sequence
    for (size_t i = 0; i < ST7735_INIT_SEQUENCE_LEN; i++) {
        const st7735_init_cmd_t *cmd = &st7735_init_sequence[i];
        display_cmd_data(self->display, cmd->cmd, cmd->data, cmd->data_len);
        if (cmd->delay_ms > 0) {
            mp_hal_delay_ms(cmd->delay_ms);
        }
    }

    // Create FrameBuffer
    mp_obj_t framebuf_class = get_framebuf_class();
    mp_obj_t rgb565_format = get_rgb565_format();

    mp_obj_t framebuf_args[4] = {
        self->framebuffer_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        rgb565_format,
    };

    self->framebuf_obj = mp_call_function_n_kw(framebuf_class,
        MP_ARRAY_SIZE(framebuf_args), 0, framebuf_args);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t st7735_show(mp_obj_t self_in) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);
    display_show(self->display, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(st7735_show_obj, st7735_show);

static mp_obj_t st7735_update_rect(size_t n_args, const mp_obj_t *args) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    uint16_t w = mp_obj_get_int(args[3]);
    uint16_t h = mp_obj_get_int(args[4]);

    // Calculate buffer offset for rectangle
    size_t offset = ((size_t)y * self->width + x) * 2;
    size_t rect_size = (size_t)w * h * 2;

    // Create temporary buffer for the rectangle
    uint8_t *temp_buffer = m_new(uint8_t, rect_size);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);

    // Copy rectangle data to temp buffer (row by row)
    uint8_t *src = (uint8_t *)bufinfo.buf + offset;
    uint8_t *dst = temp_buffer;
    for (uint16_t row = 0; row < h; row++) {
        memcpy(dst, src, w * 2);
        src += self->width * 2;
        dst += w * 2;
    }

    display_update_rect(self->display, x, y, w, h, temp_buffer, rect_size);
    m_del(uint8_t, temp_buffer, rect_size);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(st7735_update_rect_obj, 5, 5, st7735_update_rect);

static mp_obj_t st7735_set_backlight(mp_obj_t self_in, mp_obj_t percent_in) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t percent = mp_obj_get_int(percent_in);
    display_set_backlight(self->display, percent);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(st7735_set_backlight_obj, st7735_set_backlight);

static void st7735_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    st7735_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] == MP_OBJ_NULL) {
        // Load attribute
        if (attr == MP_QSTR_display) {
            dest[0] = self->framebuf_obj;
        } else if (attr == MP_QSTR_buffer) {
            dest[0] = self->framebuffer_obj;
        } else {
            // Look in locals dict
            mp_obj_t member = mp_map_lookup(&st7735_locals_dict.map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
            if (member != NULL) {
                dest[0] = member;
            }
        }
    } else {
        // Store attribute
        mp_raise_AttributeError(MP_ERROR_TEXT("can't set attribute"));
    }
}

static const mp_rom_map_elem_t st7735_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&st7735_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&st7735_update_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&st7735_set_backlight_obj) },
};
static MP_DEFINE_CONST_DICT(st7735_locals_dict, st7735_locals_dict_table);

static const mp_obj_type_t st7735_type = {
    { &mp_type_type },
    .name = MP_QSTR_ST7735,
    .make_new = st7735_make_new,
    .attr = st7735_attr,
    .locals_dict = (mp_obj_dict_t *)&st7735_locals_dict,
};

// ST7789 implementation
typedef struct {
    mp_obj_base_t base;
    display_obj_t *display;
    mp_obj_t framebuffer_obj;
    mp_obj_t framebuf_obj;
    uint16_t width;
    uint16_t height;
} st7789_obj_t;

static mp_obj_t st7789_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_spi, ARG_width, ARG_height, ARG_buffer, ARG_dc, ARG_cs, ARG_rst, ARG_bl,
        ARG_backlight_active_high, ARG_use_dma, ARG_dma_threshold
    };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_cs, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_rst, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_bl, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_use_dma, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_dma_threshold, MP_ARG_INT, {.u_int = 64} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    st7789_obj_t *self = m_new_obj(st7789_obj_t);
    self->base.type = type;
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;

    size_t buffer_size = self->width * self->height * 2;
    if (args[ARG_buffer].u_obj != mp_const_none) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_buffer].u_obj, &bufinfo, MP_BUFFER_RW);
        if (bufinfo.len < buffer_size) {
            mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
        }
        self->framebuffer_obj = args[ARG_buffer].u_obj;
    } else {
        self->framebuffer_obj = mp_obj_new_bytearray(buffer_size, NULL);
    }

    mp_obj_t display_args[10] = {
        args[ARG_spi].u_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        mp_obj_new_int(args[ARG_dc].u_int),
        mp_obj_new_int(args[ARG_cs].u_int),
        mp_obj_new_int(args[ARG_rst].u_int),
        mp_obj_new_int(args[ARG_bl].u_int),
        mp_obj_new_bool(args[ARG_backlight_active_high].u_bool),
        mp_obj_new_bool(args[ARG_use_dma].u_bool),
        mp_obj_new_int(args[ARG_dma_threshold].u_int),
    };

    mp_obj_t display_obj = display_make_new(&display_type,
        MP_ARRAY_SIZE(display_args), 0, display_args);
    self->display = MP_OBJ_TO_PTR(display_obj);

    // Send ST7789 initialization
    for (size_t i = 0; i < ST7789_INIT_SEQUENCE_LEN; i++) {
        const st7789_init_cmd_t *cmd = &st7789_init_sequence[i];
        display_cmd_data(self->display, cmd->cmd, cmd->data, cmd->data_len);
        if (cmd->delay_ms > 0) {
            mp_hal_delay_ms(cmd->delay_ms);
        }
    }

    mp_obj_t framebuf_class = get_framebuf_class();
    mp_obj_t rgb565_format = get_rgb565_format();

    mp_obj_t framebuf_args[4] = {
        self->framebuffer_obj,
        mp_obj_new_int(self->width),
        mp_obj_new_int(self->height),
        rgb565_format,
    };

    self->framebuf_obj = mp_call_function_n_kw(framebuf_class,
        MP_ARRAY_SIZE(framebuf_args), 0, framebuf_args);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t st7789_show(mp_obj_t self_in) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);
    display_show(self->display, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(st7789_show_obj, st7789_show);

static mp_obj_t st7789_update_rect(size_t n_args, const mp_obj_t *args) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    uint16_t w = mp_obj_get_int(args[3]);
    uint16_t h = mp_obj_get_int(args[4]);

    size_t offset = ((size_t)y * self->width + x) * 2;
    size_t rect_size = (size_t)w * h * 2;

    uint8_t *temp_buffer = m_new(uint8_t, rect_size);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(self->framebuffer_obj, &bufinfo, MP_BUFFER_READ);

    uint8_t *src = (uint8_t *)bufinfo.buf + offset;
    uint8_t *dst = temp_buffer;
    for (uint16_t row = 0; row < h; row++) {
        memcpy(dst, src, w * 2);
        src += self->width * 2;
        dst += w * 2;
    }

    display_update_rect(self->display, x, y, w, h, temp_buffer, rect_size);
    m_del(uint8_t, temp_buffer, rect_size);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(st7789_update_rect_obj, 5, 5, st7789_update_rect);

static mp_obj_t st7789_set_backlight(mp_obj_t self_in, mp_obj_t percent_in) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t percent = mp_obj_get_int(percent_in);
    display_set_backlight(self->display, percent);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(st7789_set_backlight_obj, st7789_set_backlight);

static void st7789_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    st7789_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_display) {
            dest[0] = self->framebuf_obj;
        } else if (attr == MP_QSTR_buffer) {
            dest[0] = self->framebuffer_obj;
        } else {
            mp_obj_t member = mp_map_lookup(&st7789_locals_dict.map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
            if (member != NULL) {
                dest[0] = member;
            }
        }
    } else {
        mp_raise_AttributeError(MP_ERROR_TEXT("can't set attribute"));
    }
}

static const mp_rom_map_elem_t st7789_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&st7789_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&st7789_update_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&st7789_set_backlight_obj) },
};
static MP_DEFINE_CONST_DICT(st7789_locals_dict, st7789_locals_dict_table);

static const mp_obj_type_t st7789_type = {
    { &mp_type_type },
    .name = MP_QSTR_ST7789,
    .make_new = st7789_make_new,
    .attr = st7789_attr,
    .locals_dict = (mp_obj_dict_t *)&st7789_locals_dict,
};

// Module definition
static const mp_rom_map_elem_t spi_displays_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spi_displays) },
    { MP_ROM_QSTR(MP_QSTR_Display), MP_ROM_PTR(&display_type) },
    { MP_ROM_QSTR(MP_QSTR_ST7735), MP_ROM_PTR(&st7735_type) },
    { MP_ROM_QSTR(MP_QSTR_ST7789), MP_ROM_PTR(&st7789_type) },
};
static MP_DEFINE_CONST_DICT(spi_displays_globals, spi_displays_globals_table);

const mp_obj_module_t spi_displays_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&spi_displays_globals,
};

MP_REGISTER_MODULE(MP_QSTR_spi_displays, spi_displays_module);