/*
 * MicroPython ST7735 Driver
 *
 * Core logic & initial setup: DeepSeek AI
 * Architecture fix, Cross-platform & GFX Fonts: Google Gemini
 * Integration & Hardware testing: [daversky]
 */
// spi_displays/modspidisplay.c
#include "py/runtime.h"
#include "py/obj.h"
#include "display.h"

extern const mp_obj_type_t mp_type_st7735;

static const mp_rom_map_elem_t spi_displays_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spi_displays) },
    { MP_ROM_QSTR(MP_QSTR_Display), MP_ROM_PTR(&mp_type_display) },
    { MP_ROM_QSTR(MP_QSTR_ST7735), MP_ROM_PTR(&mp_type_st7735) },
};
static MP_DEFINE_CONST_DICT(spi_displays_globals, spi_displays_globals_table);

const mp_obj_module_t spi_displays_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&spi_displays_globals,
};

MP_REGISTER_MODULE(MP_QSTR_spi_displays, spi_displays_module);