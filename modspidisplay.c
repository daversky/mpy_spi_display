// spi_displays/modspidisplay.c
#include "py/runtime.h"
#include "py/obj.h"
#include "display.h"

extern const mp_obj_type_t mp_type_st7735;
extern const mp_obj_type_t mp_type_st7789;

static mp_obj_t display_color_rgb(mp_obj_t r_obj, mp_obj_t g_obj, mp_obj_t b_obj) {
    int r = mp_obj_get_int(r_obj);
    int g = mp_obj_get_int(g_obj);
    int b = mp_obj_get_int(b_obj);
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        mp_raise_ValueError(MP_ERROR_TEXT("RGB values must be 0-255"));
    }
    uint16_t color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    return mp_obj_new_int(color);
}

MP_DEFINE_CONST_FUN_OBJ_3(display_color_rgb_obj, display_color_rgb);

static const mp_rom_map_elem_t spi_displays_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spi_displays)},
    {MP_ROM_QSTR(MP_QSTR_Display), MP_ROM_PTR(&mp_type_display)},
    {MP_ROM_QSTR(MP_QSTR_st7735), MP_ROM_PTR(&mp_type_st7735)},
    {MP_ROM_QSTR(MP_QSTR_st7789), MP_ROM_PTR(&mp_type_st7789)},
    {MP_ROM_QSTR(MP_QSTR_color_rgb), MP_ROM_PTR(&display_color_rgb_obj)},
};
static MP_DEFINE_CONST_DICT(spi_displays_globals, spi_displays_globals_table);

const mp_obj_module_t spi_displays_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t*)&spi_displays_globals,
};

MP_REGISTER_MODULE(MP_QSTR_spi_displays, spi_displays_module);